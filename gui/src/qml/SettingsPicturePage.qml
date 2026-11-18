import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import org.streetpea.chiaking

import "controls" as C

C.SettingsPage {
    id: page
    title: qsTr("Picture quality")
    subtitle: qsTr("How the stream is decoded, upscaled and fitted to your screen.")
    firstFocusItem: rtxVsrCombo.visible ? rtxVsrCombo : renderPresetCombo

    C.Card {
        title: qsTr("Upscaling and sharpness")

        C.SettingRow {
            divider: false
            visible: Chiaki.settings.rtxVsrSupported
            label: qsTr("RTX Video Super Resolution")
            description: qsTr("NVIDIA's AI upscaler. Streaming modes also clean up compression artifacts. Installed: %1").arg(Chiaki.settings.rtxVsrDllVersion.length ? Chiaki.settings.rtxVsrDllVersion : qsTr("not found"))
            defaultText: qsTr("Default: Off")

            C.ComboBox {
                id: rtxVsrCombo
                firstInFocusChain: true
                width: 420
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
                ToolTip.text: qsTr("NVIDIA RTX Video on SDR streams (RTX 20 series or newer, Vulkan renderer). Needs nvngx_vsr.dll next to chiaki.exe.\nUpscale: sharper picture when the window is larger than the stream.\nArtifact Reduction: removes compression blocking and banding at stream resolution.\nStreaming: upscale plus artifact cleanup for compressed streams.\nArtifact Reduction and Streaming need nvngx_vsr.dll 1.8.2 or newer (NVIDIA VFX SDK 1.2/1.3).")
            }
        }

        C.SettingRow {
            visible: Chiaki.settings.rtxVsrSupported || Chiaki.settings.frameGenSupported
            label: qsTr("Output resolution")
            description: qsTr("Resolution RTX Video upscales to and frame generation runs at, then scaled to the screen. Lower saves GPU time, higher supersamples.")
            defaultText: qsTr("Default: Auto")

            C.ComboBox {
                width: 420
                readonly property var heights: [0, 1080, 1440, 1620, 2160]
                model: [qsTr("Auto (match the screen)"), "1080p", "1440p", "1620p", qsTr("2160p (4K)")]
                currentIndex: Math.max(0, heights.indexOf(Chiaki.settings.processingHeight))
                onActivated: index => Chiaki.settings.processingHeight = heights[index]
            }
        }

        C.SettingRow {
            label: qsTr("Sharpening (AMD RCAS)")
            description: qsTr("Sharpens the final picture after upscaling and frame generation. Lower it if edges look harsh.")
            defaultText: qsTr("Default: Off")

            RowLayout {
                spacing: 16
                C.Slider {
                    Layout.preferredWidth: 300
                    from: 0
                    to: 100
                    stepSize: 5
                    value: Chiaki.settings.rcasStrength
                    onMoved: Chiaki.settings.rcasStrength = value
                }
                Label {
                    Layout.preferredWidth: 70
                    text: Chiaki.settings.rcasStrength > 0 ? qsTr("%1%").arg(Chiaki.settings.rcasStrength.toFixed(0)) : qsTr("Off")
                    color: C.Theme.textSecondary
                }
            }
        }

        C.SettingRow {
            label: qsTr("Render preset")
            description: qsTr("libplacebo scaling and debanding quality.")
            defaultText: qsTr("Default: High Quality")

            C.ComboBox {
                id: renderPresetCombo
                width: 420
                model: [qsTr("Fast"), qsTr("Default"), qsTr("High Quality"), qsTr("High Quality + Spatial Upscaling"), qsTr("High Quality + Advanced Spatial Upscaling"), qsTr("Custom")]
                currentIndex: Chiaki.settings.videoPreset
                onActivated: (index) => {
                    Chiaki.settings.videoPreset = index;
                    switch (index) {
                    case 0: Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.Fast; break;
                    case 1: Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.Default; break;
                    case 2: Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.HighQuality; break;
                    case 3: Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.HighQualitySpatial; break;
                    case 4: Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.HighQualityAdvancedSpatial; break;
                    case 5: Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.Custom; break;
                    }
                }
            }
        }

        C.SettingRow {
            visible: Chiaki.window.videoPreset == ChiakiWindow.VideoPreset.Custom
            label: qsTr("Custom renderer settings")
            description: qsTr("Every libplacebo option of the Custom preset.")

            C.Button {
                text: qsTr("Open")
                onClicked: root.showPlaceboSettingsDialog()
            }
        }
    }

    C.Card {
        title: qsTr("Window")

        C.SettingRow {
            divider: false
            label: qsTr("Window type")
            defaultText: qsTr("Default: Fullscreen")

            C.ComboBox {
                id: windowTypeCombo
                width: 420
                popup.width: 520
                model: [qsTr("Stream Resolution"), qsTr("Custom Resolution"), qsTr("Adjust Resolution Manually"), qsTr("Fullscreen"), qsTr("Zoom [adjust zoom using slider in stream menu]"), qsTr("Stretch")]
                currentIndex: Chiaki.settings.windowType
                onActivated: (index) => Chiaki.settings.windowType = index;
            }
        }

        C.SettingRow {
            visible: Chiaki.settings.windowType == 1
            label: qsTr("Custom resolution")
            description: qsTr("Width × height of the window in pixels.")
            defaultText: qsTr("Default: 1920 × 1080")

            RowLayout {
                spacing: 12
                C.TextField {
                    id: customResolutionWidth
                    Layout.preferredWidth: 150
                    text: Chiaki.settings.customResolutionWidth
                    Material.accent: text && !validate() ? Material.Red : undefined
                    onEditingFinished: {
                        if (validate()) {
                            Chiaki.settings.customResolutionWidth = parseInt(text);
                        } else {
                            Chiaki.settings.customResolutionWidth = 0;
                            text = "";
                        }
                    }
                    function validate() {
                        var num = parseInt(text);
                        return num >= 0 && num <= 9999;
                    }
                }
                Label {
                    text: "×"
                    color: C.Theme.textSecondary
                }
                C.TextField {
                    id: customResolutionHeight
                    Layout.preferredWidth: 150
                    text: Chiaki.settings.customResolutionHeight
                    Material.accent: text && !validate() ? Material.Red : undefined
                    onEditingFinished: {
                        if (validate()) {
                            Chiaki.settings.customResolutionHeight = parseInt(text);
                        } else {
                            Chiaki.settings.customResolutionHeight = 0;
                            text = "";
                        }
                    }
                    function validate() {
                        var num = parseInt(text);
                        return num >= 0 && num <= 9999;
                    }
                }
            }
        }

        C.SettingRow {
            visible: Chiaki.settings.ambientBorderSupported
            label: qsTr("Border fill")
            description: qsTr("Fills the bars around the video (a 16:9 stream on an ultrawide display) with a blurred copy of the picture instead of black.")
            defaultText: qsTr("Default: Black")

            C.Segmented {
                model: [qsTr("Black"), qsTr("Ambient")]
                currentIndex: Chiaki.settings.ambientBorder ? 1 : 0
                onActivated: index => Chiaki.settings.ambientBorder = index === 1
            }
        }

        C.SettingRow {
            label: qsTr("Fullscreen on double-click")
            defaultText: qsTr("Default: Off")

            C.Switch {
                checked: Chiaki.settings.fullscreenDoubleClick
                onToggled: Chiaki.settings.fullscreenDoubleClick = checked
            }
        }

        C.SettingRow {
            label: qsTr("Hide cursor during stream")
            defaultText: qsTr("Default: On")

            C.Switch {
                checked: Chiaki.settings.hideCursor
                onToggled: Chiaki.settings.hideCursor = checked
            }
        }

        C.SettingRow {
            label: qsTr("Display settings")
            description: qsTr("HDR, color and display mapping.")

            C.Button {
                text: qsTr("Open")
                onClicked: root.showDisplaySettingsDialog()
            }
        }
    }

    C.Card {
        title: qsTr("Decoder and renderer")

        C.SettingRow {
            divider: false
            label: qsTr("Hardware decoder")
            description: qsTr("Zero-copy hands decoded frames straight to the renderer.")
            defaultText: qsTr("Default: Auto")

            RowLayout {
                spacing: 16
                C.ComboBox {
                    id: hwDecoderCombo
                    Layout.preferredWidth: 260
                    model: Chiaki.settings.availableDecoders
                    currentIndex: Math.max(0, model.indexOf(Chiaki.settings.decoder))
                    onActivated: (index) => Chiaki.settings.decoder = index ? model[index] : ""
                }
                Label {
                    text: qsTr("Zero-copy")
                    color: C.Theme.textSecondary
                }
                C.Switch {
                    checked: Chiaki.settings.useZeroCopy
                    onToggled: Chiaki.settings.useZeroCopy = checked
                }
            }
        }

        C.SettingRow {
            label: qsTr("Renderer")
            description: qsTr("RTX Video, frame generation and Reflex need Vulkan. Changing it restarts chiaki-ng.")
            defaultText: qsTr("Default: Vulkan")

            C.Segmented {
                model: [qsTr("Vulkan"), qsTr("OpenGL")]
                currentIndex: Chiaki.settings.rendererBackend
                onActivated: (index) => {
                    if (index === Chiaki.settings.rendererBackend)
                        return;
                    Chiaki.settings.rendererBackend = index;
                    if (Chiaki.settings.restartApplication())
                        Qt.quit();
                }
            }
        }

        C.SettingRow {
            visible: Chiaki.settings.rendererBackend == 0
            label: qsTr("Vulkan deferred swap")
            defaultText: qsTr("Default: Off")

            C.Switch {
                lastInFocusChain: true
                checked: Chiaki.settings.vulkanDeferredSwap
                onToggled: Chiaki.settings.vulkanDeferredSwap = checked
            }
        }
    }
}
