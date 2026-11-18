import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import org.streetpea.chiaking

import "controls" as C

// Video enhancement settings that apply while streaming: opened from the
// stream menu (Ctrl+O) like the Display and Placebo panels. Every change
// takes effect on the next rendered frame.
DialogView {
    id: dialog
    property alias gridHeight: enhancementGrid.height
    title: qsTr("Enhancement")
    header: qsTr("* Applies immediately, defaults in () to the right")
    buttonVisible: false

    Item {
        GridLayout {
            id: enhancementGrid
            anchors {
                top: parent.top
                horizontalCenter: parent.horizontalCenter
                topMargin: 20
            }
            columns: 3
            rowSpacing: 10
            columnSpacing: 20

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Active:")
            }

            Label {
                Layout.preferredWidth: 600
                text: Chiaki.window.videoEnhancementStatus.length ? Chiaki.window.videoEnhancementStatus : qsTr("None")
                color: Material.accent
                font.bold: true
                elide: Text.ElideRight
            }

            Label {
                text: Chiaki.session ? qsTr("%1 / %2 fps").arg(Chiaki.window.streamFps.toFixed(0)).arg(Chiaki.window.displayFps.toFixed(0)) : ""
                color: "#ffe082"
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.rtxVsrSupported
                text: qsTr("RTX Video Super Resolution:")
            }

            C.ComboBox {
                id: rtxVsrCombo
                firstInFocusChain: true
                Layout.preferredWidth: 600
                visible: Chiaki.settings.rtxVsrSupported
                // NGX VSR.QualityLevel for each entry
                readonly property var modes: [0, 1, 2, 3, 4, 8, 9, 10, 11, 21, 23]
                model: [qsTr("Off"),
                    qsTr("Upscale Low"), qsTr("Upscale Medium"), qsTr("Upscale High"), qsTr("Upscale Ultra"),
                    qsTr("Artifact Reduction Low"), qsTr("Artifact Reduction Medium"),
                    qsTr("Artifact Reduction High"), qsTr("Artifact Reduction Ultra"),
                    qsTr("Streaming Medium (RTX 30+)"), qsTr("Streaming Ultra (RTX 30+)")]
                currentIndex: Math.max(0, modes.indexOf(Chiaki.settings.rtxVsrQuality))
                // Artifact Reduction / Streaming need nvngx_vsr.dll 1.8.2+.
                function modeAvailable(index) {
                    return modes[index] <= 4 || Chiaki.settings.rtxVsrExtendedModes;
                }
                onActivated: index => {
                    if (modeAvailable(index))
                        Chiaki.settings.rtxVsrQuality = modes[index];
                    else
                        currentIndex = Qt.binding(() => Math.max(0, modes.indexOf(Chiaki.settings.rtxVsrQuality)));
                }
                delegate: ItemDelegate {
                    required property var modelData
                    required property int index
                    width: ListView.view ? ListView.view.width : implicitWidth
                    enabled: rtxVsrCombo.modeAvailable(index)
                    text: enabled ? modelData : qsTr("%1 (needs nvngx_vsr.dll 1.8.2+)").arg(modelData)
                    highlighted: rtxVsrCombo.highlightedIndex === index
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Installed nvngx_vsr.dll: %1").arg(Chiaki.settings.rtxVsrDllVersion.length ? Chiaki.settings.rtxVsrDllVersion : qsTr("not found"))
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.rtxVsrSupported
                text: qsTr("(Off)")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.rtxVsrSupported || Chiaki.settings.frameGenSupported
                text: qsTr("Output Resolution:")
            }

            C.ComboBox {
                Layout.preferredWidth: 600
                visible: Chiaki.settings.rtxVsrSupported || Chiaki.settings.frameGenSupported
                readonly property var heights: [0, 1080, 1440, 1620, 2160]
                model: [qsTr("Auto (match the screen)"), "1080p", "1440p", "1620p", qsTr("2160p (4K)")]
                currentIndex: Math.max(0, heights.indexOf(Chiaki.settings.processingHeight))
                onActivated: index => Chiaki.settings.processingHeight = heights[index]
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Resolution RTX Video upscales to and frame generation works at; then scaled to the screen.")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.rtxVsrSupported || Chiaki.settings.frameGenSupported
                text: qsTr("(Auto)")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenSupported
                text: qsTr("Frame Generation:")
            }

            C.ComboBox {
                firstInFocusChain: !Chiaki.settings.rtxVsrSupported
                Layout.preferredWidth: 600
                visible: Chiaki.settings.frameGenSupported
                enabled: Chiaki.settings.directFrameMapping
                model: [qsTr("Off"),
                    Chiaki.settings.frameGenDlssSupported ? qsTr("NVIDIA DLSS Frame Generation") : qsTr("NVIDIA DLSS Frame Generation (not in this build)"),
                    Chiaki.settings.frameGenFsrSupported ? qsTr("AMD FSR 3.1 Frame Generation") : qsTr("AMD FSR 3.1 Frame Generation (not in this build)"),
                    qsTr("Flow Frame Generation (any GPU)"),
                            Chiaki.settings.frameGenVfgSupported ? (Chiaki.settings.frameGenVfgRuntime.length ? qsTr("NVIDIA VFG (Video Effects SDK)") : qsTr("NVIDIA VFG (runtime not installed)")) : qsTr("NVIDIA VFG (Windows only)")]
                currentIndex: Chiaki.settings.frameGenBackend
                onActivated: index => Chiaki.settings.frameGenBackend = index
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Needs a display running at least twice the stream frame rate. HDR streams are shown without frame generation.")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenSupported
                text: Chiaki.settings.directFrameMapping ? qsTr("(Off)") : qsTr("(needs Direct Mapping)")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenFlowSupported
                text: qsTr("Flow Quality:")
            }

            C.ComboBox {
                Layout.preferredWidth: 600
                visible: Chiaki.settings.frameGenFlowSupported
                enabled: Chiaki.settings.frameGenBackend === 3 || (Chiaki.settings.frameGenBackend === 1 && Chiaki.settings.frameGenDlssMotion > 0) || (Chiaki.settings.frameGenBackend > 0 && Chiaki.settings.frameGenCleanup > 0) || Chiaki.settings.frameGenBackend === 2
                model: [qsTr("Performance (1/4 resolution motion analysis)"), qsTr("Quality (1/2 resolution motion analysis)")]
                currentIndex: Chiaki.settings.frameGenFlowHighQuality ? 1 : 0
                onActivated: index => Chiaki.settings.frameGenFlowHighQuality = index === 1
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenFlowSupported
                text: qsTr("(Performance)")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenDlssSupported
                text: qsTr("DLSS Motion Vectors:")
            }

            C.ComboBox {
                Layout.preferredWidth: 600
                visible: Chiaki.settings.frameGenDlssSupported
                enabled: Chiaki.settings.frameGenBackend === 1
                model: [qsTr("Zero motion"), qsTr("Flow motion search (sharp edges, less jelly)"), qsTr("Flow motion search, reversed (for comparison)")]
                currentIndex: Chiaki.settings.frameGenDlssMotion
                onActivated: index => Chiaki.settings.frameGenDlssMotion = index
                ToolTip.visible: hovered
                ToolTip.text: qsTr("A video stream has no motion vectors. With Flow, DLSS Frame Generation gets per-pixel motion from chiaki-ng's motion search (Flow Quality applies, about 0.5 ms at Performance); with zero motion it relies on its own optical flow only, which drags the background along moving edges.")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenDlssSupported
                text: qsTr("(Flow)")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenVfgSupported
                text: qsTr("NVIDIA VFG Quality:")
            }

            C.ComboBox {
                Layout.preferredWidth: 600
                visible: Chiaki.settings.frameGenVfgSupported
                enabled: Chiaki.settings.frameGenBackend === 4
                model: [qsTr("Low (fastest)"), qsTr("Medium"), qsTr("High (best, heaviest)")]
                currentIndex: Chiaki.settings.frameGenVfgMode
                onActivated: index => Chiaki.settings.frameGenVfgMode = index
                ToolTip.visible: hovered
                ToolTip.text: qsTr("NVIDIA Video Frame Generation: a video-trained model that needs no motion vectors (RTX 40/50). Its NVIDIA runtime is not bundled: install it with the NVIDIA Runtime button below (or the installer option). High costs tens of milliseconds per frame pair at 1440p, so it suits 30 fps sources. Found: %1").arg(Chiaki.settings.frameGenVfgRuntime.length ? Chiaki.settings.frameGenVfgRuntime : qsTr("not found"))
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenVfgSupported
                text: qsTr("(High)")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenVfgSupported && Chiaki.settings.frameGenDlssSupported
                text: qsTr("NVIDIA VFG Second Stage:")
            }

            C.ComboBox {
                Layout.preferredWidth: 600
                visible: Chiaki.settings.frameGenVfgSupported && Chiaki.settings.frameGenDlssSupported
                enabled: Chiaki.settings.frameGenBackend === 4
                model: [qsTr("Off"), qsTr("DLSS Frame Generation 2x (VFG 2x + DLSS 2x = 4x)")]
                currentIndex: Chiaki.settings.frameGenVfgChain ? 1 : 0
                onActivated: index => Chiaki.settings.frameGenVfgChain = index === 1
                ToolTip.visible: hovered
                ToolTip.text: qsTr("VFG makes one frame between the stream frames, then DLSS Frame Generation (RTX 40/50) doubles that sequence again: 4 frames per stream frame for much less GPU time than VFG 4x, at the cost of some DLSS artifacts on UI. Needs a display at 4x the stream frame rate (e.g. 120 Hz for 30 fps); the multiplier setting is ignored.")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenVfgSupported && Chiaki.settings.frameGenDlssSupported
                text: qsTr("(Off)")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("NVIDIA Reflex:")
            }

            C.ComboBox {
                Layout.preferredWidth: 600
                model: [qsTr("Off"), qsTr("On (low latency)")]
                currentIndex: Chiaki.settings.nvidiaReflex ? 1 : 0
                onActivated: index => Chiaki.settings.nvidiaReflex = index === 1
                ToolTip.visible: hovered
                ToolTip.text: qsTr("NVIDIA Reflex low latency mode for the presentation (Vulkan, NVIDIA GPUs): frames do not queue up for the GPU. Applies from the next stream.")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(On)")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenVfgSupported
                text: qsTr("NVIDIA Runtime:")
            }

            C.Button {
                Layout.preferredWidth: 600
                visible: Chiaki.settings.frameGenVfgSupported
                enabled: Chiaki.settings.vfgRuntimeDownloadState !== 1
                text: Chiaki.settings.vfgRuntimeDownloadState === 1 ? qsTr("Downloading... (see the console window)")
                    : Chiaki.settings.vfgRuntimeDownloadState === 2 ? qsTr("Installed - restart chiaki-ng")
                    : Chiaki.settings.frameGenVfgRuntime.length ? qsTr("Re-download NVIDIA VFG + RTX VSR runtime")
                    : qsTr("Download NVIDIA VFG + RTX VSR runtime (~420 MB)")
                onClicked: {
                    if (Chiaki.settings.vfgRuntimeDownloadState === 2)
                        Chiaki.settings.restartApplication();
                    else
                        Chiaki.settings.downloadVfgRuntime();
                }
                Material.roundedScale: Material.SmallScale
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Downloads NVIDIA's official Video Effects runtime (nvidia-vfx package from pypi.nvidia.com, checked by SHA-256) and installs the NVIDIA VFG libraries and nvngx_vsr.dll 1.8.3 (all RTX VSR modes) for this user. NVIDIA's license terms apply. Found: %1").arg(Chiaki.settings.frameGenVfgRuntime.length ? Chiaki.settings.frameGenVfgRuntime : qsTr("not found"))
            }

            Label {
                visible: Chiaki.settings.frameGenVfgSupported
                text: Chiaki.settings.vfgRuntimeDownloadState === 3 ? qsTr("Failed, see the console") : Chiaki.settings.frameGenVfgRuntime.length ? qsTr("Found") : qsTr("Not found")
                color: Chiaki.settings.vfgRuntimeDownloadState === 3 ? "#ef9a9a" : Chiaki.settings.frameGenVfgRuntime.length ? "#a5d6a7" : "#ffe082"
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenSupported
                text: qsTr("Generated Frame Clean-up:")
            }

            C.ComboBox {
                Layout.preferredWidth: 600
                visible: Chiaki.settings.frameGenSupported
                enabled: Chiaki.settings.frameGenBackend > 0
                model: [qsTr("Off"), qsTr("Protect HUD and text"), qsTr("Protect HUD and text + keep detail (less flicker)")]
                currentIndex: Chiaki.settings.frameGenCleanup
                onActivated: index => Chiaki.settings.frameGenCleanup = index
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Fixes the generated frames of every backend. HUD and text that stay still are taken from the real frame, so they are not dragged along; where the two frames do not line up (fine detail, foliage, edges of moving objects) the smeared blend is replaced by a sharp pixel, which removes the sharp/blurry flicker while panning. Uses Flow's motion search (Flow Quality applies, about 0.5 ms).")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenSupported
                text: qsTr("(+ detail)")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenSupported
                text: qsTr("Frame Generation Multiplier:")
            }

            C.ComboBox {
                Layout.preferredWidth: 600
                visible: Chiaki.settings.frameGenSupported
                enabled: Chiaki.settings.frameGenBackend === 1 || Chiaki.settings.frameGenBackend === 3 || Chiaki.settings.frameGenBackend === 4
                model: [qsTr("2x"), qsTr("3x (DLSS on RTX 50 series, Flow, VFG)"), qsTr("4x (DLSS on RTX 50 series, Flow, VFG)")]
                currentIndex: (Chiaki.settings.frameGenBackend === 1 || Chiaki.settings.frameGenBackend === 3 || Chiaki.settings.frameGenBackend === 4) ? Chiaki.settings.frameGenMultiplier - 2 : 0
                onActivated: index => Chiaki.settings.frameGenMultiplier = index + 2
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Output frames per stream frame. FSR always uses 2x; the display refresh rate also limits it.")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.frameGenSupported
                text: qsTr("(2x)")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Sharpening (AMD RCAS):")
            }

            C.Slider {
                Layout.preferredWidth: 400
                from: 0
                to: 100
                stepSize: 5
                value: Chiaki.settings.rcasStrength
                onMoved: Chiaki.settings.rcasStrength = value
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Contrast adaptive sharpening of the final picture. 0 = off. Lower it if edges look harsh or compression noise shows.")
                Label {
                    anchors {
                        left: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 10
                    }
                    text: parent.value > 0 ? qsTr("%1%").arg(parent.value.toFixed(0)) : qsTr("Off")
                }
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(Off)")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Stats Overlay:")
            }

            C.ComboBox {
                Layout.preferredWidth: 600
                model: [qsTr("Off"), qsTr("Mini"), qsTr("Compact"), qsTr("Default"), qsTr("Full")]
                currentIndex: Chiaki.settings.showStreamStats ? Chiaki.settings.streamStatsStyle + 1 : 0
                onActivated: index => {
                    Chiaki.settings.showStreamStats = index > 0;
                    if (index > 0)
                        Chiaki.settings.streamStatsStyle = index - 1;
                }
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(Off)")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Stats Position:")
            }

            C.ComboBox {
                Layout.preferredWidth: 600
                enabled: Chiaki.settings.showStreamStats
                lastInFocusChain: !Chiaki.settings.ambientBorderSupported
                model: [qsTr("Top left"), qsTr("Top center"), qsTr("Top right"),
                    qsTr("Middle left"), qsTr("Middle center"), qsTr("Middle right"),
                    qsTr("Bottom left"), qsTr("Bottom center"), qsTr("Bottom right")]
                currentIndex: Chiaki.settings.streamStatsPosition
                onActivated: index => Chiaki.settings.streamStatsPosition = index
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(Middle right)")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.ambientBorderSupported
                text: qsTr("Border Fill:")
            }

            C.ComboBox {
                Layout.preferredWidth: 600
                visible: Chiaki.settings.ambientBorderSupported
                lastInFocusChain: true
                model: [qsTr("Black"), qsTr("Ambient (blurred picture)")]
                currentIndex: Chiaki.settings.ambientBorder ? 1 : 0
                onActivated: index => Chiaki.settings.ambientBorder = index === 1
            }

            Label {
                Layout.alignment: Qt.AlignRight
                visible: Chiaki.settings.ambientBorderSupported
                text: qsTr("(Black)")
            }
        }
    }
}
