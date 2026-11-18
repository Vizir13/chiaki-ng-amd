#ifndef CHIAKI_GUI_VKREFLEX_H
#define CHIAKI_GUI_VKREFLEX_H

#include <libplacebo/vulkan.h>

#include <QString>
#include <QtGlobal>

#include <vector>

// NVIDIA Reflex for the Vulkan presentation (VK_NV_low_latency2) and the
// display time of each present (VK_KHR_present_id + VK_KHR_present_wait).
//
// libplacebo owns the swapchain, so the Vulkan calls it makes are wrapped:
// its device is created with the get_proc_addr returned by
// wrapInstanceProcAddr(), which hands out wrappers of vkCreateSwapchainKHR
// (low latency mode), vkDestroySwapchainKHR, vkQueuePresentKHR (present id,
// present markers, present wait) and vkQueueSubmit[2] (the present id the
// work belongs to).
//
// CHIAKI_REFLEX=0 turns the low latency mode off, CHIAKI_PRESENT_WAIT=0
// turns off present ids, present waits and with them the low latency mode.
namespace VkReflex {

PFN_vkGetInstanceProcAddr wrapInstanceProcAddr(PFN_vkGetInstanceProcAddr real);
void appendDeviceExtensions(std::vector<const char *> &dev_extensions);
// Extra device features (present id / present wait) for pl_vulkan_params.
const VkPhysicalDeviceFeatures2 *deviceFeatures();
// Call once the device exists; enables what the device got.
void onDeviceCreated(pl_vulkan vulkan);
void onDeviceDestroyed();

// Low latency mode of the swapchains created from now on (setting).
void setLowLatencyEnabled(bool enabled);

// Reflex markers of the frame the next present belongs to.
void markFrameStart();
void markRenderSubmitted();

// Holds the next vkQueuePresentKHR until target_us (monotonic), after the
// frame's rendering was already submitted to the GPU. Returns false when the
// present is not wrapped (then the caller has to wait before submitting).
// `done` is called with the target and the time the present went out.
bool delayNextPresent(qint64 target_us, void (*done)(qint64 target_us, qint64 present_us, qint64 waited_us));

// Display period the stats are measured in (refreshes per frame).
void setRefreshRate(double hz);
// Time (monotonic us) the last present became visible, when one was seen
// within max_age_us.
bool lastDisplayTime(qint64 *display_us, qint64 max_age_us);
// Display timing since the last call, empty when present wait is off.
QString takeDisplayStats();
bool lowLatencyActive();
bool presentWaitActive();

}

#endif // CHIAKI_GUI_VKREFLEX_H
