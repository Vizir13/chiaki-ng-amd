#ifndef CHIAKI_GUI_VFG_H
#define CHIAKI_GUI_VFG_H

#include <libplacebo/vulkan.h>

#include <QString>

#include <memory>

// NVIDIA Video Frame Generation: the "VideoFrameGeneration" effect of the
// NVIDIA Video Effects SDK, a video-trained interpolation model that needs
// only the two pictures (no motion vectors or depth). It runs in CUDA on
// buffers shared with libplacebo's Vulkan device and synchronised through a
// shared timeline semaphore.
//
// The runtime is not bundled: install-nvidia-vfx.ps1 (installer option or the
// settings' download button) fetches it from NVIDIA into userRuntimeDir().
// It is found through CHIAKI_VFX_PATH, a "vfx" folder next to chiaki.exe,
// userRuntimeDir(), NV_VIDEO_EFFECTS_PATH or the SDK's default install folder
// (Windows, NVIDIA RTX GPU).
class VfgRunner
{
public:
    static constexpr int kMaxOutputs = 3;

    // Folder holding NVVideoEffects.dll, or empty if none was found.
    static QString findRuntime();
    // Per-user folder the download script installs into.
    static QString userRuntimeDir();

    VfgRunner(pl_vulkan vk, PFN_vkGetDeviceProcAddr get_device_proc);
    ~VfgRunner();
    VfgRunner(const VfgRunner &) = delete;
    VfgRunner &operator=(const VfgRunner &) = delete;

    // Loads the runtime and sets up CUDA on the GPU of `vk`; logs why it
    // cannot run.
    bool init();
    // (Re)creates the shared buffers and the effect for this size and
    // quality (0 Low, 1 Medium, 2 High). No work may be pending.
    bool ensureSize(int w, int h, int mode);
    // Whether ensureSize() would keep the current buffers and effect.
    bool sizeMatches(int w, int h, int mode) const;
    // Drops the size dependent state. No work may be pending.
    void invalidate();

    // Timeline semaphore shared with CUDA, and its last value that has been
    // (or will be) signalled.
    VkSemaphore semaphore() const;
    uint64_t lastValue() const;
    void setLastValue(uint64_t value);

    // Copies `image` (RGBA8, VK_IMAGE_LAYOUT_GENERAL) into input buffer
    // `slot` (0 or 1), and output buffer `index` into `image`.
    void recordUpload(VkCommandBuffer cmd, VkImage image, int slot);
    void recordDownload(VkCommandBuffer cmd, VkImage image, int index);

    // On the runner's worker thread (the SDK calls block for most of the
    // model's run time, which must not stall the render thread): waits for
    // `wait_value`, generates `count` evenly spaced frames between input
    // `prev_slot` and `cur_slot` into the output buffers and signals
    // `signal_value`. The value is always signalled, from the host if CUDA
    // cannot, so Vulkan work waiting for it never hangs; failures are
    // reported through takeStatus().
    void generateAsync(int prev_slot, int cur_slot, int count, uint64_t wait_value, uint64_t signal_value);
    // 0 = fine, 1 = a generation failed, 2 = the runtime crashed (`code`,
    // `stage` say where; the runner must not be used any more). Resets 1.
    int takeStatus(unsigned long *code, const char **stage);

    // Average GPU time of the generation per stream frame in milliseconds.
    float gpuTimeMs();
    // The last signal value whose generation finished on the GPU and when
    // (monotonic us; seen by a CUDA host callback). False if not measured.
    bool readyInfo(uint64_t *value, int64_t *ready_us) const;
    // The runtime call in progress, named in the log if the runtime crashes.
    const char *stage() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

#endif // CHIAKI_GUI_VFG_H
