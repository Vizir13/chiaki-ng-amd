![chiaki-ng Logo](gui/res/chiaking-logo.svg)

# chiaki-ng-enhanced

A fork of [chiaki-ng](https://github.com/streetpea/chiaki-ng) (the open source PlayStation 4/5 Remote Play client) that adds video enhancement and smoother streaming on top of upstream: NVIDIA RTX Video Super Resolution, four frame generation backends (NVIDIA VFG, DLSS, FSR and its own **Flow** interpolation; FSR and Flow also run on Linux and Steam Deck), an ambient border for ultrawide displays and a set of Windows streaming tweaks and fixes.

Everything else (setup, registration, controllers, Steam Deck usage) works as in upstream chiaki-ng – see its [documentation site](https://streetpea.github.io/chiaki-ng/).

## Downloads

Pre-releases are published on the [Releases](../../releases) page (tags like `1.10.0-rtx.4`):

| File | Platform |
|---|---|
| `chiaki-ng-*-win_x64-setup.exe` | Windows x64 installer |
| `chiaki-ng-*-win_x64-portable.zip` | Windows x64 portable |
| `chiaki-ng-*-linux_x86_64.AppImage` | Linux x86_64 (`chmod +x`, then run) |

Test builds of every commit are available as artifacts of the *Build chiaki-ng Windows x86_64 (VC)* and *Build chiaki-ng Appimage x86_64* workflows in the Actions tab.

## What this fork adds

### Video enhancement

| Feature | Platforms | GPU | Where |
|---|---|---|---|
| **RTX Video Super Resolution** – AI upscaling of the stream to the window size (Low / Medium / High / Ultra) | Windows | NVIDIA RTX 20 series or newer | Settings → Video, or Ctrl+O → Enhancement |
| **NVIDIA VFG** – NVIDIA's video-trained frame generation (Video Effects SDK), 2x–4x, Low / Medium / High; the least "jelly" option for 30 fps games. Optional second stage: VFG 2x + DLSS Frame Generation 2x = 4x for much less GPU time than VFG 4x. NVIDIA runtime downloaded on request (installer option or settings button) | Windows | NVIDIA RTX 40/50 | Settings → Video, or Ctrl+O → Enhancement |
| **DLSS Frame Generation** – 2x, 3x/4x on RTX 50 series; gets per-pixel motion vectors from Flow's motion search (less jelly at moving edges) | Windows | NVIDIA RTX 40 series or newer | Settings → Video, or Ctrl+O → Enhancement |
| **FSR 3.1 Frame Generation** – 2x, with per-pixel motion vectors from Flow's motion search (built from the FidelityFX SDK sources on Linux) | Windows, Linux | any Vulkan GPU | Settings → Video, or Ctrl+O → Enhancement |
| **Flow Frame Generation** – chiaki-ng's own optical flow interpolation, 2x–4x | Windows, Linux, Steam Deck | any Vulkan GPU | Settings → Video, or Ctrl+O → Enhancement |
| **Generated Frame Clean-up** – keeps HUD and text sharp and in place in generated frames; optional detail pass against smeared in-between frames | all | any | Settings → Video, or Ctrl+O → Enhancement |
| **AMD RCAS sharpening** – contrast adaptive sharpening of the final picture with a strength slider; works after upscaling, RTX VSR and frame generation, also at 1:1 | all | any | Settings → Video, or Ctrl+O → Enhancement |
| **Ambient border** – fills the bars around the video (e.g. 16:9 on an ultrawide display) with a blurred copy of the picture | all | any | Settings → Video, or Ctrl+O → Enhancement |

Notes:

- RTX VSR needs `nvngx_vsr.dll` (from the NVIDIA RTX Video SDK) next to `chiaki.exe`; it is not bundled. Only the Upscale modes work with the public 1.6.0 DLL; the Artifact Reduction and Streaming modes need 1.8.2+ and are greyed out otherwise. See [RTX Video](docs/setup/rtxvideo.md).
- Frame generation needs the Vulkan renderer, Frame Delivery = Direct Mapping and a display running at least twice the stream frame rate (e.g. 120 Hz for a 60 fps stream). HDR streams are shown without it. It adds about half a stream frame of latency. See [Frame Generation](docs/setup/framegen.md).
- **Flow** needs no vendor runtime: it is built from libplacebo compute shaders (luma pyramid, coarse-to-fine block matching, per-pixel vector selection, occlusion and scene cut handling). HUD and other static overlays, including anti-aliased text, are detected and kept in place. *Flow Quality*: Performance (1/4 resolution analysis, ~0.5 ms on an RTX 5070 Ti) or Quality (1/2 resolution, a few times the GPU time). Performance is recommended.
- **NVIDIA VFG runtime**: not bundled. *NVIDIA Runtime → Download* (Settings → Video or Ctrl+O → Enhancement) or the installer's *Download the NVIDIA VFG … runtime* option fetches NVIDIA's official `nvidia-vfx` package (~420 MB, SHA-256 checked) and installs the VFG libraries and `nvngx_vsr.dll` 1.8.3, which also unlocks all RTX VSR modes. NVIDIA's license terms apply.
- **30 fps games in a 60 fps stream**: the repeated pictures are detected and dropped while frame generation is on, so it interpolates between real pictures.
- The DLSS (`nvngx_dlssg.dll`) and FidelityFX (`amd_fidelityfx_vk.dll`) runtimes ship with the Windows build; their licenses are in the `licenses` folder. On Linux, FSR 3.1 is built from the bundled FidelityFX SDK 1.1.4 sources (`third-party/ffx-sdk`).

### In-stream controls and stats

- **Enhancement panel (Ctrl+E, or Ctrl+O → Enhancement)**: change RTX VSR mode, frame generation backend / multiplier / Flow quality, RCAS strength and border fill while streaming, without leaving the stream; it shows what is active and the stream / display fps.
- **Stats overlay**: stream fps / display fps, bitrate, packet loss, plus RTX VSR / frame generation state and GPU time (e.g. `VFG 4x 6.8 ms`). Styles Mini / Compact (one line) / Default / Full and nine positions (e.g. one line at the top center); it follows the stream window instead of staying above other applications.
- **Disconnect dialog**: Cancel button and × to keep streaming instead of always closing the stream.

### Streaming and stability

- Windows tuning while streaming: qWAVE QoS on the stream sockets (audio/video traffic class), MMCSS "Games" priority for the network threads, 1 ms timer resolution, no power throttling.
- Fixed the packet loss statistic when sequence numbers wrap around.
- Fixed a swapchain rebuild loop (and log flood) while the window is minimized.
- Crash guard: if the app crashes while frame generation starts, it is turned off on the next launch.
- Upstream fixes picked up: use-after-free crashes on session teardown / controller disconnect / exit, Takion send buffer use after the receive loop ended, reorder queue not clearing dropped entries.

### Builds

- Windows (MSVC/clang-cl) build with the NVIDIA NGX and AMD FidelityFX SDKs, vcpkg binary cache (~12 min per build).
- Linux AppImage build on every push, with FSR 3.1 built from source.
- Tag-driven release workflow (`*-rtx.*` tags) publishing Windows and Linux packages with checksums.

## Building

Same as upstream chiaki-ng, plus for the Windows video features:

```
cmake -DCHIAKI_ENABLE_NGX=ON -DCHIAKI_NGX_SDK_DIR=<NVIDIA DLSS SDK> \
      -DCHIAKI_ENABLE_FFX=ON -DCHIAKI_FFX_SDK_DIR=<AMD FidelityFX SDK 1.1.x> ...
```

Flow frame generation and the ambient border need no extra SDK. On Linux, FSR 3.1 frame generation is built from `third-party/ffx-sdk` (`CHIAKI_ENABLE_FFX_SOURCE`, on by default, needs the Vulkan loader); its shader headers are pre-generated with `scripts/ffx-gen-shaders.py`. See `.github/workflows/build-windows-x86.yml` for the complete Windows build.

## Credits

- [chiaki-ng](https://github.com/streetpea/chiaki-ng) by Street Pea and contributors, based on [Chiaki](https://git.sr.ht/~thestr4ng3r/chiaki) by Florian Märkl.
- NVIDIA DLSS / NGX SDK, RTX Video SDK and Video Effects SDK, AMD FidelityFX SDK, [libplacebo](https://code.videolan.org/videolan/libplacebo).
- Ideas from [Veyra-NRVideo](https://github.com/Likely7/Veyra-NRVideo) (VFG integration, scene-cut test) and [VeyAmux](https://github.com/8BiTw0LF/VeyAmux) (FSR on Linux, frame clean-up passes).

## Disclaimer

This project is not endorsed or certified by Sony Interactive Entertainment LLC.

Chiaki is a Free and Open Source Software Client for PlayStation 4 and PlayStation 5 Remote Play
for Linux, FreeBSD, OpenBSD, Android, macOS, Windows, Nintendo Switch and potentially even more platforms.
