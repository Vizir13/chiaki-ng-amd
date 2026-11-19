# Frame Generation

chiaki-ng can generate extra frames between the frames sent by the console
(for example 60 fps stream → 120 fps on a 120 Hz display). Three backends:

| Backend | Platforms | GPU | Multiplier |
|---|---|---|---|
| NVIDIA DLSS Frame Generation | Windows (VC build) | GeForce RTX 40 series or newer | 2x (3x / 4x on RTX 50 series) |
| AMD FSR 3.1 Frame Generation | Windows (VC build) | any GPU with Vulkan | 2x |
| Flow Frame Generation | Windows, Linux, Steam Deck | any GPU with Vulkan | 2x / 3x / 4x |

The DLSS and FSR runtimes (`nvngx_dlssg.dll`, `amd_fidelityfx_vk.dll`) ship
next to `chiaki.exe`; nothing extra has to be downloaded. Flow is chiaki-ng's
own interpolation and needs no runtime at all.

### Flow

Flow is built from libplacebo compute shaders, so it runs wherever the Vulkan
renderer runs. For each stream frame it

1. builds a luma pyramid (1/4 resolution and coarser),
2. estimates motion coarse to fine with symmetric block matching (seeded by
   the coarser level, neighbouring blocks and the previous frame's motion),
3. removes outliers and checks for scene cuts,
4. warps both stream frames along the motion to each in-between time and
   blends them, picking per pixel the vector that makes both frames agree.
   Where nothing agrees (areas being covered or revealed) it uses the
   temporally nearer frame; across a scene cut it repeats the nearer frame.

**Flow Quality** (Settings → Video): *Performance* analyses motion at 1/4
resolution (≈0.5 ms on an RTX 5070 Ti at 1440p), *Quality* at 1/2 resolution
(roughly 2–4x the analysis time; finer motion around small details). Use
Performance on handhelds such as the Steam Deck.

Static overlays (HUD, subtitles) are detected per pixel and kept in place,
including the anti-aliased edges and outlines of HUD text (a 1/2 resolution
mask of static pixels marks text areas);
next to them the scene's motion is looked up up to ~20 px away so the
background behind HUD text does not smear.

The overlay shows `Flow FG 2x` (`Flow FG HQ 2x` in Quality mode) and its GPU
time. Fast motion around object
edges can show some smearing in the covered/revealed areas.

## Requirements

- Renderer Backend: **Vulkan**
- Settings → Video → Frame Mapping: **Direct Mapping** (the default)
- An SDR stream (HDR streams are shown without frame generation)
- A display refresh rate of at least multiplier × stream fps
  (the multiplier is reduced automatically otherwise)

## Usage

Settings → Video → **Frame Generation**: pick DLSS, FSR or Flow, then the
**Frame Generation Multiplier** (DLSS and Flow). Works together with
[RTX Video Super Resolution](rtxvideo.md): frames are upscaled first, then
generated.

### NVIDIA VFG (Windows, experimental)

**NVIDIA VFG** uses the *Video Frame Generation* effect of the NVIDIA Video
Effects SDK: a model trained on video that only needs the two pictures, so it
has no motion-vector problem at all. It needs an NVIDIA RTX GPU (40/50 series;
30 series is not supported by NVIDIA) and is heavy: High costs tens of
milliseconds per frame pair at 1440p, which fits a 30 fps source (33 ms per
frame) better than 60 fps. Pick Low or Medium in **NVIDIA VFG Quality** if the
frame rate drops.

The NVIDIA runtime is not bundled with chiaki-ng; it is downloaded from NVIDIA
on request:

- **Installer:** tick *Download the NVIDIA VFG frame generation and RTX VSR
  runtime* (installs into `<chiaki folder>\vfx`).
- **Any time (also the portable zip):** Settings → Video (or Ctrl+O →
  Enhancement) → **NVIDIA Runtime** → *Download*, then restart chiaki-ng
  (installs into `%LOCALAPPDATA%\Chiaki\Chiaki\vfx`).
- **By hand:** run `install-nvidia-vfx.ps1` from the chiaki-ng folder
  (`-Destination <folder>` to choose where).

The script downloads NVIDIA's official `nvidia-vfx` 0.2.0.0 package from
`pypi.nvidia.com` (about 420 MB, checked against its SHA-256) and keeps only
`NVVideoEffects.dll`, `NVCVImage.dll`, `nvVFXVideoFrameGeneration.dll`,
`cudart64_12.dll`, `nvngx_vsr.dll` 1.8.3 and NVIDIA's license files (NVIDIA's
terms apply). When that `nvngx_vsr.dll` is newer than one next to `chiaki.exe`,
RTX VSR uses it too, which enables the Artifact Reduction and Streaming modes.

chiaki-ng looks for the runtime in `CHIAKI_VFX_PATH`, `<chiaki folder>\vfx`,
`%LOCALAPPDATA%\Chiaki\Chiaki\vfx`, `NV_VIDEO_EFFECTS_PATH` and
`C:\Program Files\NVIDIA Corporation\NVIDIA Video Effects`, in that order. The
log shows `[framegen] VFG: Video Effects SDK x.y.z from ...` and
`NVIDIA VFG ready: WxH, quality ...`, or what is missing.

### DLSS motion vectors

DLSS Frame Generation is made for games, which hand it per-pixel motion
vectors and depth. A video stream has neither; with zero motion DLSS relies on
its own optical flow alone and drags the background along the edges of moving
characters ("jelly"). By default chiaki-ng now gives it per-pixel motion from
Flow's motion search, measured at the current frame: every pixel takes the
vector of the nearby block whose source in the previous frame matches it, so
edges stay sharp and HUD elements get zero motion. **DLSS Motion Vectors** in
Settings → Video (and Ctrl+O → Enhancement) switches between *Flow* and *Zero
motion* for comparison; **Flow Quality** sets the analysis resolution
(Performance ≈0.5 ms). The log shows `DLSS motion vectors: Flow`.

### FSR 3.1 on Linux

Linux builds (AppImage, Flatpak, Steam Deck) build FSR 3.1 frame generation
from the AMD FidelityFX SDK 1.1.4 sources (`third-party/ffx-sdk`): the optical
flow and frame interpolation passes run directly in chiaki-ng's Vulkan
renderer, on any GPU, 2x. Windows builds use AMD's signed DLL. Both get
per-pixel motion from Flow's motion search: with zero motion FSR's own
optical flow did worse than a plain blend on video in tests.

### Generated frame clean-up

**Generated Frame Clean-up** (Settings → Video, or Ctrl+O → Enhancement)
post-processes the frames of every backend, after an idea of VeyAmux (the
Linux port of Veyra):

- **Protect HUD and text:** pixels with texture that stayed unchanged for 6
  real frames count as static (HUD, text, mini-map, logos). They get zero
  motion before DLSS/FSR run and the real pixel in the generated frames, and
  moving background next to them is taken from the frame where it is not
  covered, so text is not dragged along.
- **+ keep detail** (default): where the two frames, moved along the motion,
  do not agree (fine detail, foliage, the edge of a moving character), the
  smeared blend is replaced by the sharper of the two samples. Sharp real
  frames alternating with smeared generated ones is what reads as flicker
  while panning. Not applied to Flow, whose synthesis already does this.

Both use Flow's motion search (Flow Quality applies, about 0.5 ms at
Performance). FSR Frame Generation now also gets these motion vectors
instead of zero motion.

### 30 fps games in a 60 fps stream

The console keeps sending 60 frames per second even when the game renders
only 30, so every picture arrives twice. chiaki-ng compares every frame
with the last new picture on a small GPU thumbnail (a repeat is the same
picture, a new one in motion changes a large part of it; the size of the
encoded frame only decides near-still frames; at most two frames in a row
are dropped, so a still scene keeps updating), drops the repeats and
generates frames from the real 30 fps pictures; the multiplier doubles so the
output frame rate stays what you picked (2x of a 60 fps stream = 4x of the
30 fps pictures = 120 fps). The game's frame rate is the average distance
between new pictures, so 40, 24 and 20 fps games work too and the game's own
pacing hiccups do not switch modes. The stats overlay then shows
*(half-rate source)* for 30 fps and the log
`Source cadence: a new picture every 2 stream frames`, plus a summary of the
detection every 10 s (`Repeat detection: ...`; *shown as still scene*
counts frames kept only because they would have been a third drop in a
row). Games below 20 fps are not handled. It switches back within
about 0.2 s when the game runs at 60 again.
Pick a multiplier whose output divides your refresh rate evenly (on 240 Hz:
2x or 4x of 60, not 3x).

## Latency

Frame generation has to wait for the next stream frame before it can show the
frames in between, so it adds about half a stream frame interval (≈8 ms at
60 fps) plus a few milliseconds of GPU time. Turn it off for latency
sensitive games.

## Limitations

The stream carries no motion vectors or depth, so both backends get zero
motion vectors and a constant depth and rely on their own optical flow. Fast
motion and on-screen UI can show artifacts.

## If it crashes

Frame generation calls into the NVIDIA and AMD runtimes. A crash inside them is
caught where possible and only turns frame generation off for the session. If
chiaki-ng still closes while frame generation starts, it is switched off
automatically on the next launch (the log says so); turn it back on in the
settings once you want to retry.

The first frames log each step (`[framegen] frame N: ...`), which shows where
it stopped.

## Log messages

- `[framegen] ... initialized` and `Frame generation active: ... Nx` → working.
- `DLSS Frame Generation is not available on this GPU/driver` → RTX 40+ GPU and a current driver are needed.
- `Could not load ...amd_fidelityfx_vk.dll` → the DLL is missing next to `chiaki.exe`.
- `Frame generation requires Direct Mapping` → switch Frame Mapping to Direct Mapping.
