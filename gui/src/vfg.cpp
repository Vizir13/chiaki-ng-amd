#include "vfg.h"
#include "vkguard.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStandardPaths>

Q_DECLARE_LOGGING_CATEGORY(chiakiGui)

QString VfgRunner::userRuntimeDir()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("vfx"));
}

QString VfgRunner::findRuntime()
{
#if defined(_WIN32)
    QStringList dirs;
    const QString custom = qEnvironmentVariable("CHIAKI_VFX_PATH");
    if (!custom.isEmpty())
        dirs << custom;
    dirs << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("vfx"));
    dirs << userRuntimeDir();
    const QString sdk = qEnvironmentVariable("NV_VIDEO_EFFECTS_PATH");
    if (!sdk.isEmpty())
        dirs << sdk;
    const QString program_files = qEnvironmentVariable("ProgramFiles");
    if (!program_files.isEmpty())
        dirs << QDir(program_files).filePath(QStringLiteral("NVIDIA Corporation/NVIDIA Video Effects"));
    for (const QString &dir : dirs) {
        if (QFileInfo::exists(QDir(dir).filePath(QStringLiteral("NVVideoEffects.dll"))))
            return QDir::toNativeSeparators(QDir(dir).absolutePath());
    }
#endif
    return {};
}

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <vulkan/vulkan_win32.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

// The parts of the CUDA driver API used here (cuda.h, CUDA 12), declared
// locally so the build does not need the CUDA toolkit. Layouts match cuda.h.
namespace cu {
typedef int Result;
typedef int Device;
typedef struct CUctx_st *Context;
typedef struct CUstream_st *Stream;
typedef struct CUevent_st *Event;
typedef unsigned long long DevicePtr;
typedef struct CUextMemory_st *ExternalMemory;
typedef struct CUextSemaphore_st *ExternalSemaphore;
constexpr Result kSuccess = 0;
constexpr int kMemoryOpaqueWin32 = 2;            // CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32
constexpr int kSemaphoreTimelineWin32 = 10;      // CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TIMELINE_SEMAPHORE_WIN32
constexpr unsigned kMemoryDedicated = 1;         // CUDA_EXTERNAL_MEMORY_DEDICATED
struct MemoryHandleDesc {
    int type;
    union {
        int fd;
        struct { void *handle; const void *name; } win32;
        const void *nvSciBufObject;
    } handle;
    unsigned long long size;
    unsigned int flags;
    unsigned int reserved[16];
};
struct MemoryBufferDesc {
    unsigned long long offset;
    unsigned long long size;
    unsigned int flags;
    unsigned int reserved[16];
};
struct SemaphoreHandleDesc {
    int type;
    union {
        int fd;
        struct { void *handle; const void *name; } win32;
        const void *nvSciSyncObj;
    } handle;
    unsigned int flags;
    unsigned int reserved[16];
};
struct SemaphoreSignalParams {
    struct {
        struct { unsigned long long value; } fence;
        union { void *fence; unsigned long long reserved; } nvSciSync;
        struct { unsigned long long key; } keyedMutex;
        unsigned int reserved[12];
    } params;
    unsigned int flags;
    unsigned int reserved[16];
};
struct SemaphoreWaitParams {
    struct {
        struct { unsigned long long value; } fence;
        union { void *fence; unsigned long long reserved; } nvSciSync;
        struct { unsigned long long key; unsigned int timeoutMs; } keyedMutex;
        unsigned int reserved[10];
    } params;
    unsigned int flags;
    unsigned int reserved[16];
};
} // namespace cu

// NVIDIA Video Effects SDK C API (nvVideoEffects.h, nvCVImage.h), as used by
// NVIDIA's MIT licensed VideoFrameGenerationEffectApp sample.
typedef int NvCVStatus;
typedef void *NvVFXHandle;
typedef void NvCVImage; // opaque here: created, initialised and destroyed by NVCVImage.dll
constexpr NvCVStatus kNvCVSuccess = 0;
constexpr unsigned kNvCVGpu = 1;                  // NVCV_GPU memory space
constexpr int kDxgiFormatR8G8B8A8Unorm = 28;      // DXGI_FORMAT_R8G8B8A8_UNORM

constexpr size_t kInputs = 2;
constexpr VkDeviceSize kPitchAlign = 256;

// Shot-change test on a 64x36 grid of point samples of each input, copied to
// host memory together with the input itself. It replaces the SDK's
// AutomaticShotChangeDetection, which makes every NvVFX_Run wait for the GPU
// and repeats the test for every generated frame of a pair. Measures and
// thresholds as in Veyra (VfgShotChange.hlsl / SceneCadenceAnalyzer):
// normalised luma SAD and 256-bin histogram distance; a flash (large SAD,
// near identical histogram) is not a cut.
constexpr uint32_t kProbeColumns = 64;
constexpr uint32_t kProbeRows = 36;
constexpr uint32_t kProbeSamples = kProbeColumns * kProbeRows;
constexpr double kCutSad = 0.3;
constexpr double kCutHistogram = 0.5;
constexpr double kReplacementSad = 0.08;
constexpr double kReplacementHistogram = 0.9;
constexpr double kFlashSad = 0.5;
constexpr double kFlashHistogram = 0.1;

} // namespace

struct VfgRunner::Impl
{
    pl_vulkan vk = nullptr;
    PFN_vkGetDeviceProcAddr get_device_proc = nullptr;
    const char *stage = "";

    // Vulkan
    PFN_vkCreateBuffer vkCreateBuffer = nullptr;
    PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
    PFN_vkAllocateMemory vkAllocateMemory = nullptr;
    PFN_vkFreeMemory vkFreeMemory = nullptr;
    PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
    PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR = nullptr;
    PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer = nullptr;
    PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage = nullptr;
    PFN_vkSignalSemaphore vkSignalSemaphore = nullptr;
    PFN_vkWaitSemaphores vkWaitSemaphores = nullptr;
    PFN_vkMapMemory vkMapMemory = nullptr;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
    VkPhysicalDeviceMemoryProperties memory_props = {};
    std::array<uint8_t, VK_LUID_SIZE> luid = {};
    bool luid_valid = false;
    VkSemaphore sem = VK_NULL_HANDLE;
    uint64_t last_value = 0;

    // CUDA (nvcuda.dll)
    HMODULE cuda_dll = nullptr;
    cu::Result (__stdcall *cuInit)(unsigned) = nullptr;
    cu::Result (__stdcall *cuDeviceGetCount)(int *) = nullptr;
    cu::Result (__stdcall *cuDeviceGet)(cu::Device *, int) = nullptr;
    cu::Result (__stdcall *cuDeviceGetLuid)(char *, unsigned *, cu::Device) = nullptr;
    cu::Result (__stdcall *cuDevicePrimaryCtxRetain)(cu::Context *, cu::Device) = nullptr;
    cu::Result (__stdcall *cuDevicePrimaryCtxRelease)(cu::Device) = nullptr;
    cu::Result (__stdcall *cuCtxPushCurrent)(cu::Context) = nullptr;
    cu::Result (__stdcall *cuCtxPopCurrent)(cu::Context *) = nullptr;
    cu::Result (__stdcall *cuStreamCreate)(cu::Stream *, unsigned) = nullptr;
    cu::Result (__stdcall *cuStreamDestroy)(cu::Stream) = nullptr;
    cu::Result (__stdcall *cuStreamSynchronize)(cu::Stream) = nullptr;
    // Optional: when a generation finished (host callback on the stream).
    cu::Result (__stdcall *cuLaunchHostFunc)(cu::Stream, void (__stdcall *)(void *), void *) = nullptr;
    std::atomic<uint64_t> ready_value{0};
    std::atomic<int64_t> ready_us{0};
    cu::Result (__stdcall *cuImportExternalMemory)(cu::ExternalMemory *, const cu::MemoryHandleDesc *) = nullptr;
    cu::Result (__stdcall *cuExternalMemoryGetMappedBuffer)(cu::DevicePtr *, cu::ExternalMemory, const cu::MemoryBufferDesc *) = nullptr;
    cu::Result (__stdcall *cuDestroyExternalMemory)(cu::ExternalMemory) = nullptr;
    cu::Result (__stdcall *cuMemFree)(cu::DevicePtr) = nullptr;
    cu::Result (__stdcall *cuImportExternalSemaphore)(cu::ExternalSemaphore *, const cu::SemaphoreHandleDesc *) = nullptr;
    cu::Result (__stdcall *cuDestroyExternalSemaphore)(cu::ExternalSemaphore) = nullptr;
    cu::Result (__stdcall *cuWaitExternalSemaphoresAsync)(const cu::ExternalSemaphore *, const cu::SemaphoreWaitParams *, unsigned, cu::Stream) = nullptr;
    cu::Result (__stdcall *cuSignalExternalSemaphoresAsync)(const cu::ExternalSemaphore *, const cu::SemaphoreSignalParams *, unsigned, cu::Stream) = nullptr;
    cu::Result (__stdcall *cuEventCreate)(cu::Event *, unsigned) = nullptr;
    cu::Result (__stdcall *cuEventDestroy)(cu::Event) = nullptr;
    cu::Result (__stdcall *cuEventRecord)(cu::Event, cu::Stream) = nullptr;
    cu::Result (__stdcall *cuEventQuery)(cu::Event) = nullptr;
    cu::Result (__stdcall *cuEventElapsedTime)(float *, cu::Event, cu::Event) = nullptr;
    cu::Result (__stdcall *cuGetErrorName)(cu::Result, const char **) = nullptr;
    cu::Device device = 0;
    cu::Context context = nullptr;
    bool context_retained = false;
    cu::Stream stream = nullptr;
    cu::ExternalSemaphore cuda_sem = nullptr;
    std::array<cu::Event, 2> events = {};
    bool timing_pending = false;
    float gpu_ms = 0.0f;

    // Video Effects SDK
    HMODULE vfx_dll = nullptr;
    HMODULE cv_dll = nullptr;
    NvCVStatus (__cdecl *NvVFX_CreateEffect)(const char *, NvVFXHandle *) = nullptr;
    void (__cdecl *NvVFX_DestroyEffect)(NvVFXHandle) = nullptr;
    NvCVStatus (__cdecl *NvVFX_SetU32)(NvVFXHandle, const char *, unsigned) = nullptr;
    NvCVStatus (__cdecl *NvVFX_SetImage)(NvVFXHandle, const char *, NvCVImage *) = nullptr;
    NvCVStatus (__cdecl *NvVFX_SetCudaStream)(NvVFXHandle, const char *, cu::Stream) = nullptr;
    NvCVStatus (__cdecl *NvVFX_Load)(NvVFXHandle) = nullptr;
    NvCVStatus (__cdecl *NvVFX_Run)(NvVFXHandle, int) = nullptr;
    NvCVStatus (__cdecl *NvVFX_GetVersion)(unsigned *) = nullptr;
    NvCVStatus (__cdecl *NvCVImage_Create)(unsigned, unsigned, int, int, unsigned, unsigned, unsigned, NvCVImage **) = nullptr;
    void (__cdecl *NvCVImage_Dealloc)(NvCVImage *) = nullptr;
    void (__cdecl *NvCVImage_Destroy)(NvCVImage *) = nullptr;
    NvCVStatus (__cdecl *NvCVImage_Init)(NvCVImage *, unsigned, unsigned, int, void *, int, int, unsigned, unsigned) = nullptr;
    NvCVStatus (__cdecl *NvCVImage_FromD3DFormat)(int, int *, int *, unsigned char *) = nullptr;
    const char *(__cdecl *NvCV_GetErrorStringFromCode)(NvCVStatus) = nullptr;
    int pixel_format = 0;
    int component_type = 0;
    unsigned char layout = 0;

    struct Buffer
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        cu::ExternalMemory external = nullptr;
        cu::DevicePtr pointer = 0;
        NvCVImage *image = nullptr;
    };
    std::array<Buffer, kInputs> inputs;

    // Host visible samples of each input for the shot-change test.
    struct Probe
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        const uint8_t *data = nullptr;
    };
    std::array<Probe, kInputs> probes;
    std::vector<VkBufferImageCopy> probe_regions;
    bool makeProbe(Probe &probe);
    void freeProbe(Probe &probe);
    // Whether the pair crosses a cut; waits for `ready_value` (the inputs
    // copied) first. Runs on the worker thread.
    bool shotChange(int prev_slot, int cur_slot, uint64_t ready_value);
    uint64_t cuts = 0;
    uint64_t pairs = 0;
    std::array<Buffer, VfgRunner::kMaxOutputs> outputs;
    NvVFXHandle effect = nullptr;
    int width = 0;
    int height = 0;
    int mode = -1;
    VkDeviceSize pitch = 0;
    unsigned bound_multiplier = 0;

    // Worker thread running the SDK calls in submission order.
    std::thread worker;
    std::mutex jobs_mutex;
    std::condition_variable jobs_cv;
    std::condition_variable idle_cv;
    std::deque<std::function<void()>> jobs;
    bool busy = false;
    bool stopping = false;
    // Set by the worker after a crash; later jobs only signal from the host.
    bool dead = false;
    std::atomic<int> status { 0 };
    unsigned long crash_code = 0;
    const char *crash_stage = "";

    void startWorker()
    {
        worker = std::thread([this]() {
            std::unique_lock<std::mutex> lock(jobs_mutex);
            for (;;) {
                jobs_cv.wait(lock, [this]() { return stopping || !jobs.empty(); });
                if (jobs.empty())
                    break;
                std::function<void()> job = std::move(jobs.front());
                jobs.pop_front();
                busy = true;
                lock.unlock();
                job();
                lock.lock();
                busy = false;
                idle_cv.notify_all();
            }
        });
    }
    void enqueue(std::function<void()> job)
    {
        {
            std::lock_guard<std::mutex> lock(jobs_mutex);
            jobs.push_back(std::move(job));
        }
        jobs_cv.notify_one();
    }
    // Waits until every queued job has run.
    void drain()
    {
        if (!worker.joinable())
            return;
        std::unique_lock<std::mutex> lock(jobs_mutex);
        idle_cv.wait(lock, [this]() { return jobs.empty() && !busy; });
    }
    void stopWorker()
    {
        if (!worker.joinable())
            return;
        {
            std::lock_guard<std::mutex> lock(jobs_mutex);
            stopping = true;
        }
        jobs_cv.notify_one();
        worker.join();
    }
    // Signals `value` from the host once `after` is reached (timeline values
    // must only grow, and Vulkan signals `after` itself).
    void hostSignal(uint64_t after, uint64_t value)
    {
        VkSemaphoreWaitInfo wait_info = {};
        wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wait_info.semaphoreCount = 1;
        wait_info.pSemaphores = &sem;
        wait_info.pValues = &after;
        vkWaitSemaphores(vk->device, &wait_info, 2000000000ull);
        VkSemaphoreSignalInfo signal_info = {};
        signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signal_info.semaphore = sem;
        signal_info.value = value;
        if (vkSignalSemaphore(vk->device, &signal_info) != VK_SUCCESS)
            qCWarning(chiakiGui) << "[framegen] VFG: could not signal the shared semaphore";
    }

    bool generate(int prev_slot, int cur_slot, int count, uint64_t wait_value, uint64_t signal_value, bool *signalled);

    bool cuOk(cu::Result r, const char *what)
    {
        if (r == cu::kSuccess)
            return true;
        const char *name = nullptr;
        if (cuGetErrorName)
            cuGetErrorName(r, &name);
        qCWarning(chiakiGui).nospace() << "[framegen] VFG: " << what << " failed: " << (name ? name : "?") << " (" << r << ")";
        return false;
    }
    bool cvOk(NvCVStatus r, const char *what)
    {
        if (r == kNvCVSuccess)
            return true;
        const char *text = NvCV_GetErrorStringFromCode ? NvCV_GetErrorStringFromCode(r) : nullptr;
        qCWarning(chiakiGui).nospace() << "[framegen] VFG: " << what << " failed: " << (text ? text : "?") << " (" << r << ")";
        return false;
    }

    // Makes the CUDA context current for the scope.
    struct Current
    {
        Impl &d;
        bool active = false;
        explicit Current(Impl &d_) : d(d_) { active = d.context && d.cuCtxPushCurrent(d.context) == cu::kSuccess; }
        ~Current()
        {
            if (active) {
                cu::Context popped = nullptr;
                d.cuCtxPopCurrent(&popped);
            }
        }
    };

    bool loadCuda();
    bool loadVfx(const QString &root);
    bool loadVulkan();
    bool pickDevice();
    bool makeBuffer(Buffer &buf);
    void freeBuffer(Buffer &buf);
    bool createEffect();
    void destroySized();
};

bool VfgRunner::Impl::loadCuda()
{
    cuda_dll = LoadLibraryExW(L"nvcuda.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!cuda_dll) {
        qCWarning(chiakiGui) << "[framegen] VFG: the NVIDIA driver's CUDA library (nvcuda.dll) is missing";
        return false;
    }
    bool ok = true;
    auto bind = [&](auto &fn, const char *name) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(GetProcAddress(cuda_dll, name));
        if (!fn) {
            qCWarning(chiakiGui) << "[framegen] VFG: nvcuda.dll lacks" << name;
            ok = false;
        }
    };
    bind(cuInit, "cuInit");
    bind(cuDeviceGetCount, "cuDeviceGetCount");
    bind(cuDeviceGet, "cuDeviceGet");
    bind(cuDeviceGetLuid, "cuDeviceGetLuid");
    bind(cuDevicePrimaryCtxRetain, "cuDevicePrimaryCtxRetain");
    bind(cuDevicePrimaryCtxRelease, "cuDevicePrimaryCtxRelease_v2");
    bind(cuCtxPushCurrent, "cuCtxPushCurrent_v2");
    bind(cuCtxPopCurrent, "cuCtxPopCurrent_v2");
    bind(cuStreamCreate, "cuStreamCreate");
    bind(cuStreamDestroy, "cuStreamDestroy_v2");
    bind(cuStreamSynchronize, "cuStreamSynchronize");
    bind(cuImportExternalMemory, "cuImportExternalMemory");
    bind(cuExternalMemoryGetMappedBuffer, "cuExternalMemoryGetMappedBuffer");
    bind(cuDestroyExternalMemory, "cuDestroyExternalMemory");
    bind(cuMemFree, "cuMemFree_v2");
    bind(cuImportExternalSemaphore, "cuImportExternalSemaphore");
    bind(cuDestroyExternalSemaphore, "cuDestroyExternalSemaphore");
    bind(cuWaitExternalSemaphoresAsync, "cuWaitExternalSemaphoresAsync");
    bind(cuSignalExternalSemaphoresAsync, "cuSignalExternalSemaphoresAsync");
    bind(cuEventCreate, "cuEventCreate");
    bind(cuEventDestroy, "cuEventDestroy_v2");
    bind(cuEventRecord, "cuEventRecord");
    bind(cuEventQuery, "cuEventQuery");
    bind(cuEventElapsedTime, "cuEventElapsedTime");
    bind(cuGetErrorName, "cuGetErrorName");
    cuLaunchHostFunc = reinterpret_cast<decltype(cuLaunchHostFunc)>(GetProcAddress(cuda_dll, "cuLaunchHostFunc"));
    return ok && cuOk(cuInit(0), "cuInit");
}

bool VfgRunner::Impl::loadVfx(const QString &root)
{
    // The SDK finds its features (nvVFXVideoFrameGeneration.dll and models)
    // through NV_VIDEO_EFFECTS_PATH; their folders also go on the DLL path.
    if (qEnvironmentVariableIsEmpty("NV_VIDEO_EFFECTS_PATH"))
        qputenv("NV_VIDEO_EFFECTS_PATH", root.toUtf8());
    AddDllDirectory(reinterpret_cast<const wchar_t *>(root.utf16()));
    QString feature;
    QDirIterator it(root, { QStringLiteral("nvVFXVideoFrameGeneration.dll") }, QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext())
        feature = QDir::toNativeSeparators(QFileInfo(it.next()).absolutePath());
    if (feature.isEmpty()) {
        qCWarning(chiakiGui) << "[framegen] VFG: nvVFXVideoFrameGeneration.dll not found under" << root
                             << "- install the Video Frame Generation feature of the NVIDIA Video Effects SDK";
        return false;
    }
    AddDllDirectory(reinterpret_cast<const wchar_t *>(feature.utf16()));
    constexpr DWORD flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
    const QString cv_path = QDir(root).filePath(QStringLiteral("NVCVImage.dll"));
    const QString vfx_path = QDir(root).filePath(QStringLiteral("NVVideoEffects.dll"));
    cv_dll = LoadLibraryExW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(cv_path).utf16()), nullptr, flags);
    vfx_dll = LoadLibraryExW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(vfx_path).utf16()), nullptr, flags);
    if (!cv_dll || !vfx_dll) {
        qCWarning(chiakiGui) << "[framegen] VFG: could not load NVCVImage.dll / NVVideoEffects.dll from" << root
                             << "(Windows error" << GetLastError() << ")";
        return false;
    }
    bool ok = true;
    auto bind = [&](HMODULE dll, auto &fn, const char *name) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(GetProcAddress(dll, name));
        if (!fn) {
            qCWarning(chiakiGui) << "[framegen] VFG: the Video Effects runtime lacks" << name;
            ok = false;
        }
    };
    bind(vfx_dll, NvVFX_CreateEffect, "NvVFX_CreateEffect");
    bind(vfx_dll, NvVFX_DestroyEffect, "NvVFX_DestroyEffect");
    bind(vfx_dll, NvVFX_SetU32, "NvVFX_SetU32");
    bind(vfx_dll, NvVFX_SetImage, "NvVFX_SetImage");
    bind(vfx_dll, NvVFX_SetCudaStream, "NvVFX_SetCudaStream");
    bind(vfx_dll, NvVFX_Load, "NvVFX_Load");
    bind(vfx_dll, NvVFX_Run, "NvVFX_Run");
    bind(vfx_dll, NvVFX_GetVersion, "NvVFX_GetVersion");
    bind(cv_dll, NvCVImage_Create, "NvCVImage_Create");
    bind(cv_dll, NvCVImage_Dealloc, "NvCVImage_Dealloc");
    bind(cv_dll, NvCVImage_Destroy, "NvCVImage_Destroy");
    bind(cv_dll, NvCVImage_Init, "NvCVImage_Init");
    bind(cv_dll, NvCVImage_FromD3DFormat, "NvCVImage_FromD3DFormat");
    bind(cv_dll, NvCV_GetErrorStringFromCode, "NvCV_GetErrorStringFromCode");
    if (!ok)
        return false;
    unsigned version = 0;
    NvVFX_GetVersion(&version);
    qCInfo(chiakiGui).nospace() << "[framegen] VFG: Video Effects SDK " << (version >> 24) << "." << ((version >> 16) & 0xff)
                                << "." << ((version >> 8) & 0xff) << " from " << root << ", feature in " << feature;
    // RGBA8 interleaved, the layout of the frame generation textures.
    return cvOk(NvCVImage_FromD3DFormat(kDxgiFormatR8G8B8A8Unorm, &pixel_format, &component_type, &layout),
                "NvCVImage_FromD3DFormat");
}

bool VfgRunner::Impl::loadVulkan()
{
    bool ok = true;
#define VFG_DEVICE_PROC(name_) \
    name_ = reinterpret_cast<PFN_##name_>(get_device_proc(vk->device, #name_)); \
    if (!name_) { qCWarning(chiakiGui) << "[framegen] VFG: missing Vulkan function" << #name_; ok = false; }
    VFG_DEVICE_PROC(vkCreateBuffer)
    VFG_DEVICE_PROC(vkDestroyBuffer)
    VFG_DEVICE_PROC(vkGetBufferMemoryRequirements)
    VFG_DEVICE_PROC(vkAllocateMemory)
    VFG_DEVICE_PROC(vkFreeMemory)
    VFG_DEVICE_PROC(vkBindBufferMemory)
    VFG_DEVICE_PROC(vkGetMemoryWin32HandleKHR)
    VFG_DEVICE_PROC(vkCmdCopyImageToBuffer)
    VFG_DEVICE_PROC(vkCmdCopyBufferToImage)
    VFG_DEVICE_PROC(vkSignalSemaphore)
    VFG_DEVICE_PROC(vkWaitSemaphores)
    VFG_DEVICE_PROC(vkMapMemory)
    VFG_DEVICE_PROC(vkCmdPipelineBarrier)
#undef VFG_DEVICE_PROC
    auto get_memory_props = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        vk->get_proc_addr(vk->instance, "vkGetPhysicalDeviceMemoryProperties"));
    auto get_props2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
        vk->get_proc_addr(vk->instance, "vkGetPhysicalDeviceProperties2"));
    if (!ok || !get_memory_props || !get_props2) {
        qCWarning(chiakiGui) << "[framegen] VFG: the Vulkan device cannot share memory with CUDA"
                                " (VK_KHR_external_memory_win32 missing?)";
        return false;
    }
    get_memory_props(vk->phys_device, &memory_props);
    VkPhysicalDeviceIDProperties id_props = {};
    id_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    VkPhysicalDeviceProperties2 props = {};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props.pNext = &id_props;
    get_props2(vk->phys_device, &props);
    luid_valid = id_props.deviceLUIDValid;
    std::memcpy(luid.data(), id_props.deviceLUID, VK_LUID_SIZE);
    if (props.properties.vendorID != 0x10de) {
        qCWarning(chiakiGui) << "[framegen] VFG needs an NVIDIA GPU";
        return false;
    }
    if (!(vk->gpu->export_caps.sync & PL_HANDLE_WIN32) || !(vk->gpu->export_caps.buf & PL_HANDLE_WIN32)) {
        qCWarning(chiakiGui) << "[framegen] VFG: the Vulkan device cannot export memory and semaphores";
        return false;
    }
    return true;
}

bool VfgRunner::Impl::pickDevice()
{
    int count = 0;
    if (!cuOk(cuDeviceGetCount(&count), "cuDeviceGetCount") || count <= 0)
        return false;
    for (int i = 0; i < count; i++) {
        cu::Device dev = 0;
        if (cuDeviceGet(&dev, i) != cu::kSuccess)
            continue;
        char dev_luid[8] = {};
        unsigned node_mask = 0;
        if (cuDeviceGetLuid(dev_luid, &node_mask, dev) != cu::kSuccess)
            continue;
        if (!luid_valid || std::memcmp(dev_luid, luid.data(), 8) == 0) {
            device = dev;
            return true;
        }
    }
    qCWarning(chiakiGui) << "[framegen] VFG: no CUDA device matches the Vulkan GPU";
    return false;
}

bool VfgRunner::Impl::makeBuffer(Buffer &buf)
{
    const VkDeviceSize size = pitch * static_cast<VkDeviceSize>(height);
    VkExternalMemoryBufferCreateInfo ext_info = {};
    ext_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
    ext_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    VkBufferCreateInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.pNext = &ext_info;
    buf_info.size = size;
    buf_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vk->device, &buf_info, nullptr, &buf.buffer) != VK_SUCCESS)
        return false;
    VkMemoryRequirements reqs = {};
    vkGetBufferMemoryRequirements(vk->device, buf.buffer, &reqs);
    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < memory_props.memoryTypeCount; i++) {
        if ((reqs.memoryTypeBits & (1u << i)) &&
            (memory_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            type = i;
            break;
        }
    }
    if (type == UINT32_MAX)
        return false;
    VkMemoryDedicatedAllocateInfo dedicated = {};
    dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicated.buffer = buf.buffer;
    VkExportMemoryAllocateInfo export_info = {};
    export_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    export_info.pNext = &dedicated;
    export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    VkMemoryAllocateInfo alloc = {};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.pNext = &export_info;
    alloc.allocationSize = reqs.size;
    alloc.memoryTypeIndex = type;
    if (vkAllocateMemory(vk->device, &alloc, nullptr, &buf.memory) != VK_SUCCESS)
        return false;
    if (vkBindBufferMemory(vk->device, buf.buffer, buf.memory, 0) != VK_SUCCESS)
        return false;
    VkMemoryGetWin32HandleInfoKHR handle_info = {};
    handle_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    handle_info.memory = buf.memory;
    handle_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    HANDLE handle = nullptr;
    if (vkGetMemoryWin32HandleKHR(vk->device, &handle_info, &handle) != VK_SUCCESS || !handle)
        return false;
    cu::MemoryHandleDesc mem_desc = {};
    mem_desc.type = cu::kMemoryOpaqueWin32;
    mem_desc.handle.win32.handle = handle;
    mem_desc.size = reqs.size;
    mem_desc.flags = cu::kMemoryDedicated;
    // CUDA does not take ownership of an opaque Win32 handle.
    const bool imported = cuOk(cuImportExternalMemory(&buf.external, &mem_desc), "cuImportExternalMemory");
    CloseHandle(handle);
    if (!imported)
        return false;
    cu::MemoryBufferDesc map_desc = {};
    map_desc.size = size;
    if (!cuOk(cuExternalMemoryGetMappedBuffer(&buf.pointer, buf.external, &map_desc), "cuExternalMemoryGetMappedBuffer"))
        return false;
    // NvCVImage_Create allocates the descriptor (and a tiny CPU image that is
    // dropped); Init then points it at the shared CUDA memory without owning it.
    if (!cvOk(NvCVImage_Create(1, 1, pixel_format, component_type, layout, 0, 1, &buf.image), "NvCVImage_Create"))
        return false;
    NvCVImage_Dealloc(buf.image);
    return cvOk(NvCVImage_Init(buf.image, static_cast<unsigned>(width), static_cast<unsigned>(height),
                               static_cast<int>(pitch), reinterpret_cast<void *>(buf.pointer),
                               pixel_format, component_type, layout, kNvCVGpu),
                "NvCVImage_Init");
}

void VfgRunner::Impl::freeBuffer(Buffer &buf)
{
    if (buf.image)
        NvCVImage_Destroy(buf.image);
    if (buf.pointer)
        cuMemFree(buf.pointer);
    if (buf.external)
        cuDestroyExternalMemory(buf.external);
    if (buf.buffer)
        vkDestroyBuffer(vk->device, buf.buffer, nullptr);
    if (buf.memory)
        vkFreeMemory(vk->device, buf.memory, nullptr);
    buf = Buffer();
}

bool VfgRunner::Impl::makeProbe(Probe &probe)
{
    VkBufferCreateInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = kProbeSamples * 4;
    buf_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vk->device, &buf_info, nullptr, &probe.buffer) != VK_SUCCESS)
        return false;
    VkMemoryRequirements reqs = {};
    vkGetBufferMemoryRequirements(vk->device, probe.buffer, &reqs);
    const VkMemoryPropertyFlags wanted = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < memory_props.memoryTypeCount && type == UINT32_MAX; i++) {
        if ((reqs.memoryTypeBits & (1u << i)) && (memory_props.memoryTypes[i].propertyFlags & wanted) == wanted)
            type = i;
    }
    if (type == UINT32_MAX)
        return false;
    VkMemoryAllocateInfo alloc = {};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = reqs.size;
    alloc.memoryTypeIndex = type;
    void *mapped = nullptr;
    if (vkAllocateMemory(vk->device, &alloc, nullptr, &probe.memory) != VK_SUCCESS ||
        vkBindBufferMemory(vk->device, probe.buffer, probe.memory, 0) != VK_SUCCESS ||
        vkMapMemory(vk->device, probe.memory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS || !mapped)
        return false;
    probe.data = static_cast<const uint8_t *>(mapped);
    return true;
}

void VfgRunner::Impl::freeProbe(Probe &probe)
{
    // Freeing the memory also unmaps it.
    if (probe.buffer)
        vkDestroyBuffer(vk->device, probe.buffer, nullptr);
    if (probe.memory)
        vkFreeMemory(vk->device, probe.memory, nullptr);
    probe = Probe();
}

bool VfgRunner::Impl::shotChange(int prev_slot, int cur_slot, uint64_t ready_value)
{
    const Probe &a = probes[static_cast<size_t>(prev_slot)];
    const Probe &b = probes[static_cast<size_t>(cur_slot)];
    if (!a.data || !b.data)
        return false;
    stage = "vkWaitSemaphores (shot change)";
    VkSemaphoreWaitInfo wait_info = {};
    wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    wait_info.semaphoreCount = 1;
    wait_info.pSemaphores = &sem;
    wait_info.pValues = &ready_value;
    if (vkWaitSemaphores(vk->device, &wait_info, 200000000ull) != VK_SUCCESS)
        return false;
    // Integer luma of the samples (RGBA8), as in Veyra's sampler.
    std::array<uint32_t, 256> hist_a = {};
    std::array<uint32_t, 256> hist_b = {};
    uint64_t sad = 0;
    for (uint32_t i = 0; i < kProbeSamples; i++) {
        const uint8_t *pa = a.data + i * 4;
        const uint8_t *pb = b.data + i * 4;
        const uint32_t la = (54u * pa[0] + 183u * pa[1] + 19u * pa[2] + 128u) >> 8;
        const uint32_t lb = (54u * pb[0] + 183u * pb[1] + 19u * pb[2] + 128u) >> 8;
        hist_a[(std::min)(la, 255u)]++;
        hist_b[(std::min)(lb, 255u)]++;
        sad += la > lb ? la - lb : lb - la;
    }
    uint64_t hist_l1 = 0;
    for (size_t i = 0; i < hist_a.size(); i++)
        hist_l1 += hist_a[i] > hist_b[i] ? hist_a[i] - hist_b[i] : hist_b[i] - hist_a[i];
    const double sad_n = static_cast<double>(sad) / (255.0 * kProbeSamples);
    const double hist_n = static_cast<double>(hist_l1) / (2.0 * kProbeSamples);
    const bool flash = sad_n > kFlashSad && hist_n < kFlashHistogram;
    const bool cut = !flash && ((sad_n > kCutSad && hist_n > kCutHistogram) ||
                                (sad_n > kReplacementSad && hist_n > kReplacementHistogram));
    pairs++;
    if (cut) {
        cuts++;
        // Rare by nature; a burst means the test misfires on this content.
        if (cuts <= 20 || cuts % 100 == 0)
            qCInfo(chiakiGui).nospace() << "[framegen] VFG: shot change " << cuts << " of " << pairs
                                        << " pairs (sad " << sad_n << ", histogram " << hist_n << ")";
    }
    return cut;
}

bool VfgRunner::Impl::createEffect()
{
    // Order as in NVIDIA's VideoFrameGenerationEffectApp sample.
    stage = "NvVFX_CreateEffect";
    if (!cvOk(NvVFX_CreateEffect("VideoFrameGeneration", &effect), "NvVFX_CreateEffect"))
        return false;
    stage = "NvVFX_Load";
    const bool ok = cvOk(NvVFX_SetCudaStream(effect, "CudaStream", stream), "SetCudaStream") &&
                    cvOk(NvVFX_SetU32(effect, "InputWidth", static_cast<unsigned>(width)), "InputWidth") &&
                    cvOk(NvVFX_SetU32(effect, "InputHeight", static_cast<unsigned>(height)), "InputHeight") &&
                    cvOk(NvVFX_SetU32(effect, "Mode", static_cast<unsigned>(mode)), "Mode") &&
                    cvOk(NvVFX_SetU32(effect, "AutomaticShotChangeDetectionEnabled", 0u), "AutomaticShotChangeDetectionEnabled") &&
                    cvOk(NvVFX_Load(effect), "NvVFX_Load") &&
                    cvOk(NvVFX_SetImage(effect, "DstImage0", outputs[0].image), "DstImage0");
    bound_multiplier = 0;
    return ok;
}

void VfgRunner::Impl::destroySized()
{
    drain();
    Current current(*this);
    if (current.active && stream)
        cuStreamSynchronize(stream);
    if (effect)
        NvVFX_DestroyEffect(effect);
    effect = nullptr;
    for (auto &buf : inputs)
        freeBuffer(buf);
    for (auto &buf : outputs)
        freeBuffer(buf);
    width = height = 0;
    mode = -1;
    bound_multiplier = 0;
}

VfgRunner::VfgRunner(pl_vulkan vk, PFN_vkGetDeviceProcAddr get_device_proc)
    : impl(std::make_unique<Impl>())
{
    impl->vk = vk;
    impl->get_device_proc = get_device_proc;
}

VfgRunner::~VfgRunner()
{
    Impl &d = *impl;
    d.drain();
    d.stopWorker();
    if (d.vfx_dll && d.cuda_dll)
        d.destroySized();
    for (auto &probe : d.probes)
        d.freeProbe(probe);
    {
        Impl::Current current(d);
        for (auto &event : d.events) {
            if (event)
                d.cuEventDestroy(event);
            event = nullptr;
        }
        if (d.stream)
            d.cuStreamDestroy(d.stream);
        if (d.cuda_sem)
            d.cuDestroyExternalSemaphore(d.cuda_sem);
    }
    if (d.context_retained)
        d.cuDevicePrimaryCtxRelease(d.device);
    if (d.sem)
        pl_vulkan_sem_destroy(d.vk->gpu, &d.sem);
    // The runtime libraries stay loaded: unloading CUDA or the SDK while the
    // process keeps running is not supported by them.
}

bool VfgRunner::init()
{
    Impl &d = *impl;
    const QString root = findRuntime();
    if (root.isEmpty()) {
        qCWarning(chiakiGui) << "[framegen] VFG: NVIDIA runtime not found (use \"Download NVIDIA runtime\" in the"
                                " settings, put it in a \"vfx\" folder next to chiaki.exe, or set CHIAKI_VFX_PATH)";
        return false;
    }
    if (!d.loadVulkan() || !d.loadCuda() || !d.pickDevice() || !d.loadVfx(root))
        return false;
    if (!d.cuOk(d.cuDevicePrimaryCtxRetain(&d.context, d.device), "cuDevicePrimaryCtxRetain"))
        return false;
    d.context_retained = true;
    Impl::Current current(d);
    if (!current.active)
        return false;
    if (!d.cuOk(d.cuStreamCreate(&d.stream, 1 /* CU_STREAM_NON_BLOCKING */), "cuStreamCreate"))
        return false;
    for (auto &event : d.events) {
        if (!d.cuOk(d.cuEventCreate(&event, 0), "cuEventCreate"))
            return false;
    }

    // One timeline semaphore, signalled by Vulkan when the inputs are copied
    // and by CUDA when the generated frames are written.
    union pl_handle handle = {};
    struct pl_vulkan_sem_params sem_params = {};
    sem_params.type = VK_SEMAPHORE_TYPE_TIMELINE;
    sem_params.export_handle = PL_HANDLE_WIN32;
    sem_params.out_handle = &handle;
    d.sem = pl_vulkan_sem_create(d.vk->gpu, &sem_params);
    if (!d.sem || !handle.handle) {
        qCWarning(chiakiGui) << "[framegen] VFG: could not create a shared semaphore";
        return false;
    }
    cu::SemaphoreHandleDesc sem_desc = {};
    sem_desc.type = cu::kSemaphoreTimelineWin32;
    sem_desc.handle.win32.handle = handle.handle;
    const bool imported = d.cuOk(d.cuImportExternalSemaphore(&d.cuda_sem, &sem_desc), "cuImportExternalSemaphore");
    CloseHandle(handle.handle);
    if (!imported)
        return false;
    for (auto &probe : d.probes) {
        if (!d.makeProbe(probe)) {
            qCWarning(chiakiGui) << "[framegen] VFG: could not create the shot-change sample buffers";
            return false;
        }
    }
    d.startWorker();
    qCInfo(chiakiGui) << "[framegen] NVIDIA VFG initialized";
    return true;
}

bool VfgRunner::ensureSize(int w, int h, int mode)
{
    Impl &d = *impl;
    mode = std::clamp(mode, 0, 2);
    if (d.effect && w == d.width && h == d.height && mode == d.mode)
        return true;
    d.destroySized();
    Impl::Current current(d);
    if (!current.active)
        return false;
    d.width = w;
    d.height = h;
    d.mode = mode;
    d.pitch = (static_cast<VkDeviceSize>(w) * 4 + kPitchAlign - 1) / kPitchAlign * kPitchAlign;
    d.probe_regions.assign(kProbeSamples, VkBufferImageCopy());
    for (uint32_t i = 0; i < kProbeSamples; i++) {
        VkBufferImageCopy &region = d.probe_regions[i];
        region.bufferOffset = static_cast<VkDeviceSize>(i) * 4;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageOffset.x = static_cast<int32_t>((i % kProbeColumns) * static_cast<uint32_t>(w) / kProbeColumns);
        region.imageOffset.y = static_cast<int32_t>((i / kProbeColumns) * static_cast<uint32_t>(h) / kProbeRows);
        region.imageExtent = { 1, 1, 1 };
    }
    d.stage = "buffers";
    for (auto &buf : d.inputs) {
        if (!d.makeBuffer(buf)) {
            qCWarning(chiakiGui) << "[framegen] VFG: could not create the shared input buffers";
            d.destroySized();
            return false;
        }
    }
    for (auto &buf : d.outputs) {
        if (!d.makeBuffer(buf)) {
            qCWarning(chiakiGui) << "[framegen] VFG: could not create the shared output buffers";
            d.destroySized();
            return false;
        }
    }
    if (!d.createEffect()) {
        d.destroySized();
        return false;
    }
    static const char *const names[] = { "Low", "Medium", "High" };
    qCInfo(chiakiGui).nospace() << "[framegen] VFG ready: " << w << "x" << h << ", quality " << names[mode];
    return true;
}

bool VfgRunner::sizeMatches(int w, int h, int mode) const
{
    const Impl &d = *impl;
    return d.effect && w == d.width && h == d.height && std::clamp(mode, 0, 2) == d.mode;
}

void VfgRunner::invalidate()
{
    impl->destroySized();
}

VkSemaphore VfgRunner::semaphore() const
{
    return impl->sem;
}

uint64_t VfgRunner::lastValue() const
{
    return impl->last_value;
}

void VfgRunner::setLastValue(uint64_t value)
{
    impl->last_value = value;
}

void VfgRunner::recordUpload(VkCommandBuffer cmd, VkImage image, int slot)
{
    Impl &d = *impl;
    VkBufferImageCopy region = {};
    region.bufferRowLength = static_cast<uint32_t>(d.pitch / 4);
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { static_cast<uint32_t>(d.width), static_cast<uint32_t>(d.height), 1 };
    d.vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_GENERAL, d.inputs[static_cast<size_t>(slot)].buffer, 1, &region);
    const Impl::Probe &probe = d.probes[static_cast<size_t>(slot)];
    if (probe.buffer && !d.probe_regions.empty()) {
        d.vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_GENERAL, probe.buffer,
                                 static_cast<uint32_t>(d.probe_regions.size()), d.probe_regions.data());
        // The worker reads the samples on the host after the semaphore.
        VkMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        d.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                               1, &barrier, 0, nullptr, 0, nullptr);
    }
}

void VfgRunner::recordDownload(VkCommandBuffer cmd, VkImage image, int index)
{
    Impl &d = *impl;
    VkBufferImageCopy region = {};
    region.bufferRowLength = static_cast<uint32_t>(d.pitch / 4);
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { static_cast<uint32_t>(d.width), static_cast<uint32_t>(d.height), 1 };
    d.vkCmdCopyBufferToImage(cmd, d.outputs[static_cast<size_t>(index)].buffer, image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
}

bool VfgRunner::Impl::generate(int prev_slot, int cur_slot, int count, uint64_t wait_value, uint64_t signal_value,
                               bool *signalled)
{
    Impl &d = *this;
    // On a cut the SDK outputs the current picture instead of a blend.
    const bool shot_change = shotChange(prev_slot, cur_slot, wait_value);
    *signalled = false;
    count = std::clamp(count, 1, kMaxOutputs);
    Impl::Current current(d);
    if (!current.active || !d.effect)
        return false;

    // Timing of the previous call, if finished.
    if (d.timing_pending && d.cuEventQuery(d.events[1]) == cu::kSuccess) {
        float ms = 0.0f;
        if (d.cuEventElapsedTime(&ms, d.events[0], d.events[1]) == cu::kSuccess)
            d.gpu_ms = d.gpu_ms > 0.0f ? d.gpu_ms * 0.9f + ms * 0.1f : ms;
        d.timing_pending = false;
    }

    d.stage = "cuWaitExternalSemaphoresAsync";
    cu::SemaphoreWaitParams wait = {};
    wait.params.fence.value = wait_value;
    if (!d.cuOk(d.cuWaitExternalSemaphoresAsync(&d.cuda_sem, &wait, 1, d.stream), "cuWaitExternalSemaphoresAsync"))
        return false;
    // From here on the stream waits for `wait_value`; signal even on failure.
    const bool time_it = !d.timing_pending;
    if (time_it)
        d.cuEventRecord(d.events[0], d.stream);

    const unsigned multiplier = static_cast<unsigned>(count + 1);
    bool ok = true;
    if (d.bound_multiplier != multiplier) {
        d.stage = "FrameMultiplier";
        ok = d.cvOk(d.NvVFX_SetU32(d.effect, "FrameMultiplier", multiplier), "FrameMultiplier");
        if (ok)
            d.bound_multiplier = multiplier;
    }
    // Both inputs are bound again for every new pair, as the SDK requires.
    d.stage = "SetImage";
    ok = ok && d.cvOk(d.NvVFX_SetImage(d.effect, "SrcImage0", d.inputs[static_cast<size_t>(prev_slot)].image), "SrcImage0") &&
         d.cvOk(d.NvVFX_SetImage(d.effect, "SrcImage1", d.inputs[static_cast<size_t>(cur_slot)].image), "SrcImage1") &&
         d.cvOk(d.NvVFX_SetU32(d.effect, "ShotChange", shot_change ? 1u : 0u), "ShotChange");
    for (int k = 1; ok && k <= count; k++) {
        d.stage = "NvVFX_Run";
        ok = d.cvOk(d.NvVFX_SetImage(d.effect, "DstImage0", d.outputs[static_cast<size_t>(k - 1)].image), "DstImage0") &&
             d.cvOk(d.NvVFX_SetU32(d.effect, "FrameIndex", static_cast<unsigned>(k)), "FrameIndex") &&
             d.cvOk(d.NvVFX_Run(d.effect, 1), "NvVFX_Run");
    }
    if (time_it && ok) {
        d.cuEventRecord(d.events[1], d.stream);
        d.timing_pending = true;
    }
    // The semaphore is signalled even after a failure, so the Vulkan side
    // waiting for it never stalls; the caller drops the frames then.
    d.stage = "cuSignalExternalSemaphoresAsync";
    cu::SemaphoreSignalParams signal = {};
    signal.params.fence.value = signal_value;
    const bool signalled_ok = d.cuOk(d.cuSignalExternalSemaphoresAsync(&d.cuda_sem, &signal, 1, d.stream),
                                  "cuSignalExternalSemaphoresAsync");
    if (!signalled_ok)
        return false;
    *signalled = true;
    if (cuLaunchHostFunc) {
        struct Ready { Impl *impl; uint64_t value; };
        auto *r = new Ready{this, signal_value};
        auto cb = [](void *p) {
            auto *ready = static_cast<Ready *>(p);
            LARGE_INTEGER f, v;
            QueryPerformanceFrequency(&f);
            QueryPerformanceCounter(&v);
            // Same clock as chiaki_time_now_monotonic_us.
            ready->impl->ready_us.store(static_cast<int64_t>(v.QuadPart * 1000000 / f.QuadPart));
            ready->impl->ready_value.store(ready->value);
            delete ready;
        };
        if (cuLaunchHostFunc(d.stream, static_cast<void (__stdcall *)(void *)>(cb), r) != cu::kSuccess)
            delete r;
    }
    return ok;
}

void VfgRunner::generateAsync(int prev_slot, int cur_slot, int count, uint64_t wait_value, uint64_t signal_value)
{
    Impl *d = impl.get();
    d->enqueue([d, prev_slot, cur_slot, count, wait_value, signal_value]() {
        if (d->dead) {
            d->hostSignal(wait_value, signal_value);
            return;
        }
        bool signalled = false;
        unsigned long seh_code = 0;
        const bool ok = guardedCall([&]() {
            return d->generate(prev_slot, cur_slot, count, wait_value, signal_value, &signalled);
        }, &seh_code);
        if (!signalled)
            d->hostSignal(wait_value, signal_value);
        if (seh_code) {
            d->crash_code = seh_code;
            d->crash_stage = d->stage;
            d->dead = true;
            d->status.store(2);
        } else if (!ok) {
            int expected = 0;
            d->status.compare_exchange_strong(expected, 1);
        }
    });
}

int VfgRunner::takeStatus(unsigned long *code, const char **stage)
{
    Impl &d = *impl;
    const int status = d.status.load();
    if (status == 2) {
        *code = d.crash_code;
        *stage = d.crash_stage;
        return 2;
    }
    if (status == 1) {
        int expected = 1;
        d.status.compare_exchange_strong(expected, 0);
    }
    return status;
}

float VfgRunner::gpuTimeMs()
{
    return impl->gpu_ms;
}

bool VfgRunner::readyInfo(uint64_t *value, int64_t *ready_us) const
{
    if (!impl->cuLaunchHostFunc)
        return false;
    *ready_us = impl->ready_us.load();
    *value = impl->ready_value.load();
    return *ready_us > 0;
}

const char *VfgRunner::stage() const
{
    return impl->stage;
}

#else // !_WIN32

struct VfgRunner::Impl {};

VfgRunner::VfgRunner(pl_vulkan, PFN_vkGetDeviceProcAddr) {}
VfgRunner::~VfgRunner() = default;
bool VfgRunner::init() { return false; }
bool VfgRunner::ensureSize(int, int, int) { return false; }
bool VfgRunner::sizeMatches(int, int, int) const { return false; }
void VfgRunner::invalidate() {}
VkSemaphore VfgRunner::semaphore() const { return VK_NULL_HANDLE; }
uint64_t VfgRunner::lastValue() const { return 0; }
void VfgRunner::setLastValue(uint64_t) {}
void VfgRunner::recordUpload(VkCommandBuffer, VkImage, int) {}
void VfgRunner::recordDownload(VkCommandBuffer, VkImage, int) {}
void VfgRunner::generateAsync(int, int, int, uint64_t, uint64_t) {}
int VfgRunner::takeStatus(unsigned long *, const char **) { return 0; }
float VfgRunner::gpuTimeMs() { return 0.0f; }
bool VfgRunner::readyInfo(uint64_t *, int64_t *) const { return false; }
const char *VfgRunner::stage() const { return ""; }

#endif // _WIN32
