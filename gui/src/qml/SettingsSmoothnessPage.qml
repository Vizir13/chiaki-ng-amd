import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import org.streetpea.chiaking

import "controls" as C
import "controls/Presets.js" as Presets

C.SettingsPage {
    id: page
    title: qsTr("Smoothness")
    subtitle: qsTr("Frame generation turns a 30 fps stream into 60 fps or more. Pick a preset, or fine-tune below.")
    firstFocusItem: presetRow

    readonly property var s: Chiaki.settings
    readonly property bool fgUsable: s.frameGenSupported && s.directFrameMapping
    readonly property bool multiplierBackend: s.frameGenBackend === 1 || s.frameGenBackend === 3 || s.frameGenBackend === 4
    readonly property bool chainActive: s.frameGenBackend === 4 && s.frameGenVfgChain && s.frameGenDlssSupported

    // Generator choices: backend index and whether it can be picked here
    readonly property var generators: {
        let g = [{ label: qsTr("Off"), backend: 0, usable: true }];
        if (s.frameGenFlowSupported)
            g.push({ label: qsTr("Flow"), backend: 3, usable: true });
        if (s.frameGenFsrSupported)
            g.push({ label: qsTr("FSR 3.1"), backend: 2, usable: true });
        if (s.frameGenDlssSupported)
            g.push({ label: qsTr("DLSS FG"), backend: 1, usable: true });
        if (s.frameGenVfgSupported)
            g.push({ label: qsTr("NVIDIA VFG"), backend: 4, usable: s.frameGenVfgRuntime.length > 0 || s.frameGenBackend === 4 });
        return g;
    }

    function applyPreset(preset) {
        // Also keeps the current mix as "Custom" (Main.qml)
        root.applySmoothnessPreset(preset.id);
    }

    // Presets: one focus stop, Left/Right choose, Return applies
    FocusScope {
        id: presetRow
        Layout.fillWidth: true
        implicitHeight: presetGrid.implicitHeight
        activeFocusOnTab: true
        property int focusIndex: 0
        readonly property var presets: Presets.list(page.s)
        readonly property string active: Presets.current(page.s)

        onActiveFocusChanged: {
            if (activeFocus) {
                for (let i = 0; i < presets.length; ++i) {
                    if (presets[i].id === active)
                        focusIndex = i;
                }
            }
        }

        Keys.onPressed: (event) => {
            switch (event.key) {
            case Qt.Key_Left:
                focusIndex = Math.max(0, focusIndex - 1);
                event.accepted = true;
                break;
            case Qt.Key_Right:
                focusIndex = Math.min(presets.length - 1, focusIndex + 1);
                event.accepted = true;
                break;
            case Qt.Key_Return:
            case Qt.Key_Enter:
                page.applyPreset(presets[focusIndex]);
                event.accepted = true;
                break;
            case Qt.Key_Down: {
                let item = nextItemInFocusChain();
                if (item)
                    item.forceActiveFocus(Qt.TabFocusReason);
                event.accepted = true;
                break;
            }
            case Qt.Key_Up:
                event.accepted = true;
                break;
            }
        }

        GridLayout {
            id: presetGrid
            anchors { left: parent.left; right: parent.right }
            columns: 4
            columnSpacing: 14

            Repeater {
                model: presetRow.presets
                delegate: Rectangle {
                    id: presetCard
                    required property var modelData
                    required property int index
                    readonly property bool selected: modelData.id === presetRow.active
                    readonly property bool focused: presetRow.activeFocus && presetRow.focusIndex === index
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: Math.max(130, presetText.implicitHeight + 40)
                    radius: C.Theme.radius + 2
                    color: selected ? C.Theme.surfaceAccent : (presetMouse.containsMouse ? C.Theme.surfaceRaised : C.Theme.surface)
                    border.width: focused ? C.Theme.focusWidth + 1 : 2
                    border.color: selected || focused ? C.Theme.accent : C.Theme.border
                    opacity: modelData.available ? 1.0 : 0.45

                    ColumnLayout {
                        id: presetText
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                Layout.fillWidth: true
                                text: presetCard.modelData.name
                                font.pixelSize: C.Theme.fontLabel
                                font.weight: Font.DemiBold
                                color: C.Theme.text
                            }
                            Rectangle {
                                visible: presetCard.selected
                                width: 12
                                height: 12
                                radius: 6
                                color: C.Theme.accent
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: presetCard.modelData.available ? presetCard.modelData.desc : qsTr("Not available on this PC.")
                            wrapMode: Text.Wrap
                            font.pixelSize: C.Theme.fontCaption + 1
                            lineHeight: 1.15
                            color: C.Theme.textSecondary
                        }
                    }

                    MouseArea {
                        id: presetMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: presetCard.modelData.available
                        onClicked: {
                            presetRow.focusIndex = presetCard.index;
                            page.applyPreset(presetCard.modelData);
                        }
                    }
                }
            }
        }
    }

    Label {
        Layout.topMargin: -12
        text: presetRow.active === "custom" ? qsTr("Your own mix of the settings below (Custom).") : qsTr("Changing anything below switches to Custom.")
        font.pixelSize: C.Theme.fontCaption
        color: C.Theme.textSecondary
    }

    C.Card {
        title: qsTr("Frame generation")
        visible: page.s.frameGenSupported
        subtitle: page.s.directFrameMapping ? "" : qsTr("Needs Frame delivery: Direct Mapping (below).")

        C.SettingRow {
            divider: false
            enabled: page.fgUsable
            label: qsTr("Generator")
            description: page.s.frameGenVfgSupported && !page.s.frameGenVfgRuntime.length
                ? qsTr("NVIDIA VFG needs its runtime: download it below.")
                : qsTr("NVIDIA VFG has the best quality on RTX cards. Flow runs on any GPU.")
            defaultText: qsTr("Default: Off")

            C.Segmented {
                readonly property var g: page.generators
                model: g.map(x => x.label)
                enabledSegments: g.map(x => x.usable)
                currentIndex: Math.max(0, g.findIndex(x => x.backend === page.s.frameGenBackend))
                onActivated: index => page.s.frameGenBackend = g[index].backend
            }
        }

        C.SettingRow {
            enabled: page.fgUsable && page.multiplierBackend && !page.chainActive
            label: qsTr("Output")
            description: page.chainActive ? qsTr("The DLSS second stage below gives 4×.")
                : page.s.frameGenBackend === 2 ? qsTr("FSR always doubles the frame rate.")
                : qsTr("Frames shown per stream frame. 3× and 4× need a fast display (120 Hz+ for a 30 fps stream).")
            defaultText: qsTr("Default: 2×")

            C.Segmented {
                model: ["2×", "3×", "4×"]
                currentIndex: page.chainActive ? 2 : (page.multiplierBackend ? page.s.frameGenMultiplier - 2 : 0)
                onActivated: index => page.s.frameGenMultiplier = index + 2
            }
        }

        C.SettingRow {
            visible: page.s.frameGenVfgSupported
            enabled: page.fgUsable && page.s.frameGenBackend === 4
            label: qsTr("VFG quality")
            description: qsTr("High is the cleanest and costs about 11 ms of GPU time per frame at 1440p; Medium about 2 ms.")
            defaultText: qsTr("Default: High")

            C.Segmented {
                model: [qsTr("Low"), qsTr("Medium"), qsTr("High")]
                currentIndex: page.s.frameGenVfgMode
                onActivated: index => page.s.frameGenVfgMode = index
            }
        }

        C.SettingRow {
            visible: page.s.frameGenVfgSupported && page.s.frameGenDlssSupported
            enabled: page.fgUsable && page.s.frameGenBackend === 4
            label: qsTr("DLSS second stage")
            description: qsTr("VFG 2× then DLSS Frame Generation 2×: 4× for much less GPU time than VFG 4×, with some DLSS artifacts on UI.")
            defaultText: qsTr("Default: Off")

            C.Switch {
                checked: page.s.frameGenVfgChain
                onToggled: page.s.frameGenVfgChain = checked
            }
        }

        C.SettingRow {
            visible: page.s.frameGenFlowSupported
            enabled: page.fgUsable && (page.s.frameGenBackend === 3 || (page.s.frameGenBackend === 1 && page.s.frameGenDlssMotion > 0) || (page.s.frameGenBackend > 0 && page.s.frameGenCleanup > 0) || page.s.frameGenBackend === 2)
            label: qsTr("Motion search quality")
            description: qsTr("Used by Flow, the clean-up and DLSS motion vectors. Quality follows small details better and costs about 4× the analysis time.")
            defaultText: qsTr("Default: Performance")

            C.Segmented {
                model: [qsTr("Performance"), qsTr("Quality")]
                currentIndex: page.s.frameGenFlowHighQuality ? 1 : 0
                onActivated: index => page.s.frameGenFlowHighQuality = index === 1
            }
        }

        C.SettingRow {
            enabled: page.fgUsable && page.s.frameGenBackend > 0
            label: qsTr("Generated frame clean-up")
            description: qsTr("Keeps still HUD and text from the real frame and replaces smeared pixels where frames do not line up.")
            defaultText: qsTr("Default: + detail")

            C.Segmented {
                model: [qsTr("Off"), qsTr("Protect HUD"), qsTr("+ detail")]
                currentIndex: page.s.frameGenCleanup
                onActivated: index => page.s.frameGenCleanup = index
            }
        }

        C.SettingRow {
            visible: page.s.frameGenDlssSupported
            enabled: page.fgUsable && page.s.frameGenBackend === 1
            label: qsTr("DLSS motion vectors")
            description: qsTr("A video stream has no motion vectors; Flow's motion search gives DLSS sharper edges.")
            defaultText: qsTr("Default: Flow")

            C.ComboBox {
                width: 420
                model: [qsTr("Zero motion"), qsTr("Flow motion search (sharp edges, less jelly)"), qsTr("Flow motion search, reversed (for comparison)")]
                currentIndex: page.s.frameGenDlssMotion
                onActivated: index => page.s.frameGenDlssMotion = index
            }
        }

        C.SettingRow {
            visible: page.s.frameGenVfgSupported
            label: qsTr("NVIDIA runtime")
            description: page.s.vfgRuntimeDownloadState === 3 ? qsTr("Download failed, see the console window.")
                : page.s.frameGenVfgRuntime.length ? qsTr("Found: %1").arg(page.s.frameGenVfgRuntime)
                : qsTr("NVIDIA VFG and the newest RTX Video modes need NVIDIA's Video Effects runtime (about 420 MB).")

            C.Button {
                enabled: page.s.vfgRuntimeDownloadState !== 1
                primary: !page.s.frameGenVfgRuntime.length && page.s.vfgRuntimeDownloadState === 0
                text: page.s.vfgRuntimeDownloadState === 1 ? qsTr("Downloading…")
                    : page.s.vfgRuntimeDownloadState === 2 ? qsTr("Restart chiaki-ng")
                    : page.s.frameGenVfgRuntime.length ? qsTr("Download again")
                    : qsTr("Download")
                onClicked: {
                    if (page.s.vfgRuntimeDownloadState === 2)
                        page.s.restartApplication();
                    else
                        page.s.downloadVfgRuntime();
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Downloads NVIDIA's official Video Effects runtime (nvidia-vfx package from pypi.nvidia.com, checked by SHA-256) and installs the NVIDIA VFG libraries and nvngx_vsr.dll 1.8.3 for this user. NVIDIA's license terms apply.")
            }
        }
    }

    C.Card {
        title: qsTr("Presentation")

        C.SettingRow {
            divider: false
            label: qsTr("NVIDIA Reflex")
            description: qsTr("Frames do not queue up for the GPU. Lower latency. Applies from the next stream.")
            defaultText: qsTr("Default: On")

            C.Switch {
                checked: page.s.nvidiaReflex
                onToggled: page.s.nvidiaReflex = checked
            }
        }

        C.SettingRow {
            label: qsTr("VSync")
            description: qsTr("No tearing. Frames are aligned to your display refresh.")
            defaultText: qsTr("Default: Off")

            C.Switch {
                checked: page.s.vSyncEnabled
                onClicked: {
                    page.s.vSyncEnabled = checked
                    if (Chiaki.window.runtimeRendererBackend === 1 && page.s.restartApplication())
                        Qt.quit()
                }
            }
        }

        C.SettingRow {
            label: qsTr("Frame delivery")
            description: qsTr("Direct Mapping is needed for frame generation. Changing it restarts chiaki-ng.")
            defaultText: qsTr("Default: Direct Mapping")

            C.Segmented {
                model: [qsTr("Direct Mapping"), qsTr("Frame Queue")]
                currentIndex: page.s.directFrameMapping ? 0 : 1
                onActivated: (index) => {
                    const direct = index === 0;
                    if (direct === page.s.directFrameMapping)
                        return;
                    page.s.directFrameMapping = direct;
                    if (page.s.restartApplication())
                        Qt.quit();
                }
            }
        }

        C.SettingRow {
            enabled: !page.s.directFrameMapping
            label: qsTr("Frame mixer")
            description: qsTr("Blends frames to the display refresh. Only with Frame Queue.")
            defaultText: qsTr("Default: None")

            C.ComboBox {
                width: 300
                model: [qsTr("None"), qsTr("Oversample"), qsTr("Hermite"), qsTr("Linear"), qsTr("Cubic")]
                currentIndex: page.s.directFrameMapping ? 0 : page.s.placeboFrameMixer
                onActivated: index => page.s.placeboFrameMixer = index
            }
        }

        C.SettingRow {
            label: qsTr("Even pacing buffer")
            description: qsTr("Holds frames a few ms so uneven network arrival does not stutter. Sized automatically.")

            C.Chip {
                text: qsTr("Automatic")
                dot: true
                textColor: C.Theme.good
                color: C.Theme.goodBg
            }
        }
    }
}
