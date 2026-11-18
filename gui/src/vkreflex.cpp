#include "vkreflex.h"

#include "chiaki/time.h"

#include <QLoggingCategory>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

Q_DECLARE_LOGGING_CATEGORY(chiakiGui);

#if defined(VK_NV_low_latency2) && defined(VK_KHR_present_wait) && defined(VK_KHR_present_id)
#define CHIAKI_HAVE_VK_REFLEX 1
#endif

namespace VkReflex {

#ifdef CHIAKI_HAVE_VK_REFLEX

namespace {

bool envOff(const char *name)
{
    bool ok = false;
    const int v = qEnvironmentVariableIntValue(name, &ok);
    return ok && v == 0;
}

qint64 nowUs() { return static_cast<qint64>(chiaki_time_now_monotonic_us()); }

PFN_vkGetInstanceProcAddr real_gipa = nullptr;
PFN_vkCreateDevice real_create_device = nullptr;
VkInstance wrapped_instance = VK_NULL_HANDLE;
bool gpu_priority_high = false;
PFN_vkGetDeviceProcAddr real_gdpa = nullptr;
PFN_vkCreateSwapchainKHR real_create_swapchain = nullptr;
PFN_vkDestroySwapchainKHR real_destroy_swapchain = nullptr;
PFN_vkQueuePresentKHR real_queue_present = nullptr;
PFN_vkAcquireNextImageKHR real_acquire = nullptr;
// Host access to a swapchain must be externally synchronized: acquires
// (render thread), presents (present thread), create and destroy take it.
std::mutex swapchain_sync;
PFN_vkQueueSubmit real_queue_submit = nullptr;
PFN_vkQueueSubmit2 real_queue_submit2 = nullptr;
PFN_vkQueueSubmit2 real_queue_submit2_khr = nullptr;
PFN_vkSetLatencySleepModeNV set_sleep_mode = nullptr;
PFN_vkSetLatencyMarkerNV set_marker = nullptr;
PFN_vkWaitForPresentKHR wait_for_present = nullptr;

VkDevice device = VK_NULL_HANDLE;
std::atomic<bool> low_latency{false};
std::atomic<bool> present_ids{false};
std::atomic<bool> present_wait{false};
std::atomic<bool> low_latency_wanted{true};
// The current swapchain was created with the low latency mode.
std::atomic<bool> swapchain_latency{false};
std::atomic<uint64_t> last_present_id{0};
std::atomic<uint64_t> marked_frame_id{0};
std::atomic<qint64> present_target_us{0};
std::atomic<void (*)(qint64, qint64, qint64)> present_done{nullptr};

// Current swapchain; markers and waits only use it while it is alive.
std::mutex swapchain_mutex;
VkSwapchainKHR swapchain = VK_NULL_HANDLE;

// Present wait thread.
struct PendingPresent {
    VkSwapchainKHR swapchain;
    uint64_t id;
    qint64 present_us;
    int kind;
};
std::atomic<int> next_kind{0};
std::mutex wait_mutex; // held while waiting on the swapchain
std::mutex queue_mutex;
std::condition_variable queue_cv;
std::deque<PendingPresent> queue;
std::thread waiter;
bool waiter_stop = false;

// Display timing (written by the waiter thread).
std::mutex stats_mutex;
double refresh_period_us = 0.0;
qint64 last_display_us = 0;
uint64_t last_display_id = 0;
qint64 interval_sum_us = 0, interval_max_us = 0, interval_count = 0;
qint64 latency_sum_us = 0, latency_max_us = 0, latency_count = 0;
// Present -> on screen by kind (generated, real).
qint64 kind_sum_us[3] = {}, kind_max_us[3] = {}, kind_count[3] = {};
qint64 refreshes[6] = {}; // frames shown for 1..5, 6+ refreshes
qint64 timeouts = 0;
// The last frames on screen for the stream overlay (not reset by
// takeDisplayStats): refreshes each one stayed, present -> on screen.
constexpr int pace_ring_size = 120;
int pace_refreshes[pace_ring_size] = {};
qint64 pace_latency_us[pace_ring_size] = {};
int pace_head = 0, pace_count = 0;

void markerFor(VkSwapchainKHR sc, uint64_t id, VkLatencyMarkerNV marker)
{
    if (!set_marker || sc == VK_NULL_HANDLE || id == 0)
        return;
    VkSetLatencyMarkerInfoNV info = {};
    info.sType = VK_STRUCTURE_TYPE_SET_LATENCY_MARKER_INFO_NV;
    info.presentID = id;
    info.marker = marker;
    set_marker(device, sc, &info);
}

void markCurrent(VkLatencyMarkerNV marker, uint64_t id)
{
    if (!low_latency.load(std::memory_order_relaxed) || !swapchain_latency.load(std::memory_order_relaxed))
        return;
    std::lock_guard<std::mutex> lock(swapchain_mutex);
    markerFor(swapchain, id, marker);
}

void recordDisplay(const PendingPresent &p, qint64 display_us)
{
    // CHIAKI_PRESENT_TRACE=<n>: logs n presents (from the 600th on).
    static const int trace_n = qEnvironmentVariableIntValue("CHIAKI_PRESENT_TRACE");
    static int traced = 0, seen = 0;
    if (trace_n > 0 && ++seen > 600 && traced < trace_n) {
        traced++;
        qCInfo(chiakiGui).nospace() << "[present-trace] id " << p.id << " kind " << p.kind << " present "
                                    << p.present_us << " on screen " << display_us << " (+"
                                    << (display_us - p.present_us) / 1000.0 << " ms)";
    }
    std::lock_guard<std::mutex> lock(stats_mutex);
    if (last_display_id && p.id == last_display_id + 1 && display_us > last_display_us) {
        const qint64 interval = display_us - last_display_us;
        interval_sum_us += interval;
        interval_max_us = qMax(interval_max_us, interval);
        interval_count++;
        if (refresh_period_us > 0.0) {
            const int n = qBound(1, static_cast<int>(interval / refresh_period_us + 0.5), 6);
            refreshes[n - 1]++;
            pace_refreshes[pace_head] = n;
            pace_latency_us[pace_head] = qMax<qint64>(0, display_us - p.present_us);
            pace_head = (pace_head + 1) % pace_ring_size;
            pace_count = qMin(pace_count + 1, pace_ring_size);
        }
    }
    const qint64 latency = display_us - p.present_us;
    if (latency >= 0) {
        latency_sum_us += latency;
        if (p.kind >= 1 && p.kind <= 2) {
            kind_sum_us[p.kind] += latency;
            kind_max_us[p.kind] = qMax(kind_max_us[p.kind], latency);
            kind_count[p.kind]++;
        }
        latency_max_us = qMax(latency_max_us, latency);
        latency_count++;
    }
    last_display_us = display_us;
    last_display_id = p.id;
}

void waiterLoop()
{
    for (;;) {
        PendingPresent p;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [] { return waiter_stop || !queue.empty(); });
            if (waiter_stop)
                return;
            p = queue.front();
            queue.pop_front();
        }
        // Short waits so destroying the swapchain is never held up long.
        const qint64 give_up_us = nowUs() + 250000;
        for (;;) {
            VkResult res;
            {
                std::lock_guard<std::mutex> wait_lock(wait_mutex);
                {
                    std::lock_guard<std::mutex> lock(swapchain_mutex);
                    if (swapchain != p.swapchain)
                        break;
                }
                res = wait_for_present(device, p.swapchain, p.id, 5000000ull);
            }
            if (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR) {
                recordDisplay(p, nowUs());
                break;
            }
            if (res != VK_TIMEOUT || nowUs() > give_up_us) {
                std::lock_guard<std::mutex> lock(stats_mutex);
                timeouts++;
                break;
            }
        }
    }
}

void flushPresents(VkSwapchainKHR sc);
uint32_t moreImages(const VkSwapchainCreateInfoKHR *info);

VKAPI_ATTR VkResult VKAPI_CALL wrapCreateSwapchain(VkDevice dev, const VkSwapchainCreateInfoKHR *orig_info,
                                                   const VkAllocationCallbacks *alloc, VkSwapchainKHR *out)
{
    if (orig_info->oldSwapchain != VK_NULL_HANDLE)
        flushPresents(orig_info->oldSwapchain);
    std::lock_guard<std::mutex> sync(swapchain_sync);
    // One image more than libplacebo asks for: presents waiting on the
    // present thread still hold theirs, and the next acquire should not wait.
    VkSwapchainCreateInfoKHR base = *orig_info;
    base.minImageCount = moreImages(orig_info);
    const VkSwapchainCreateInfoKHR *info = &base;
    VkResult res;
    bool latency_mode = false;
    if (low_latency.load() && low_latency_wanted.load()) {
        VkSwapchainCreateInfoKHR ci = *info;
        VkSwapchainLatencyCreateInfoNV lci = {};
        lci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_LATENCY_CREATE_INFO_NV;
        lci.pNext = ci.pNext;
        lci.latencyModeEnable = VK_TRUE;
        ci.pNext = &lci;
        res = real_create_swapchain(dev, &ci, alloc, out);
        latency_mode = res == VK_SUCCESS;
        if (!latency_mode)
            res = real_create_swapchain(dev, info, alloc, out);
    } else {
        res = real_create_swapchain(dev, info, alloc, out);
    }
    if (res != VK_SUCCESS)
        return res;
    if (low_latency.load() && !latency_mode)
        qCInfo(chiakiGui) << "[reflex] Low latency mode off for the swapchain";
    if (latency_mode && set_sleep_mode) {
        VkLatencySleepModeInfoNV mode = {};
        mode.sType = VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV;
        mode.lowLatencyMode = VK_TRUE;
        mode.lowLatencyBoost = VK_FALSE;
        mode.minimumIntervalUs = 0;
        const VkResult mres = set_sleep_mode(dev, *out, &mode);
        qCInfo(chiakiGui).nospace() << "[reflex] Low latency mode " << (mres == VK_SUCCESS ? "on" : "failed")
                                    << " for the swapchain (VkResult " << mres << ")";
    }
    {
        std::lock_guard<std::mutex> lock(swapchain_mutex);
        swapchain = *out;
        swapchain_latency.store(latency_mode);
    }
    {
        std::lock_guard<std::mutex> lock(stats_mutex);
        last_display_id = 0;
    }
    return res;
}

VKAPI_ATTR void VKAPI_CALL wrapDestroySwapchain(VkDevice dev, VkSwapchainKHR sc, const VkAllocationCallbacks *alloc)
{
    // Presents of it still waiting on the present thread go out first.
    flushPresents(sc);
    {
        // Waits for a present wait on it to return.
        std::lock_guard<std::mutex> wait_lock(wait_mutex);
        std::lock_guard<std::mutex> lock(swapchain_mutex);
        if (swapchain == sc)
            swapchain = VK_NULL_HANDLE;
    }
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        for (auto it = queue.begin(); it != queue.end();)
            it = it->swapchain == sc ? queue.erase(it) : it + 1;
    }
    std::lock_guard<std::mutex> sync(swapchain_sync);
    real_destroy_swapchain(dev, sc, alloc);
}

// Acquire without blocking while holding the sync lock (a blocked acquire
// would wait for an image only a queued present frees: that deadlocked,
// the window flickered and hung leaving fullscreen).
VKAPI_ATTR VkResult VKAPI_CALL wrapAcquireNextImage(VkDevice dev, VkSwapchainKHR sc, uint64_t timeout,
                                                    VkSemaphore sem, VkFence fence, uint32_t *idx)
{
    const qint64 begin = nowUs();
    for (;;) {
        VkResult res;
        {
            std::lock_guard<std::mutex> sync(swapchain_sync);
            res = real_acquire(dev, sc, 0, sem, fence, idx);
        }
        if (res != VK_NOT_READY && res != VK_TIMEOUT)
            return res;
        if (timeout == 0)
            return res;
        if (timeout != UINT64_MAX && static_cast<uint64_t>(nowUs() - begin) * 1000ull >= timeout)
            return VK_TIMEOUT;
        std::this_thread::sleep_for(std::chrono::microseconds(250));
    }
}

// Sleeps until `target` (bounded, in case it is stale); returns the wait.
qint64 waitForTarget(qint64 target)
{
    const qint64 begin = nowUs();
    const qint64 until = qMin(target, begin + 50000);
    for (;;) {
        const qint64 remaining = until - nowUs();
        if (remaining <= 0)
            break;
        if (remaining > 1500)
            std::this_thread::sleep_for(std::chrono::microseconds(remaining - 1000));
        else
            std::this_thread::yield();
    }
    return nowUs() - begin;
}

VkResult doPresent(VkQueue q, const VkPresentInfoKHR *info, uint64_t id, int kind);

// Present thread: scheduled presents wait for their time here, so the
// render thread goes on with the next stream frame meanwhile (waiting there
// started the next frame's VSR / VFG late, and its generated frame then
// missed its vblank: frames stayed 2 and 4 refreshes instead of 3).
struct PresentJob {
    VkQueue queue;
    uint32_t qidx;
    qint64 target;
    uint64_t id;
    int kind;
    std::vector<VkSemaphore> waits;
    VkSwapchainKHR swapchain;
    uint32_t image;
    bool has_fence;
    VkFence fence;
};
pl_vulkan placebo_vk = nullptr;
// Graphics queue the present thread presents on (the family's last one).
VkQueue present_queue = VK_NULL_HANDLE;
uint32_t present_queue_idx = 0;
std::mutex present_mutex;
std::condition_variable present_cv;
std::deque<PresentJob> present_jobs;
VkSwapchainKHR present_busy = VK_NULL_HANDLE;
std::thread presenter;
bool presenter_stop = false;
std::atomic<int> async_error{VK_SUCCESS};

void runJob(PresentJob &job)
{
    qint64 waited = 0;
    if (job.target > 0)
        waited = waitForTarget(job.target);
    VkSwapchainPresentFenceInfoEXT fence_info = {};
    VkPresentInfoKHR pi = {};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = static_cast<uint32_t>(job.waits.size());
    pi.pWaitSemaphores = job.waits.data();
    pi.swapchainCount = 1;
    pi.pSwapchains = &job.swapchain;
    pi.pImageIndices = &job.image;
    if (job.has_fence) {
        fence_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
        fence_info.swapchainCount = 1;
        fence_info.pFences = &job.fence;
        pi.pNext = &fence_info;
    }
    const uint32_t qf = placebo_vk->queue_graphics.index;
    // A queue of its own: on the renderer's queue the present waited behind
    // work submitted after the frame (the next stream frame's generated frame
    // waits there for VFG), so real frames reached the screen ~11 ms late.
    VkQueue queue = job.queue;
    uint32_t qidx = job.qidx;
    if (present_queue != VK_NULL_HANDLE) {
        queue = present_queue;
        qidx = present_queue_idx;
    }
    placebo_vk->lock_queue(placebo_vk, qf, qidx);
    VkResult res;
    {
        std::lock_guard<std::mutex> sync(swapchain_sync);
        res = doPresent(queue, &pi, job.id, job.kind);
    }
    placebo_vk->unlock_queue(placebo_vk, qf, qidx);
    if (job.target > 0) {
        if (auto cb = present_done.load())
            cb(job.target, nowUs(), waited);
    }
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_ERROR_SURFACE_LOST_KHR)
        async_error.store(res);
    else if (res != VK_SUCCESS) {
        int expected = VK_SUCCESS;
        async_error.compare_exchange_strong(expected, res);
    }
}

void presenterLoop()
{
    std::unique_lock<std::mutex> lock(present_mutex);
    for (;;) {
        present_cv.wait(lock, [] { return presenter_stop || !present_jobs.empty(); });
        if (presenter_stop)
            return;
        PresentJob job = std::move(present_jobs.front());
        present_jobs.pop_front();
        present_busy = job.swapchain;
        lock.unlock();
        runJob(job);
        lock.lock();
        present_busy = VK_NULL_HANDLE;
        present_cv.notify_all();
    }
}

void flushPresents(VkSwapchainKHR sc)
{
    if (!presenter.joinable())
        return;
    std::unique_lock<std::mutex> lock(present_mutex);
    present_cv.wait(lock, [sc] {
        if (present_busy == sc)
            return false;
        for (const auto &j : present_jobs) {
            if (j.swapchain == sc)
                return false;
        }
        return true;
    });
}

// Index of `q` in libplacebo's graphics queue family, or -1.
int graphicsQueueIndex(VkQueue q)
{
    static PFN_vkGetDeviceQueue get_queue = nullptr;
    if (!placebo_vk)
        return -1;
    if (!get_queue)
        get_queue = reinterpret_cast<PFN_vkGetDeviceQueue>(real_gdpa(device, "vkGetDeviceQueue"));
    if (!get_queue)
        return -1;
    for (uint32_t i = 0; i < placebo_vk->queue_graphics.count; i++) {
        VkQueue gq = VK_NULL_HANDLE;
        get_queue(device, placebo_vk->queue_graphics.index, i, &gq);
        if (gq == q)
            return static_cast<int>(i);
    }
    return -1;
}

uint32_t moreImages(const VkSwapchainCreateInfoKHR *info)
{
    if (!presenter.joinable() || !placebo_vk || !real_gipa)
        return info->minImageCount;
    static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR get_caps = nullptr;
    if (!get_caps)
        get_caps = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
            real_gipa(placebo_vk->instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    VkSurfaceCapabilitiesKHR caps = {};
    if (!get_caps || get_caps(placebo_vk->phys_device, info->surface, &caps) != VK_SUCCESS)
        return info->minImageCount;
    uint32_t n = info->minImageCount + 1;
    if (caps.maxImageCount > 0)
        n = qMin(n, caps.maxImageCount);
    return n;
}

// What the present thread can take: one swapchain, no results array, and
// at most libplacebo's present fence in the chain.
bool queueable(const VkPresentInfoKHR *info, bool *has_fence, VkFence *fence)
{
    *has_fence = false;
    if (info->swapchainCount != 1 || info->pResults)
        return false;
    for (auto *s = reinterpret_cast<const VkBaseInStructure *>(info->pNext); s; s = s->pNext) {
        if (s->sType != VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT)
            return false;
        auto *f = reinterpret_cast<const VkSwapchainPresentFenceInfoEXT *>(s);
        if (f->swapchainCount != 1 || !f->pFences)
            return false;
        *has_fence = true;
        *fence = f->pFences[0];
    }
    return true;
}

VKAPI_ATTR VkResult VKAPI_CALL wrapQueuePresent(VkQueue q, const VkPresentInfoKHR *info)
{
    const qint64 target = present_target_us.exchange(0);
    const int kind = next_kind.exchange(0);
    const uint64_t id = present_ids.load() && info->swapchainCount > 0 ? last_present_id.fetch_add(1) + 1 : 0;
    bool has_fence = false;
    VkFence fence = VK_NULL_HANDLE;
    int qidx = -1;
    if (presenter.joinable() && queueable(info, &has_fence, &fence) && (qidx = graphicsQueueIndex(q)) >= 0) {
        PresentJob job;
        job.queue = q;
        job.qidx = static_cast<uint32_t>(qidx);
        job.target = target;
        job.id = id;
        job.kind = kind;
        job.waits.assign(info->pWaitSemaphores, info->pWaitSemaphores + info->waitSemaphoreCount);
        job.swapchain = info->pSwapchains[0];
        job.image = info->pImageIndices[0];
        job.has_fence = has_fence;
        job.fence = fence;
        {
            std::lock_guard<std::mutex> lock(present_mutex);
            present_jobs.push_back(std::move(job));
        }
        present_cv.notify_all();
        // A failure of an earlier present (e.g. out of date) is reported now.
        return static_cast<VkResult>(async_error.exchange(VK_SUCCESS));
    }
    // Inline: the rendering is already on the GPU; only the flip waits.
    if (target > 0) {
        const qint64 waited = waitForTarget(target);
        if (auto cb = present_done.load())
            cb(target, nowUs(), waited);
    }
    return doPresent(q, info, id, kind);
}

VkResult doPresent(VkQueue q, const VkPresentInfoKHR *info, uint64_t id, int kind)
{
    if (id == 0)
        return real_queue_present(q, info);
    std::vector<uint64_t> ids(info->swapchainCount, id);
    VkPresentIdKHR pid = {};
    pid.sType = VK_STRUCTURE_TYPE_PRESENT_ID_KHR;
    pid.pNext = info->pNext;
    pid.swapchainCount = info->swapchainCount;
    pid.pPresentIds = ids.data();
    VkPresentInfoKHR pi = *info;
    pi.pNext = &pid;
    const VkSwapchainKHR sc = info->pSwapchains[0];
    const bool ll = low_latency.load(std::memory_order_relaxed) && swapchain_latency.load(std::memory_order_relaxed);
    if (ll)
        markerFor(sc, id, VK_LATENCY_MARKER_PRESENT_START_NV);
    const qint64 present_us = nowUs();
    const VkResult res = real_queue_present(q, &pi);
    if (ll)
        markerFor(sc, id, VK_LATENCY_MARKER_PRESENT_END_NV);
    if (present_wait.load(std::memory_order_relaxed) && (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR)) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (queue.size() > 8)
            queue.pop_front();
        queue.push_back({sc, id, present_us, kind});
        queue_cv.notify_one();
    }
    return res;
}

// The work submitted belongs to the frame of the next present.
VKAPI_ATTR VkResult VKAPI_CALL wrapQueueSubmit(VkQueue q, uint32_t count, const VkSubmitInfo *submits, VkFence fence)
{
    if (!low_latency.load(std::memory_order_relaxed) || count == 0)
        return real_queue_submit(q, count, submits, fence);
    const uint64_t id = last_present_id.load(std::memory_order_relaxed) + 1;
    std::vector<VkSubmitInfo> s(submits, submits + count);
    std::vector<VkLatencySubmissionPresentIdNV> tags(count);
    for (uint32_t i = 0; i < count; i++) {
        tags[i] = {};
        tags[i].sType = VK_STRUCTURE_TYPE_LATENCY_SUBMISSION_PRESENT_ID_NV;
        tags[i].pNext = s[i].pNext;
        tags[i].presentID = id;
        s[i].pNext = &tags[i];
    }
    return real_queue_submit(q, count, s.data(), fence);
}

VkResult submit2(PFN_vkQueueSubmit2 real, VkQueue q, uint32_t count, const VkSubmitInfo2 *submits, VkFence fence)
{
    if (!low_latency.load(std::memory_order_relaxed) || count == 0)
        return real(q, count, submits, fence);
    const uint64_t id = last_present_id.load(std::memory_order_relaxed) + 1;
    std::vector<VkSubmitInfo2> s(submits, submits + count);
    std::vector<VkLatencySubmissionPresentIdNV> tags(count);
    for (uint32_t i = 0; i < count; i++) {
        tags[i] = {};
        tags[i].sType = VK_STRUCTURE_TYPE_LATENCY_SUBMISSION_PRESENT_ID_NV;
        tags[i].pNext = s[i].pNext;
        tags[i].presentID = id;
        s[i].pNext = &tags[i];
    }
    return real(q, count, s.data(), fence);
}

VKAPI_ATTR VkResult VKAPI_CALL wrapQueueSubmit2(VkQueue q, uint32_t count, const VkSubmitInfo2 *submits, VkFence fence)
{
    return submit2(real_queue_submit2, q, count, submits, fence);
}

VKAPI_ATTR VkResult VKAPI_CALL wrapQueueSubmit2Khr(VkQueue q, uint32_t count, const VkSubmitInfo2 *submits, VkFence fence)
{
    return submit2(real_queue_submit2_khr, q, count, submits, fence);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL wrapGetDeviceProcAddr(VkDevice dev, const char *name)
{
    const PFN_vkVoidFunction real = real_gdpa ? real_gdpa(dev, name) : nullptr;
    if (!real || !name)
        return real;
#define WRAP(name_, slot_, wrapper_) \
    if (std::strcmp(name, name_) == 0) { \
        slot_ = reinterpret_cast<decltype(slot_)>(real); \
        return reinterpret_cast<PFN_vkVoidFunction>(wrapper_); \
    }
    WRAP("vkCreateSwapchainKHR", real_create_swapchain, wrapCreateSwapchain)
    WRAP("vkDestroySwapchainKHR", real_destroy_swapchain, wrapDestroySwapchain)
    WRAP("vkQueuePresentKHR", real_queue_present, wrapQueuePresent)
    WRAP("vkAcquireNextImageKHR", real_acquire, wrapAcquireNextImage)
    WRAP("vkQueueSubmit", real_queue_submit, wrapQueueSubmit)
    WRAP("vkQueueSubmit2", real_queue_submit2, wrapQueueSubmit2)
    WRAP("vkQueueSubmit2KHR", real_queue_submit2_khr, wrapQueueSubmit2Khr)
#undef WRAP
    return real;
}

// CHIAKI_GPU_PRIORITY=1: the graphics queues get a high global priority, so
// presents and final renders could preempt CUDA's VFG work. Opt-in: overlay
// hooks (RTSS, Overwolf, OBS-style graphics-hook64) crashed in vkCreateDevice
// with it, and it did not change when real frames reached the screen.
VKAPI_ATTR VkResult VKAPI_CALL wrapCreateDevice(VkPhysicalDevice phys, const VkDeviceCreateInfo *info,
                                                const VkAllocationCallbacks *alloc, VkDevice *out)
{
    gpu_priority_high = false;
    bool has_ext = false;
    for (uint32_t i = 0; i < info->enabledExtensionCount; i++) {
        if (std::strcmp(info->ppEnabledExtensionNames[i], "VK_KHR_global_priority") == 0 ||
            std::strcmp(info->ppEnabledExtensionNames[i], VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME) == 0)
            has_ext = true;
    }
    auto get_props = wrapped_instance
        ? reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
              real_gipa(wrapped_instance, "vkGetPhysicalDeviceQueueFamilyProperties"))
        : nullptr;
    if (has_ext && get_props && qEnvironmentVariableIntValue("CHIAKI_GPU_PRIORITY") == 1) {
        uint32_t n = 0;
        get_props(phys, &n, nullptr);
        std::vector<VkQueueFamilyProperties> props(n);
        get_props(phys, &n, props.data());
        std::vector<VkDeviceQueueCreateInfo> queues(info->pQueueCreateInfos,
                                                    info->pQueueCreateInfos + info->queueCreateInfoCount);
        std::vector<VkDeviceQueueGlobalPriorityCreateInfoEXT> prio(queues.size());
        bool any = false;
        for (size_t i = 0; i < queues.size(); i++) {
            const uint32_t fam = queues[i].queueFamilyIndex;
            if (fam >= n || !(props[fam].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                continue;
            prio[i] = {};
            prio[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT;
            prio[i].pNext = queues[i].pNext;
            prio[i].globalPriority = VK_QUEUE_GLOBAL_PRIORITY_HIGH_EXT;
            queues[i].pNext = &prio[i];
            any = true;
        }
        if (any) {
            VkDeviceCreateInfo ci = *info;
            ci.pQueueCreateInfos = queues.data();
            const VkResult res = real_create_device(phys, &ci, alloc, out);
            if (res == VK_SUCCESS) {
                gpu_priority_high = true;
                return res;
            }
            qCInfo(chiakiGui).nospace() << "[reflex] High GPU priority refused (VkResult " << res << ")";
        }
    }
    return real_create_device(phys, info, alloc, out);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL wrapGetInstanceProcAddr(VkInstance inst, const char *name)
{
    if (inst)
        wrapped_instance = inst;
    if (name && std::strcmp(name, "vkCreateDevice") == 0) {
        real_create_device = reinterpret_cast<PFN_vkCreateDevice>(real_gipa(inst, name));
        return real_create_device ? reinterpret_cast<PFN_vkVoidFunction>(wrapCreateDevice) : nullptr;
    }
    if (name && std::strcmp(name, "vkGetDeviceProcAddr") == 0) {
        real_gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>(real_gipa(inst, name));
        return real_gdpa ? reinterpret_cast<PFN_vkVoidFunction>(wrapGetDeviceProcAddr) : nullptr;
    }
    return real_gipa(inst, name);
}

VkPhysicalDevicePresentWaitFeaturesKHR present_wait_features = {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR, nullptr, VK_TRUE};
VkPhysicalDevicePresentIdFeaturesKHR present_id_features = {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR, &present_wait_features, VK_TRUE};
VkPhysicalDeviceFeatures2 features2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &present_id_features, {}};

bool featureOn(const VkPhysicalDeviceFeatures2 *chain, VkStructureType type)
{
    for (auto *s = reinterpret_cast<const VkBaseInStructure *>(chain); s; s = s->pNext) {
        if (s->sType == type)
            return *reinterpret_cast<const VkBool32 *>(s + 1) == VK_TRUE;
    }
    return false;
}

bool hasExtension(pl_vulkan vk, const char *name)
{
    for (int i = 0; i < vk->num_extensions; i++) {
        if (std::strcmp(vk->extensions[i], name) == 0)
            return true;
    }
    return false;
}

} // namespace

PFN_vkGetInstanceProcAddr wrapInstanceProcAddr(PFN_vkGetInstanceProcAddr real)
{
    if (envOff("CHIAKI_PRESENT_WAIT") || !real)
        return real;
    real_gipa = real;
    return wrapGetInstanceProcAddr;
}

void appendDeviceExtensions(std::vector<const char *> &dev_extensions)
{
    if (envOff("CHIAKI_PRESENT_WAIT"))
        return;
    dev_extensions.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
    // By name: older Vulkan headers (Linux builds) lack the KHR macros; the
    // EXT structs used with it are the same.
    dev_extensions.push_back("VK_KHR_global_priority");
    dev_extensions.push_back(VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME);
    dev_extensions.push_back(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
    if (!envOff("CHIAKI_REFLEX"))
        dev_extensions.push_back(VK_NV_LOW_LATENCY_2_EXTENSION_NAME);
}

const VkPhysicalDeviceFeatures2 *deviceFeatures()
{
    return envOff("CHIAKI_PRESENT_WAIT") ? nullptr : &features2;
}

void onDeviceCreated(pl_vulkan vk)
{
    if (!vk || !real_gdpa || envOff("CHIAKI_PRESENT_WAIT"))
        return;
    device = vk->device;
    const bool ids = hasExtension(vk, VK_KHR_PRESENT_ID_EXTENSION_NAME) &&
                     featureOn(vk->features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR);
    const bool waits = ids && hasExtension(vk, VK_KHR_PRESENT_WAIT_EXTENSION_NAME) &&
                       featureOn(vk->features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR);
    const bool latency = ids && hasExtension(vk, VK_NV_LOW_LATENCY_2_EXTENSION_NAME);
    if (waits)
        wait_for_present = reinterpret_cast<PFN_vkWaitForPresentKHR>(real_gdpa(device, "vkWaitForPresentKHR"));
    if (latency) {
        set_sleep_mode = reinterpret_cast<PFN_vkSetLatencySleepModeNV>(real_gdpa(device, "vkSetLatencySleepModeNV"));
        set_marker = reinterpret_cast<PFN_vkSetLatencyMarkerNV>(real_gdpa(device, "vkSetLatencyMarkerNV"));
    }
    present_ids.store(ids && real_queue_present);
    present_wait.store(waits && wait_for_present && real_queue_present);
    low_latency.store(latency && set_sleep_mode && set_marker && real_create_swapchain && real_queue_present);
    placebo_vk = vk;
    present_queue = VK_NULL_HANDLE;
    // libplacebo creates (and locks) every queue of the family but reports
    // only the ones it uses (1): take the family's last queue from the driver,
    // clear of libplacebo (0) and frame generation (1).
    if (!envOff("CHIAKI_PRESENT_QUEUE")) {
        auto get_queue = reinterpret_cast<PFN_vkGetDeviceQueue>(real_gdpa(device, "vkGetDeviceQueue"));
        auto get_props = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
            real_gipa(vk->instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
        uint32_t n = 0;
        if (get_props)
            get_props(vk->phys_device, &n, nullptr);
        std::vector<VkQueueFamilyProperties> props(n);
        if (n)
            get_props(vk->phys_device, &n, props.data());
        const uint32_t fam = vk->queue_graphics.index;
        if (get_queue && fam < n && props[fam].queueCount >= 3) {
            present_queue_idx = props[fam].queueCount - 1;
            get_queue(device, fam, present_queue_idx, &present_queue);
        }
    }
    if (real_queue_present && vk->lock_queue && vk->unlock_queue && !envOff("CHIAKI_ASYNC_PRESENT") &&
        !presenter.joinable()) {
        presenter_stop = false;
        async_error.store(VK_SUCCESS);
        presenter = std::thread(presenterLoop);
    }
    if (present_wait.load() && !waiter.joinable()) {
        waiter_stop = false;
        waiter = std::thread(waiterLoop);
    }
    qCInfo(chiakiGui).nospace() << "[reflex] NVIDIA Reflex (VK_NV_low_latency2) "
                                << (low_latency.load() ? "on" : "off") << ", present ids "
                                << (present_ids.load() ? "on" : "off") << ", present wait "
                                << (present_wait.load() ? "on" : "off") << ", GPU priority "
                                << (gpu_priority_high ? "high" : "normal") << ", present thread "
                                << (presenter.joinable() ? "on" : "off") << ", present queue "
                                << (present_queue != VK_NULL_HANDLE ? static_cast<int>(present_queue_idx) : -1);
}

void onDeviceDestroyed()
{
    if (presenter.joinable()) {
        {
            std::lock_guard<std::mutex> lock(present_mutex);
            presenter_stop = true;
            present_jobs.clear();
        }
        present_cv.notify_all();
        presenter.join();
    }
    placebo_vk = nullptr;
    if (waiter.joinable()) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            waiter_stop = true;
            queue.clear();
        }
        queue_cv.notify_all();
        waiter.join();
    }
    low_latency.store(false);
    present_ids.store(false);
    present_wait.store(false);
    device = VK_NULL_HANDLE;
}

bool delayNextPresent(qint64 target_us, void (*done)(qint64, qint64, qint64))
{
    if (!real_queue_present || real_gipa == nullptr || device == VK_NULL_HANDLE)
        return false;
    present_done.store(done);
    present_target_us.store(target_us);
    return true;
}

void setLowLatencyEnabled(bool enabled)
{
    low_latency_wanted.store(enabled);
}

void setNextPresentKind(int kind)
{
    next_kind.store(kind);
}

void markFrameStart()
{
    if (!low_latency.load(std::memory_order_relaxed))
        return;
    const uint64_t id = last_present_id.load(std::memory_order_relaxed) + 1;
    if (marked_frame_id.exchange(id) == id)
        return;
    markCurrent(VK_LATENCY_MARKER_SIMULATION_START_NV, id);
    markCurrent(VK_LATENCY_MARKER_SIMULATION_END_NV, id);
    markCurrent(VK_LATENCY_MARKER_RENDERSUBMIT_START_NV, id);
}

void markRenderSubmitted()
{
    if (!low_latency.load(std::memory_order_relaxed))
        return;
    markCurrent(VK_LATENCY_MARKER_RENDERSUBMIT_END_NV, last_present_id.load(std::memory_order_relaxed) + 1);
}

void setRefreshRate(double hz)
{
    if (hz <= 1.0)
        return;
    std::lock_guard<std::mutex> lock(stats_mutex);
    refresh_period_us = 1000000.0 / hz;
}

bool lastDisplayTime(qint64 *display_us, qint64 max_age_us)
{
    if (!present_wait.load(std::memory_order_relaxed))
        return false;
    std::lock_guard<std::mutex> lock(stats_mutex);
    if (last_display_us <= 0 || nowUs() - last_display_us > max_age_us)
        return false;
    *display_us = last_display_us;
    return true;
}

QString takeDisplayStats()
{
    if (!present_wait.load(std::memory_order_relaxed))
        return QString();
    std::lock_guard<std::mutex> lock(stats_mutex);
    const QString s = QStringLiteral(
        "interval %1/%2 ms, refreshes per frame 1:%3 2:%4 3:%5 4:%6 5:%7 6+:%8, present -> on screen %9/%10 ms, waits failed %11")
        .arg(interval_count ? interval_sum_us / interval_count / 1000.0 : 0.0, 0, 'f', 2)
        .arg(interval_max_us / 1000.0, 0, 'f', 1)
        .arg(refreshes[0]).arg(refreshes[1]).arg(refreshes[2]).arg(refreshes[3]).arg(refreshes[4]).arg(refreshes[5])
        .arg(latency_count ? latency_sum_us / latency_count / 1000.0 : 0.0, 0, 'f', 1)
        .arg(latency_max_us / 1000.0, 0, 'f', 1)
        .arg(timeouts)
        + QStringLiteral(", present -> on screen generated %1/%2 ms, real %3/%4 ms")
        .arg(kind_count[1] ? kind_sum_us[1] / kind_count[1] / 1000.0 : 0.0, 0, 'f', 1)
        .arg(kind_max_us[1] / 1000.0, 0, 'f', 1)
        .arg(kind_count[2] ? kind_sum_us[2] / kind_count[2] / 1000.0 : 0.0, 0, 'f', 1)
        .arg(kind_max_us[2] / 1000.0, 0, 'f', 1);
    interval_sum_us = interval_max_us = interval_count = 0;
    latency_sum_us = latency_max_us = latency_count = 0;
    for (int k = 0; k < 3; k++)
        kind_sum_us[k] = kind_max_us[k] = kind_count[k] = 0;
    for (auto &r : refreshes)
        r = 0;
    timeouts = 0;
    return s;
}

int recentPacing(int *refreshes_out, int max, double *latency_ms, double *hz)
{
    if (!present_wait.load(std::memory_order_relaxed))
        return 0;
    std::lock_guard<std::mutex> lock(stats_mutex);
    const int n = qMin(max, pace_count);
    qint64 latency_sum = 0;
    for (int i = 0; i < n; i++) {
        const int idx = (pace_head - n + i + pace_ring_size) % pace_ring_size;
        refreshes_out[i] = pace_refreshes[idx];
        latency_sum += pace_latency_us[idx];
    }
    if (latency_ms)
        *latency_ms = n ? latency_sum / static_cast<double>(n) / 1000.0 : 0.0;
    if (hz)
        *hz = refresh_period_us > 0.0 ? 1000000.0 / refresh_period_us : 0.0;
    return n;
}

bool lowLatencyActive() { return low_latency.load(); }
bool presentWaitActive() { return present_wait.load(); }

#else // !CHIAKI_HAVE_VK_REFLEX

PFN_vkGetInstanceProcAddr wrapInstanceProcAddr(PFN_vkGetInstanceProcAddr real) { return real; }
void appendDeviceExtensions(std::vector<const char *> &) {}
const VkPhysicalDeviceFeatures2 *deviceFeatures() { return nullptr; }
void onDeviceCreated(pl_vulkan) {}
void onDeviceDestroyed() {}
void setLowLatencyEnabled(bool) {}
void setNextPresentKind(int) {}
void markFrameStart() {}
void markRenderSubmitted() {}
void setRefreshRate(double) {}
bool lastDisplayTime(qint64 *, qint64) { return false; }
bool delayNextPresent(qint64, void (*)(qint64, qint64, qint64)) { return false; }
QString takeDisplayStats() { return QString(); }
int recentPacing(int *, int, double *, double *) { return 0; }
bool lowLatencyActive() { return false; }
bool presentWaitActive() { return false; }

#endif

} // namespace VkReflex
