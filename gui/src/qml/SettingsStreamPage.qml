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

    title: qsTr("Stream & network")
    subtitle: qsTr("What the console sends: resolution, frame rate, bitrate and codec, plus network warnings and remote play over PSN.")
    firstFocusItem: consoleSelection
    property int selectedConsole: SettingsDialog.Console.PS5

    C.Card {
        title: qsTr("Stream quality")
        subtitle: qsTr("Applies from the next stream. Local is your home network, Remote is over the internet.")
                            GridLayout {
            Layout.alignment: Qt.AlignLeft
                                id: streamGrid
                                columns: 3
                                rowSpacing: 10
                                columnSpacing: 20
                                Label {
                                    Layout.alignment: Qt.AlignRight
                                    text: qsTr("Settings for:")
                                }
                            C.ComboBox {
                                id: consoleSelection
                                Layout.preferredWidth: 400
                                Layout.alignment: Qt.AlignLeft
                                model: [qsTr("PS4"), qsTr("PS5")]
                                currentIndex: selectedConsole
                                onActivated: (index) => selectedConsole = index
                                KeyNavigation.priority: {
                                    if(!popup.visible)
                                        KeyNavigation.BeforeItem
                                    else
                                        KeyNavigation.AfterItem
                                }
                                KeyNavigation.down: {
                                    if(selectedConsole == SettingsDialog.Console.PS4)
                                        resolutionLocalPS4
                                    else
                                        resolutionLocalPS5

                                }

                            }
                            Label {
                                Layout.alignment: Qt.AlignRight
                                text: qsTr("")
                            }
                            Label {
                                Layout.alignment: Qt.AlignRight
                                text: qsTr("")
                            }
                            Label {
                                Layout.alignment: Qt.AlignRight
                                text: qsTr("")
                            }
                            Label {
                                Layout.alignment: Qt.AlignRight
                                text: qsTr("")
                            }
                            Label {
                                Layout.alignment: Qt.AlignRight
                                text: qsTr("")
                            }
                            Label {
                                Layout.alignment: Qt.AlignCenter
                                text: qsTr("Local")
                            }
                            Label {
                                Layout.alignment: Qt.AlignCenter
                                text: qsTr("Remote")
                            }
                            Label {
                                Layout.alignment: Qt.AlignRight
                                text: qsTr("Resolution:")
                            }
                            C.ComboBox {
                                id: resolutionLocalPS4
                                Layout.preferredWidth: 400
                                model: [qsTr("360p"), qsTr("540p"), qsTr("720p (Default)"), qsTr("1080p (PS5 and PS4 Pro)")]
                                currentIndex: Chiaki.settings.resolutionLocalPS4 - 1
                                onActivated: (index) => {
                                    Chiaki.settings.resolutionLocalPS4 = index + 1
                                    Chiaki.settings.bitrateLocalPS4 = 0
                                }
                                visible: selectedConsole == SettingsDialog.Console.PS4
                                KeyNavigation.right: resolutionRemotePS4
                                KeyNavigation.down: fpsLocalPS4
                                KeyNavigation.up: consoleSelection
                                KeyNavigation.priority: {
                                    if(!popup.visible)
                                        KeyNavigation.BeforeItem
                                    else
                                        KeyNavigation.AfterItem
                                }
                            }
                            C.ComboBox {
                                id: resolutionRemotePS4
                                Layout.preferredWidth: 400
                                model: [qsTr("360p"), qsTr("540p"), qsTr("720p (Default)"), qsTr("1080p (PS5 and PS4 Pro)")]
                                currentIndex: Chiaki.settings.resolutionRemotePS4 - 1
                                onActivated: (index) => {
                                    Chiaki.settings.resolutionRemotePS4 = index + 1
                                    Chiaki.settings.bitrateRemotePS4 = 0
                                }
                                visible: selectedConsole == SettingsDialog.Console.PS4
                                KeyNavigation.left: resolutionLocalPS4
                                KeyNavigation.down: fpsRemotePS4
                                KeyNavigation.up: consoleSelection
                                KeyNavigation.priority: {
                                    if(!popup.visible)
                                        KeyNavigation.BeforeItem
                                    else
                                        KeyNavigation.AfterItem
                                }
                            }
                            C.ComboBox {
                                id: resolutionLocalPS5
                                Layout.preferredWidth: 400
                                model: [qsTr("360p"), qsTr("540p"), qsTr("720p"), qsTr("1080p (Default)")]
                                currentIndex: Chiaki.settings.resolutionLocalPS5 - 1
                                onActivated: (index) => {
                                    Chiaki.settings.resolutionLocalPS5 = index + 1
                                    Chiaki.settings.bitrateLocalPS5 = 0
                                }
                                visible: selectedConsole == SettingsDialog.Console.PS5
                                KeyNavigation.right: resolutionRemotePS5
                                KeyNavigation.up: consoleSelection
                                KeyNavigation.down: fpsLocalPS5
                                KeyNavigation.priority: {
                                    if(!popup.visible)
                                        KeyNavigation.BeforeItem
                                    else
                                        KeyNavigation.AfterItem
                                }
                            }
                            C.ComboBox {
                                id: resolutionRemotePS5
                                Layout.preferredWidth: 400
                                model: [qsTr("360p"), qsTr("540p"), qsTr("720p (Default)"), qsTr("1080p")]
                                currentIndex: Chiaki.settings.resolutionRemotePS5 - 1
                                onActivated: (index) => {
                                    Chiaki.settings.resolutionRemotePS5 = index + 1
                                    Chiaki.settings.bitrateRemotePS5 = 0
                                }
                                visible: selectedConsole == SettingsDialog.Console.PS5
                                KeyNavigation.left: resolutionLocalPS5
                                KeyNavigation.up: consoleSelection
                                KeyNavigation.down: fpsRemotePS5
                                KeyNavigation.priority: {
                                    if(!popup.visible)
                                        KeyNavigation.BeforeItem
                                    else
                                        KeyNavigation.AfterItem
                                }
                            }
                            Label {
                                Layout.alignment: Qt.AlignRight
                                text: qsTr("FPS:")
                            }
                            C.ComboBox {
                                id: fpsLocalPS4
                                Layout.preferredWidth: 400
                                model: [qsTr("30 fps"), qsTr("60 fps (Default)")]
                                currentIndex: (Chiaki.settings.fpsLocalPS4 / 30) - 1
                                onActivated: (index) => Chiaki.settings.fpsLocalPS4 = (index + 1) * 30
                                visible: selectedConsole == SettingsDialog.Console.PS4
                                KeyNavigation.up: resolutionLocalPS4
                                KeyNavigation.right: fpsRemotePS4
                                KeyNavigation.down: bitrateLocalPS4
                                KeyNavigation.priority: {
                                    if(!popup.visible)
                                        KeyNavigation.BeforeItem
                                    else
                                        KeyNavigation.AfterItem
                                }
                            }
                            C.ComboBox {
                                id: fpsRemotePS4
                                Layout.preferredWidth: 400
                                model: [qsTr("30 fps"), qsTr("60 fps (Default)")]
                                currentIndex: (Chiaki.settings.fpsRemotePS4 / 30) - 1
                                onActivated: (index) => Chiaki.settings.fpsRemotePS4 = (index + 1) * 30
                                visible: selectedConsole == SettingsDialog.Console.PS4
                                KeyNavigation.up: resolutionRemotePS4
                                KeyNavigation.left: fpsLocalPS4
                                KeyNavigation.down: bitrateRemotePS4
                                KeyNavigation.priority: {
                                    if(!popup.visible)
                                        KeyNavigation.BeforeItem
                                    else
                                        KeyNavigation.AfterItem
                                }
                            }
                            C.ComboBox {
                                id: fpsLocalPS5
                                Layout.preferredWidth: 400
                                model: [qsTr("30 fps"), qsTr("60 fps (Default)")]
                                currentIndex: (Chiaki.settings.fpsLocalPS5 / 30) - 1
                                onActivated: (index) => Chiaki.settings.fpsLocalPS5 = (index + 1) * 30
                                visible: selectedConsole == SettingsDialog.Console.PS5
                                KeyNavigation.up: resolutionLocalPS5
                                KeyNavigation.right: fpsRemotePS5
                                KeyNavigation.down: bitrateLocalPS5
                                KeyNavigation.priority: {
                                    if(!popup.visible)
                                        KeyNavigation.BeforeItem
                                    else
                                        KeyNavigation.AfterItem
                                }
                            }
                            C.ComboBox {
                                id: fpsRemotePS5
                                Layout.preferredWidth: 400
                                model: [qsTr("30 fps"), qsTr("60 fps (Default)")]
                                currentIndex: (Chiaki.settings.fpsRemotePS5 / 30) - 1
                                onActivated: (index) => Chiaki.settings.fpsRemotePS5 = (index + 1) * 30
                                visible: selectedConsole == SettingsDialog.Console.PS5
                                KeyNavigation.up: resolutionRemotePS5
                                KeyNavigation.left: fpsLocalPS5
                                KeyNavigation.down: bitrateRemotePS5
                                KeyNavigation.priority: {
                                    if(!popup.visible)
                                        KeyNavigation.BeforeItem
                                    else
                                        KeyNavigation.AfterItem
                                }
                            }
                            Label {
                                Layout.alignment: Qt.AlignRight
                                text: qsTr("Bitrate:")
                            }
                            C.Slider {
                                id: bitrateLocalPS4
                                visible: selectedConsole == SettingsDialog.Console.PS4
                                property var bitrate: {
                                    var rate = 0;
                                    switch (Chiaki.settings.resolutionLocalPS4) {
                                    case 1: rate = 2; break; // 360p
                                    case 2: rate = 6; break; // 540p
                                    case 3: rate = 10; break; // 720p
                                    case 4: rate = 15; break; // 1080p
                                    }
                                    return rate;
                                }
                                Layout.preferredWidth: 200
                                from: 2
                                to: 100
                                stepSize: 1
                                value: Chiaki.settings.bitrateLocalPS4 / 1000 ? (Chiaki.settings.bitrateLocalPS4 / 1000) : bitrate
                                onMoved: Chiaki.settings.bitrateLocalPS4 = value * 1000;
                                KeyNavigation.up: fpsLocalPS4
                                KeyNavigation.down: bitrateLocalPS4
                                KeyNavigation.priority: KeyNavigation.BeforeItem
                                Label {
                                    anchors {
                                        left: parent.right
                                        verticalCenter: parent.verticalCenter
                                        leftMargin: 10
                                    }
                                    text: (parent.value) + qsTr(" Mbps") + qsTr(" (%1 Mbps)").arg(parent.bitrate.toFixed(0))
                                }
                            }
                            C.Slider {
                                id: bitrateRemotePS4
                                visible: selectedConsole == SettingsDialog.Console.PS4
                                property var bitrate: {
                                    var rate = 0;
                                    switch (Chiaki.settings.resolutionRemotePS4) {
                                    case 1: rate = 2; break; // 360p
                                    case 2: rate = 6; break; // 540p
                                    case 3: rate = 10; break; // 720p
                                    case 4: rate = 15; break; // 1080p
                                    }
                                    return rate;
                                }
                                Layout.preferredWidth: 200
                                from: 2
                                to: 100
                                stepSize: 1
                                value: Chiaki.settings.bitrateRemotePS4 / 1000 ? (Chiaki.settings.bitrateRemotePS4 / 1000) : bitrate
                                onMoved: Chiaki.settings.bitrateRemotePS4 = value * 1000;
                                KeyNavigation.up: fpsRemotePS4
                                KeyNavigation.down: bitrateRemotePS4
                                KeyNavigation.priority: KeyNavigation.BeforeItem

                                Label {
                                    anchors {
                                        left: parent.right
                                        verticalCenter: parent.verticalCenter
                                        leftMargin: 10
                                    }
                                    text: (parent.value) + qsTr(" Mbps") + qsTr(" (%1 Mbps)").arg(parent.bitrate.toFixed(0))
                                }
                            }
                            C.Slider {
                                id: bitrateLocalPS5
                                visible: selectedConsole == SettingsDialog.Console.PS5
                                property var bitrate: {
                                    var rate = 0;
                                    switch (Chiaki.settings.resolutionLocalPS5) {
                                    case 1: rate = 2; break; // 360p
                                    case 2: rate = 6; break; // 540p
                                    case 3: rate = 10; break; // 720p
                                    case 4: rate = 15; break; // 1080p
                                    }
                                    return rate;
                                }
                                Layout.preferredWidth: 200
                                from: 2
                                to: 100
                                stepSize: 1
                                value: Chiaki.settings.bitrateLocalPS5 / 1000 ? (Chiaki.settings.bitrateLocalPS5 / 1000) : bitrate
                                onMoved: Chiaki.settings.bitrateLocalPS5 = value * 1000;
                                KeyNavigation.up: fpsLocalPS5
                                KeyNavigation.down: codecLocalPS5
                                KeyNavigation.priority: KeyNavigation.BeforeItem

                                Label {
                                    anchors {
                                        left: parent.right
                                        verticalCenter: parent.verticalCenter
                                        leftMargin: 10
                                    }
                                    text: (parent.value) + qsTr(" Mbps") + qsTr(" (%1 Mbps)").arg(parent.bitrate.toFixed(0))
                                }
                            }
                            C.Slider {
                                id: bitrateRemotePS5
                                visible: selectedConsole == SettingsDialog.Console.PS5
                                property var bitrate: {
                                    var rate = 0;
                                    switch (Chiaki.settings.resolutionRemotePS5) {
                                    case 1: rate = 2; break; // 360p
                                    case 2: rate = 6; break; // 540p
                                    case 3: rate = 10; break; // 720p
                                    case 4: rate = 15; break; // 1080p
                                    }
                                    return rate;
                                }
                                Layout.preferredWidth: 200
                                from: 2
                                to: 100
                                stepSize: 1
                                value: Chiaki.settings.bitrateRemotePS5 / 1000 ? (Chiaki.settings.bitrateRemotePS5 / 1000) : bitrate
                                onMoved: Chiaki.settings.bitrateRemotePS5 = value * 1000;
                                KeyNavigation.up: fpsRemotePS5
                                KeyNavigation.down: codecRemotePS5
                                KeyNavigation.priority: KeyNavigation.BeforeItem

                                Label {
                                    anchors {
                                        left: parent.right
                                        verticalCenter: parent.verticalCenter
                                        leftMargin: 10
                                    }
                                    text: (parent.value) + qsTr(" Mbps") + qsTr(" (%1 Mbps)").arg(parent.bitrate.toFixed(0))
                                }
                            }
                            Label {
                                Layout.alignment: Qt.AlignRight
                                text: qsTr("Codec:")
                                visible: selectedConsole == SettingsDialog.Console.PS5
                            }
                            C.ComboBox {
                                id: codecLocalPS5
                                Layout.preferredWidth: 400
                                property bool openGlBackend: Chiaki.settings.rendererBackend === 1
        						model: openGlBackend ? [qsTr("H264"), qsTr("H265 (Default)")] : [qsTr("H264"), qsTr("H265 (Default)"), qsTr("H265 HDR")]
                                currentIndex: Chiaki.settings.codecLocalPS5
                                onActivated: (index) => Chiaki.settings.codecLocalPS5 = index
                                visible: selectedConsole == SettingsDialog.Console.PS5
                                Keys.onReturnPressed: {
                                    if (popup.visible) {
                                        activated(highlightedIndex);
                                        popup.close();
                                    } else
                                        popup.open();
                                }
                                KeyNavigation.up: bitrateLocalPS5
                                KeyNavigation.right: codecRemotePS5
                                KeyNavigation.down: codecLocalPS5
                                KeyNavigation.priority: {
                                    if(!popup.visible)
                                        KeyNavigation.BeforeItem
                                    else
                                        KeyNavigation.AfterItem
                                }
                            }
                                C.ComboBox {
                                    id: codecRemotePS5
                                    Layout.preferredWidth: 400
                                property bool openGlBackend: Chiaki.settings.rendererBackend === 1
                                model: openGlBackend ? [qsTr("H264"), qsTr("H265 (Default)")] : [qsTr("H264"), qsTr("H265 (Default)"), qsTr("H265 HDR")]
                                    currentIndex: Chiaki.settings.codecRemotePS5
                                    onActivated: (index) => Chiaki.settings.codecRemotePS5 = index
                                    visible: selectedConsole == SettingsDialog.Console.PS5
                                    Keys.onReturnPressed: {
                                        if (popup.visible) {
                                            activated(highlightedIndex);
                                            popup.close();
                                        } else
                                            popup.open();
                                    }
                                    KeyNavigation.up: bitrateRemotePS5
                                    KeyNavigation.left: codecLocalPS5
                                    KeyNavigation.priority: {
                                        if(!popup.visible)
                                            KeyNavigation.BeforeItem
                                        else
                                            KeyNavigation.AfterItem
                                    }
                                }
                            }
    }

    C.Card {
        title: qsTr("Network and stats")
        GridLayout {
            Layout.alignment: Qt.AlignLeft
            columns: 3
            rowSpacing: 10
            columnSpacing: 20
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Weak Wifi Notification:")
            }
            C.Slider {
                Layout.preferredWidth: 250
                from: 0
                to: 100
                stepSize: 1
                value: Chiaki.settings.wifiDroppedNotif
                onMoved: Chiaki.settings.wifiDroppedNotif = value

                Label {
                    anchors {
                        left: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 10
                    }
                    text: qsTr(">= %1% dropped packets").arg(parent.value)
                }
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(3%)")
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Packet Loss Reported Max:")
            }
            C.Slider {
                Layout.preferredWidth: 250
                from: 0
                to: 100
                stepSize: 1
                value: Chiaki.settings.packetLossReportedMax
                onMoved: Chiaki.settings.packetLossReportedMax = value

                Label {
                    anchors {
                        left: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 10
                    }
                    text: qsTr("%1% packet loss").arg(parent.value)
                }
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(5%)")
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Request IDR Frame on FEC Failure")
            }
            C.CheckBox {
                checked: Chiaki.settings.iDROnFECFailureEnabled
                onToggled: Chiaki.settings.iDROnFECFailureEnabled = !Chiaki.settings.iDROnFECFailureEnabled
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(Unchecked)")
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Show Stream Stats During Gameplay")
            }
            C.CheckBox {
                checked: Chiaki.settings.showStreamStats
                onToggled: Chiaki.settings.showStreamStats = !Chiaki.settings.showStreamStats
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(Unchecked)")
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Stream Stats Style")
            }
            C.ComboBox {
                Layout.preferredWidth: 400
                enabled: Chiaki.settings.showStreamStats
                // Click the pill during a stream to open or close the details
                model: [qsTr("Pill (fps, frame gen, Reflex, latency)"), qsTr("Pill + bitrate and packet loss"),
                    qsTr("Pill + details open"), qsTr("Pill + details, every number")]
                currentIndex: Chiaki.settings.streamStatsStyle
                onActivated: index => Chiaki.settings.streamStatsStyle = index
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(Pill + details open)")
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Stream Stats Position")
            }
            C.ComboBox {
                Layout.preferredWidth: 400
                enabled: Chiaki.settings.showStreamStats
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
        }
    }

    C.Card {
        title: qsTr("PSN and remote play")
        subtitle: qsTr("Sign in to PSN for automatic registration and to play outside your home network.")
        GridLayout {
            Layout.alignment: Qt.AlignLeft
            id: remoteGrid
            columns: 3
            rowSpacing: 20
            columnSpacing: 10
            C.Button {
                id: openPsnLogin
                Layout.alignment: Qt.AlignHCenter
                Layout.columnSpan: 3

                text: qsTr("Login to PSN")
                onClicked: {
                    root.showPSNTokenDialog(false)
                }
                Material.roundedScale: Material.SmallScale
                visible: !Chiaki.settings.psnRefreshToken || !Chiaki.settings.psnAuthToken || !Chiaki.settings.psnAuthTokenExpiry || !Chiaki.settings.psnAccountId
            }
            C.Button {
                id: resetPsnTokens
                topPadding: 26
                leftPadding: 30
                rightPadding: 30
                bottomPadding: 26
                text: qsTr("Clear PSN Token")
                Layout.alignment: Qt.AlignHCenter
                Layout.columnSpan: 3
                onClicked: {
                    Chiaki.settings.psnRefreshToken = ""
                    Chiaki.settings.psnAuthToken = ""
                    Chiaki.settings.psnAuthTokenExpiry = ""
                    Chiaki.settings.psnAccountId = ""
                    openPsnLogin.forceActiveFocus(Qt.TabFocusReason);
                }
                Material.roundedScale: Material.SmallScale
                visible: Chiaki.settings.psnRefreshToken && Chiaki.settings.psnAuthToken && Chiaki.settings.psnAuthTokenExpiry && Chiaki.settings.psnAccountId
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Hole Punching Port Guessing:")
            }
            C.CheckBox {
                id: holePunchGuessingCheckbox
                text: qsTr("Force STUN port guessing")
                checked: Chiaki.settings.portGuessingEnabled
                onToggled: Chiaki.settings.portGuessingEnabled = checked
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(Unchecked)")
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Port Guess Count:")
            }
            C.Slider {
                id: portGuessCountSlider
                Layout.preferredWidth: 250
                from: 0
                to: 75
                stepSize: 1
                value: Chiaki.settings.portGuessCount
                onMoved: Chiaki.settings.portGuessCount = value

                Label {
                    anchors {
                        left: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 10
                    }
                    text: parent.value + qsTr(" guesses")
                }
            }
            Label {
                Layout.alignment: Qt.AlignRight
                Layout.leftMargin: 100
                text: qsTr("(75)")
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Port Guess Socket Count:")
            }
            C.Slider {
                id: portGuessSocketSlider
                Layout.preferredWidth: 250
                from: 0
                to: 500
                stepSize: 1
                value: Chiaki.settings.portGuessSocketCount
                onMoved: Chiaki.settings.portGuessSocketCount = value

                Label {
                    anchors {
                        left: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 10
                    }
                    text: parent.value + qsTr(" sockets")
                }
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(250)")
                Layout.leftMargin: 100
            }
        }
    }

}
