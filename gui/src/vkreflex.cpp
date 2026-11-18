#include "vkreflex.h"

#include "chiaki/time.h"

#include <QLoggingCategory>

#include <atomic>
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
PFN_vkGetDeviceProcAddr real_gdpa = nullptr;
PFN_vkCreateSwapchainKHR real_create_swapchain = nullptr;
PFN_vkDestroySwapchainKHR real_destroy_swapchain = nullptr;
PFN_vkQueuePresentKHR real_queue_present = nullptr;
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
std::atomic<uint64_t> last_present_id{0};
std::atomic<uint64_t> marked_frame_id{0};

// Current swapchain; markers and waits only use it while it is alive.
std::mutex swapchain_mutex;
VkSwapchainKHR swapchain = VK_NULL_HANDLE;

// Present wait thread.
struct PendingPresent {
    VkSwapchainKHR swapchain;
    uint64_t id;
    qint64 present_us;
};
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
qint64 refreshes[6] = {}; // frames shown for 1..5, 6+ refreshes
qint64 timeouts = 0;

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
    if (!low_latency.load(std::memory_order_relaxed))
        return;
    std::lock_guard<std::mutex> lock(swapchain_mutex);
    markerFor(swapchain, id, marker);
}

void recordDisplay(const PendingPresent &p, qint64 display_us)
{
    std::lock_guard<std::mutex> lock(stats_mutex);
    if (last_display_id && p.id == last_display_id + 1 && display_us > last_display_us) {
        const qint64 interval = display_us - last_display_us;
        interval_sum_us += interval;
        interval_max_us = qMax(interval_max_us, interval);
        interval_count++;
        if (refresh_period_us > 0.0) {
            const int n = qBound(1, static_cast<int>(interval / refresh_period_us + 0.5), 6);
            refreshes[n - 1]++;
        }
    }
    const qint64 latency = display_us - p.present_us;
    if (latency >= 0) {
        latency_sum_us += latency;
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

VKAPI_ATTR VkResult VKAPI_CALL wrapCreateSwapchain(VkDevice dev, const VkSwapchainCreateInfoKHR *info,
                                                   const VkAllocationCallbacks *alloc, VkSwapchainKHR *out)
{
    VkResult res;
    bool latency_mode = false;
    if (low_latency.load()) {
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
    }
    {
        std::lock_guard<std::mutex> lock(stats_mutex);
        last_display_id = 0;
    }
    return res;
}

VKAPI_ATTR void VKAPI_CALL wrapDestroySwapchain(VkDevice dev, VkSwapchainKHR sc, const VkAllocationCallbacks *alloc)
{
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
    real_destroy_swapchain(dev, sc, alloc);
}

VKAPI_ATTR VkResult VKAPI_CALL wrapQueuePresent(VkQueue q, const VkPresentInfoKHR *info)
{
    if (!present_ids.load() || info->swapchainCount == 0)
        return real_queue_present(q, info);
    const uint64_t id = last_present_id.fetch_add(1) + 1;
    std::vector<uint64_t> ids(info->swapchainCount, id);
    VkPresentIdKHR pid = {};
    pid.sType = VK_STRUCTURE_TYPE_PRESENT_ID_KHR;
    pid.pNext = info->pNext;
    pid.swapchainCount = info->swapchainCount;
    pid.pPresentIds = ids.data();
    VkPresentInfoKHR pi = *info;
    pi.pNext = &pid;
    const VkSwapchainKHR sc = info->pSwapchains[0];
    const bool ll = low_latency.load(std::memory_order_relaxed);
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
        queue.push_back({sc, id, present_us});
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
    WRAP("vkQueueSubmit", real_queue_submit, wrapQueueSubmit)
    WRAP("vkQueueSubmit2", real_queue_submit2, wrapQueueSubmit2)
    WRAP("vkQueueSubmit2KHR", real_queue_submit2_khr, wrapQueueSubmit2Khr)
#undef WRAP
    return real;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL wrapGetInstanceProcAddr(VkInstance inst, const char *name)
{
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
    if (present_wait.load() && !waiter.joinable()) {
        waiter_stop = false;
        waiter = std::thread(waiterLoop);
    }
    qCInfo(chiakiGui).nospace() << "[reflex] NVIDIA Reflex (VK_NV_low_latency2) "
                                << (low_latency.load() ? "on" : "off") << ", present ids "
                                << (present_ids.load() ? "on" : "off") << ", present wait "
                                << (present_wait.load() ? "on" : "off");
}

void onDeviceDestroyed()
{
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
        .arg(timeouts);
    interval_sum_us = interval_max_us = interval_count = 0;
    latency_sum_us = latency_max_us = latency_count = 0;
    for (auto &r : refreshes)
        r = 0;
    timeouts = 0;
    return s;
}

bool lowLatencyActive() { return low_latency.load(); }
bool presentWaitActive() { return present_wait.load(); }

#else // !CHIAKI_HAVE_VK_REFLEX

PFN_vkGetInstanceProcAddr wrapInstanceProcAddr(PFN_vkGetInstanceProcAddr real) { return real; }
void appendDeviceExtensions(std::vector<const char *> &) {}
const VkPhysicalDeviceFeatures2 *deviceFeatures() { return nullptr; }
void onDeviceCreated(pl_vulkan) {}
void onDeviceDestroyed() {}
void markFrameStart() {}
void markRenderSubmitted() {}
void setRefreshRate(double) {}
bool lastDisplayTime(qint64 *, qint64) { return false; }
QString takeDisplayStats() { return QString(); }
bool lowLatencyActive() { return false; }
bool presentWaitActive() { return false; }

#endif

} // namespace VkReflex
