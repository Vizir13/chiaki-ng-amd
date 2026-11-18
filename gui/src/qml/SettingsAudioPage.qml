import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs

import org.streetpea.chiaking

import "controls" as C

// Generated from the former tabs of SettingsDialog.qml; the controls are unchanged.
C.SettingsPage {
    id: page

    title: qsTr("Audio")
    subtitle: qsTr("Speakers, microphone and voice processing.")
    firstFocusItem: audioOutDevice

    C.Card {
        title: qsTr("Audio")
        GridLayout {
            Layout.alignment: Qt.AlignLeft
            id: audiowifigrid
            columns: 3
            rowSpacing: 10
            columnSpacing: 20
            onVisibleChanged: if (visible) Chiaki.settings.refreshAudioDevices()
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Output Device:")
            }
            C.ComboBox {
                id: audioOutDevice
                Layout.preferredWidth: 400
                popup.x: (width - popup.width) / 2
                popup.width: 700
                popup.font.pixelSize: 16
                model: [qsTr("Auto")].concat(Chiaki.settings.availableAudioOutDevices)
                currentIndex: Math.max(0, model.indexOf(Chiaki.settings.audioOutDevice))
                onActivated: (index) => Chiaki.settings.audioOutDevice = index ? model[index] : ""
                KeyNavigation.down: audioInDevice
                KeyNavigation.priority: popup.visible ? KeyNavigation.AfterItem : KeyNavigation.BeforeItem
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(Auto)")
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Input Device:")
            }
            C.ComboBox {
                id: audioInDevice
                Layout.preferredWidth: 400
                popup.x: (width - popup.width) / 2
                popup.width: 700
                popup.font.pixelSize: 16
                model: [qsTr("Auto")].concat(Chiaki.settings.availableAudioInDevices)
                currentIndex: Math.max(0, model.indexOf(Chiaki.settings.audioInDevice))
                onActivated: (index) => Chiaki.settings.audioInDevice = index ? model[index] : ""
                KeyNavigation.up: audioOutDevice
                KeyNavigation.down: audioBufferSizeSlider
                KeyNavigation.priority: popup.visible ? KeyNavigation.AfterItem : KeyNavigation.BeforeItem
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(Auto)")
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Audio Buffer Size:")
            }
            C.Slider {
                id: audioBufferSizeSlider
                Layout.preferredWidth: 250
                from: 1
                to: 10
                stepSize: 1
                value: Chiaki.settings.audioBufferSize / 1920 ? (Chiaki.settings.audioBufferSize / 1920) : 5
                onMoved: Chiaki.settings.audioBufferSize = value * 1920;
                KeyNavigation.up: audioInDevice

                Label {
                    anchors {
                        left: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 10
                    }
                    text: {
                        (parent.value * 10).toFixed(0) + qsTr(" ms")
                    }
                }
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(50 ms)")
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Audio Volume:")
            }
            C.Slider {
                Layout.preferredWidth: 250
                from: 0
                to: 128
                stepSize: 1
                value: Chiaki.settings.audioVolume
                onMoved: Chiaki.settings.audioVolume = value

                Label {
                    anchors {
                        left: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 10
                    }
                    text: {
                        ((parent.value / 128.0) * 100).toFixed(0) + qsTr("% volume")
                    }
                }
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(100%)")
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Start Mic Unmuted:")
            }
            C.CheckBox {
                checked: Chiaki.settings.startMicUnmuted
                onToggled: Chiaki.settings.startMicUnmuted = checked
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(Unchecked)")
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Speech Processing:")
                visible: typeof Chiaki.settings.speechProcessing !== "undefined"
            }
            C.CheckBox {
                text: qsTr("Noise suppression + echo cancellation")
                checked: Chiaki.settings.speechProcessing
                onToggled: Chiaki.settings.speechProcessing = !Chiaki.settings.speechProcessing
                visible: typeof Chiaki.settings.speechProcessing !== "undefined"
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(Unchecked)")
                visible: typeof Chiaki.settings.speechProcessing !== "undefined"
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Noise To Suppress:")
                visible: if (typeof Chiaki.settings.speechProcessing !== "undefined") {Chiaki.settings.speechProcessing} else {false}
            }
            C.Slider {
                Layout.preferredWidth: 250
                from: 0
                to: 60
                stepSize: 1
                visible: if (typeof Chiaki.settings.speechProcessing !== "undefined") {Chiaki.settings.speechProcessing} else {false}
                value: Chiaki.settings.noiseSuppressLevel
                onMoved: Chiaki.settings.noiseSuppressLevel = value

                Label {
                    anchors {
                        left: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 10
                    }
                    text: qsTr("%1 dB").arg(parent.value)
                }
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(6 dB)")
                visible: if (typeof Chiaki.settings.speechProcessing !== "undefined") {Chiaki.settings.speechProcessing} else {false}
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Echo To Suppress:")
                visible: if (typeof Chiaki.settings.speechProcessing !== "undefined") {Chiaki.settings.speechProcessing} else {false}
            }
            C.Slider {
                Layout.preferredWidth: 250
                from: 0
                to: 60
                stepSize: 1
                value: Chiaki.settings.echoSuppressLevel
                visible: if (typeof Chiaki.settings.speechProcessing !== "undefined") {Chiaki.settings.speechProcessing} else {false}
                onMoved: Chiaki.settings.echoSuppressLevel = value

                Label {
                    anchors {
                        left: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 10
                    }
                    text: qsTr("%1 dB").arg(parent.value)
                }
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(30 dB)")
                visible: if (typeof Chiaki.settings.speechProcessing !== "undefined") {Chiaki.settings.speechProcessing} else {false}
            }
        }
    }

}
