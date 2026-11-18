import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import org.streetpea.chiaking

import "controls" as C
import "controls/Presets.js" as Presets

Pane {
    id: consolePane
    padding: 0
    background: Rectangle { color: C.Theme.bg }

    // The selected console is remembered by a key, not by its position, so
    // discovery updates that reorder Chiaki.hosts do not move the selection.
    property string selectedKey
    readonly property var hosts: Chiaki.hosts
    readonly property var visibleIndexes: {
        let r = [];
        for (let i = 0; i < hosts.length; ++i) {
            if (hosts[i].display)
                r.push(i);
        }
        return r;
    }
    readonly property int selectedIndex: {
        for (let i = 0; i < visibleIndexes.length; ++i) {
            if (hostKey(hosts[visibleIndexes[i]]) === selectedKey)
                return visibleIndexes[i];
        }
        return defaultIndex();
    }
    readonly property var selectedHost: selectedIndex >= 0 ? hosts[selectedIndex] : null

    function hostKey(h) {
        if (!h)
            return "";
        return (h.mac || h.duid || "") + "|" + (h.name || "") + "|" + (h.manual ? h.address : "");
    }
    function defaultIndex() {
        // Prefer a console that is on and found on the network
        for (let i = 0; i < visibleIndexes.length; ++i) {
            const h = hosts[visibleIndexes[i]];
            if (h.discovered && h.state === "ready")
                return visibleIndexes[i];
        }
        return visibleIndexes.length ? visibleIndexes[0] : -1;
    }
    function select(index) {
        if (index >= 0 && index < hosts.length)
            selectedKey = hostKey(hosts[index]);
    }
    function moveSelection(dir) {
        const n = visibleIndexes.length;
        if (!n)
            return;
        let pos = visibleIndexes.indexOf(selectedIndex);
        pos = pos < 0 ? 0 : (pos + dir + n) % n;
        select(visibleIndexes[pos]);
    }

    function connectToHost(index) {
        const h = hosts[index];
        if (!h)
            return;
        if (h.discovered)
            Chiaki.connectToHost(index, h.name);
        else
            Chiaki.connectToHost(index);
    }
    function canWake(h) { return h && h.registered && !h.duid && !h.discovered; }
    function wakeUpHost(index) {
        if (canWake(hosts[index]))
            Chiaki.wakeUpHost(index);
    }
    function canDelete(h) { return h && (h.manual || (h.discovered && !h.registered)); }
    function deleteHost(index) {
        const h = hosts[index];
        if (!h)
            return;
        if (h.manual)
            root.showConfirmDialog(qsTr("Delete Console"), qsTr("Are you sure you want to delete this console?"), () => {Chiaki.deleteHost(index)});
        else if (h.discovered && !h.registered)
            root.showConfirmDialog(qsTr("Hide Console"), qsTr("Are you sure you want to hide this console?") + "\n\n" + qsTr("Note: You can unhide from the Consoles section of the Settings under Hidden Consoles"), () => Chiaki.hideHost(h.mac, h.name));
    }
    function setConsolePin(index) {
        if (index >= 0)
            root.showConsolePinDialog(index);
    }

    function statusText(h) {
        if (h.duid && !h.discovered)
            return qsTr("Remote via PSN");
        if (!h.discovered)
            return h.registered ? qsTr("Not found") : qsTr("Manual");
        if (h.state === "ready")
            return qsTr("Ready");
        if (h.state === "standby")
            return qsTr("Rest mode");
        return h.state;
    }
    function statusColor(h) {
        if (h.duid && !h.discovered)
            return C.Theme.accent;
        if (h.discovered && h.state === "ready")
            return C.Theme.good;
        if (h.discovered && h.state === "standby")
            return C.Theme.warn;
        return C.Theme.textSecondary;
    }
    function statusBg(h) {
        if (h.discovered && h.state === "ready")
            return C.Theme.goodBg;
        if (h.discovered && h.state === "standby")
            return C.Theme.warnBg;
        return C.Theme.surfaceRaised;
    }
    function connectionText(h) {
        let t;
        if (h.duid && !h.discovered)
            t = qsTr("Over the internet");
        else if (h.discovered)
            t = h.manual ? qsTr("Local network · manual") : qsTr("Local network");
        else
            t = qsTr("Manual");
        if (h.address && !Chiaki.settings.streamerMode)
            t += " · " + h.address;
        return t;
    }
    function detailText(h) {
        let parts = [];
        if (h.duid && h.discovered)
            parts.push(qsTr("Automatic registration available"));
        else if (!h.duid)
            parts.push(h.registered ? qsTr("Registered") : qsTr("Not registered"));
        if (h.mac && !Chiaki.settings.streamerMode)
            parts.push(qsTr("ID %1").arg(h.mac));
        if (!h.discovered && !h.duid)
            parts.push(qsTr("State: %1").arg(h.state));
        return parts.join("  ·  ");
    }
    function controllerName() {
        const list = Chiaki.controllers;
        if (!list.length)
            return "";
        const c = list[0];
        if (c.dualSenseEdge)
            return qsTr("DualSense Edge");
        if (c.dualSense)
            return qsTr("DualSense");
        if (c.handheld)
            return qsTr("Handheld controls");
        if (c.steamVirtual)
            return qsTr("Steam Input controller");
        if (c.playStation)
            return qsTr("DualShock 4");
        return qsTr("Game controller");
    }

    StackView.onActivated: {
        forceActiveFocus(Qt.TabFocusReason);
        if(!Chiaki.autoConnect && !root.initialAsk && !Chiaki.window.directStream)
        {
            root.initialAsk = true;
            if(Chiaki.settings.addSteamShortcutAsk && (typeof Chiaki.createSteamShortcut === "function"))
                root.showRemindDialog(qsTr("Official Steam artwork + controller layout"), qsTr("Would you like to either create a new non-Steam game for chiaki-ng\nor update an existing non-Steam game with the official artwork and controller layout?") + "\n\n" + qsTr("(Note: If you select no now and want to do this later, click the button or press R3 from the main menu.)"), false, () => root.showSteamShortcutDialog(true));
            else if(Chiaki.settings.remotePlayAsk)
            {
                if(!Chiaki.settings.psnRefreshToken || !Chiaki.settings.psnAuthToken || !Chiaki.settings.psnAuthTokenExpiry || !Chiaki.settings.psnAccountId)
                    root.showRemindDialog(qsTr("Remote Play via PSN"), qsTr("Would you like to connect to PSN?\nThis enables:\n- Automatic registration\n- Playing outside of your home network without port forwarding?") + "\n\n" + qsTr("(Note: If you select no now and want to do this later, go to the Config section of the settings.)"), true, () => root.showPSNTokenDialog(false));
                else
                    Chiaki.settings.remotePlayAsk = false;
            }
        }
    }
    Keys.onUpPressed: moveSelection(-1)
    Keys.onDownPressed: moveSelection(1)
    Keys.onLeftPressed: moveSelection(-1)
    Keys.onRightPressed: moveSelection(1)
    Keys.onMenuPressed: root.showSettingsDialog()
    Keys.onReturnPressed: connectToHost(selectedIndex)
    Keys.onYesPressed: wakeUpHost(selectedIndex)
    Keys.onNoPressed: deleteHost(selectedIndex)
    Keys.onEscapePressed: root.showConfirmDialog(qsTr("Quit"), qsTr("Are you sure you want to quit?"), () => Qt.quit())
    Keys.onPressed: (event) => {
        if (event.modifiers)
            return;
        switch (event.key) {
        case Qt.Key_PageUp:
            if (selectedHost && selectedHost.registered) setConsolePin(selectedIndex);
            event.accepted = true;
            break;
        case Qt.Key_PageDown:
            if (Chiaki.settings.psnAuthToken) Chiaki.refreshPsnToken();
            event.accepted = true;
            break;
        case Qt.Key_F1:
            if (typeof Chiaki.createSteamShortcut === "function") root.showSteamShortcutDialog(false);
            event.accepted = true;
            break;
        case Qt.Key_F2:
            root.showManualHostDialog();
            event.accepted = true;
            break;
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Side navigation
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 300
            color: C.Theme.bgDeep

            Rectangle {
                anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
                width: 1
                color: C.Theme.borderSubtle
            }

            ColumnLayout {
                anchors {
                    fill: parent
                    leftMargin: 20
                    rightMargin: 21
                    topMargin: 32
                    bottomMargin: 24
                }
                spacing: 6

                RowLayout {
                    Layout.leftMargin: 10
                    Layout.bottomMargin: 30
                    spacing: 12
                    Image {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        sourceSize: Qt.size(34, 34)
                        source: "qrc:/icons/ui/logo.svg"
                    }
                    Label {
                        text: "chiaki-ng"
                        font.family: C.Theme.displayFamily
                        font.weight: Font.Bold
                        font.pixelSize: 25
                        color: C.Theme.text
                    }
                }

                C.NavItem {
                    Layout.fillWidth: true
                    text: qsTr("Consoles")
                    iconSource: "qrc:/icons/ui/consoles-active.svg"
                    current: true
                    focusPolicy: Qt.NoFocus
                }
                C.NavItem {
                    Layout.fillWidth: true
                    text: qsTr("PSN account")
                    iconSource: "qrc:/icons/ui/account.svg"
                    badge: Chiaki.settings.psnAuthToken ? qsTr("On") : ""
                    focusPolicy: Qt.NoFocus
                    onClicked: {
                        if (Chiaki.settings.psnAuthToken)
                            root.showSettingsDialog("stream");
                        else
                            root.showPSNTokenDialog(false);
                    }
                }
                C.NavItem {
                    Layout.fillWidth: true
                    text: qsTr("Settings")
                    iconSource: "qrc:/icons/ui/settings.svg"
                    focusPolicy: Qt.NoFocus
                    onClicked: root.showSettingsDialog()
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.bottomMargin: 10
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Find consoles")
                        font.pixelSize: C.Theme.fontSmall
                        color: C.Theme.textSecondary
                    }
                    C.Switch {
                        focusPolicy: Qt.NoFocus
                        checked: Chiaki.discoveryEnabled
                        onToggled: Chiaki.discoveryEnabled = checked
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: controllerColumn.implicitHeight + 32
                    radius: C.Theme.radius
                    color: "#121922"
                    border.width: 1
                    border.color: C.Theme.borderSubtle
                    ColumnLayout {
                        id: controllerColumn
                        anchors { fill: parent; margins: 16 }
                        spacing: 4
                        Label {
                            text: qsTr("Controller")
                            font.pixelSize: C.Theme.fontCaption
                            color: C.Theme.textSecondary
                        }
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: Chiaki.controllers.length ? consolePane.controllerName() : qsTr("None connected")
                            font.pixelSize: C.Theme.fontBody
                            font.weight: Font.DemiBold
                            color: C.Theme.text
                        }
                        Label {
                            visible: Chiaki.controllers.length > 0
                            text: Chiaki.controllers.length > 1 ? qsTr("%1 connected").arg(Chiaki.controllers.length) : qsTr("Connected")
                            font.pixelSize: C.Theme.fontCaption
                            color: C.Theme.good
                        }
                    }
                }

                Label {
                    Layout.leftMargin: 10
                    Layout.topMargin: 10
                    text: qsTr("Version %1").arg(Qt.application.version)
                    font.pixelSize: C.Theme.fontCaption
                    color: C.Theme.textDisabled
                }
            }
        }

        // Consoles
        Flickable {
            id: mainFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: mainColumn.implicitHeight + 80
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { visible: mainFlick.contentHeight > mainFlick.height }

            ColumnLayout {
                id: mainColumn
                x: 48
                y: 40
                width: Math.min(mainFlick.width - 96, 1500)
                spacing: 32

                // Header
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    ColumnLayout {
                        spacing: 6
                        Label {
                            text: qsTr("Consoles")
                            font.family: C.Theme.displayFamily
                            font.weight: Font.Bold
                            font.pixelSize: C.Theme.fontDisplay
                            color: C.Theme.text
                        }
                        Label {
                            text: qsTr("Found on your network and registered to this PC")
                            font.pixelSize: C.Theme.fontSmall
                            color: C.Theme.textSecondary
                        }
                    }
                    Item { Layout.fillWidth: true }
                    C.Button {
                        text: qsTr("Steam shortcut")
                        glyph: "qrc:/icons/l3.svg"
                        focusPolicy: Qt.NoFocus
                        visible: typeof Chiaki.createSteamShortcut === "function"
                        onClicked: root.showSteamShortcutDialog(false)
                    }
                    C.Button {
                        text: qsTr("Refresh PSN")
                        glyph: "qrc:/icons/r1.svg"
                        focusPolicy: Qt.NoFocus
                        visible: Chiaki.settings.psnAuthToken
                        onClicked: Chiaki.refreshPsnToken()
                    }
                    C.Button {
                        text: qsTr("Add console manually")
                        glyph: "qrc:/icons/r3.svg"
                        focusPolicy: Qt.NoFocus
                        onClicked: root.showManualHostDialog()
                    }
                    C.Button {
                        text: qsTr("Register new console")
                        focusPolicy: Qt.NoFocus
                        onClicked: root.showRegistDialog("255.255.255.255", true)
                    }
                }

                // Selected console
                Rectangle {
                    id: hero
                    readonly property var h: consolePane.selectedHost
                    visible: h !== null
                    Layout.fillWidth: true
                    implicitHeight: Math.max(360, heroColumn.implicitHeight + 72)
                    radius: C.Theme.radiusLarge
                    color: C.Theme.surface
                    border.width: 1
                    border.color: C.Theme.border
                    clip: true

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 1
                        spacing: 0

                        Rectangle {
                            Layout.fillHeight: true
                            Layout.preferredWidth: Math.round(hero.width * 0.32)
                            color: C.Theme.surfaceInset
                            radius: C.Theme.radiusLarge - 1
                            // square off the right edge
                            Rectangle {
                                anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
                                width: parent.radius
                                color: parent.color
                            }
                            Rectangle {
                                anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
                                width: 1
                                color: C.Theme.border
                            }
                            Image {
                                anchors.centerIn: parent
                                width: Math.min(parent.width * 0.7, 300)
                                height: Math.min(parent.height * 0.7, 240)
                                fillMode: Image.PreserveAspectFit
                                sourceSize: Qt.size(width, height)
                                source: hero.h ? "image://svg/console-ps" + (hero.h.ps5 ? "5" : "4") + (hero.h.state == "standby" ? "#light_standby" : "#light_on") : ""
                            }
                        }

                        ColumnLayout {
                            id: heroColumn
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.margins: 36
                            spacing: 22

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                Label {
                                    text: hero.h ? hero.h.name : ""
                                    font.family: C.Theme.displayFamily
                                    font.weight: Font.DemiBold
                                    font.pixelSize: C.Theme.fontHeading + 2
                                    color: C.Theme.text
                                    elide: Text.ElideRight
                                    Layout.maximumWidth: hero.width * 0.35
                                }
                                C.Chip {
                                    visible: hero.h !== null
                                    dot: true
                                    text: hero.h ? consolePane.statusText(hero.h) : ""
                                    textColor: hero.h ? consolePane.statusColor(hero.h) : C.Theme.textSecondary
                                    color: hero.h ? consolePane.statusBg(hero.h) : C.Theme.surfaceRaised
                                }
                                C.Chip {
                                    visible: hero.h !== null
                                    text: hero.h ? consolePane.connectionText(hero.h) : ""
                                }
                                Item { Layout.fillWidth: true }
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: hero.h ? consolePane.detailText(hero.h) : ""
                                font.pixelSize: C.Theme.fontCaption
                                color: C.Theme.textSecondary
                                elide: Text.ElideRight
                            }

                            GridLayout {
                                id: tiles
                                Layout.fillWidth: true
                                columns: 3
                                columnSpacing: 14
                                rowSpacing: 14
                                readonly property var stream: hero.h ? Presets.streamSummary(Chiaki.settings, hero.h) : null

                                Repeater {
                                    model: [
                                        { label: qsTr("Stream"), value: tiles.stream ? tiles.stream.text : "", accent: false },
                                        { label: qsTr("On screen"), value: tiles.stream ? Presets.screenSummary(Chiaki.settings, tiles.stream.fps) : "", accent: false },
                                        { label: qsTr("Preset"), value: Presets.currentName(Chiaki.settings), accent: true }
                                    ]
                                    delegate: Rectangle {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 1
                                        implicitHeight: 82
                                        radius: C.Theme.radius
                                        color: C.Theme.surfaceRaised
                                        Column {
                                            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 18 }
                                            spacing: 6
                                            Label {
                                                text: modelData.label
                                                font.pixelSize: C.Theme.fontCaption
                                                color: C.Theme.textSecondary
                                            }
                                            Label {
                                                width: parent.width
                                                elide: Text.ElideRight
                                                text: modelData.value
                                                font.pixelSize: C.Theme.fontBody + 1
                                                font.weight: Font.DemiBold
                                                color: modelData.accent ? C.Theme.accent : C.Theme.text
                                            }
                                        }
                                    }
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: hero.h !== null && hero.h.discovered && !!hero.h.app
                                text: hero.h && hero.h.app ? qsTr("Playing %1").arg(hero.h.app) + (hero.h.titleId ? "  ·  " + hero.h.titleId : "") : ""
                                font.pixelSize: C.Theme.fontSmall
                                color: C.Theme.textSecondary
                                elide: Text.ElideRight
                            }

                            Item { Layout.fillHeight: true }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                C.Button {
                                    primary: true
                                    focusPolicy: Qt.NoFocus
                                    radius: C.Theme.radius
                                    implicitHeight: 62
                                    leftPadding: 30
                                    rightPadding: 30
                                    font.pixelSize: C.Theme.fontLabel + 1
                                    font.weight: Font.Bold
                                    icon.source: "qrc:/icons/ui/play.svg"
                                    icon.width: 22
                                    icon.height: 22
                                    icon.color: C.Theme.textOnAccent
                                    text: hero.h && (hero.h.registered || hero.h.duid) ? qsTr("Start stream") : qsTr("Register")
                                    onClicked: consolePane.connectToHost(consolePane.selectedIndex)
                                }
                                Image {
                                    Layout.preferredWidth: 30
                                    Layout.preferredHeight: 30
                                    sourceSize: Qt.size(30, 30)
                                    source: root.controllerButton("cross")
                                }
                                C.Button {
                                    Layout.leftMargin: 8
                                    focusPolicy: Qt.NoFocus
                                    flat: true
                                    implicitHeight: 62
                                    radius: C.Theme.radius
                                    text: qsTr("Profile: %1").arg(Chiaki.settings.currentProfile ? Chiaki.settings.currentProfile : qsTr("default"))
                                    onClicked: root.showProfileDialog()
                                    outlined: true
                                }
                                Item { Layout.fillWidth: true }
                                C.Button {
                                    visible: consolePane.canWake(hero.h)
                                    focusPolicy: Qt.NoFocus
                                    flat: true
                                    text: qsTr("Wake")
                                    glyph: root.controllerButton("pyramid")
                                    onClicked: consolePane.wakeUpHost(consolePane.selectedIndex)
                                }
                                C.Button {
                                    visible: hero.h !== null && hero.h.registered
                                    focusPolicy: Qt.NoFocus
                                    flat: true
                                    text: qsTr("Console PIN")
                                    glyph: "qrc:/icons/l1.svg"
                                    onClicked: consolePane.setConsolePin(consolePane.selectedIndex)
                                }
                                C.Button {
                                    visible: consolePane.canDelete(hero.h)
                                    focusPolicy: Qt.NoFocus
                                    flat: true
                                    text: hero.h && hero.h.manual ? qsTr("Delete") : qsTr("Hide")
                                    glyph: root.controllerButton("box")
                                    onClicked: consolePane.deleteHost(consolePane.selectedIndex)
                                }
                            }
                        }
                    }
                }

                // No consoles yet
                Rectangle {
                    visible: consolePane.selectedHost === null
                    Layout.fillWidth: true
                    implicitHeight: 300
                    radius: C.Theme.radiusLarge
                    color: C.Theme.surface
                    border.width: 1
                    border.color: C.Theme.border
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 14
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: Chiaki.discoveryEnabled ? qsTr("Looking for consoles…") : qsTr("No consoles yet")
                            font.family: C.Theme.displayFamily
                            font.weight: Font.DemiBold
                            font.pixelSize: C.Theme.fontHeading
                            color: C.Theme.text
                        }
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Turn the console on, or add it by its address.")
                            font.pixelSize: C.Theme.fontSmall
                            color: C.Theme.textSecondary
                        }
                    }
                }

                // Other consoles
                Label {
                    visible: otherRepeater.count > 1
                    text: qsTr("Other consoles")
                    font.pixelSize: C.Theme.fontLabel
                    font.weight: Font.DemiBold
                    color: C.Theme.text
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 20
                    rowSpacing: 20

                    Repeater {
                        id: otherRepeater
                        model: consolePane.visibleIndexes
                        delegate: Rectangle {
                            id: other
                            required property var modelData
                            readonly property int hostIndex: modelData
                            readonly property var h: consolePane.hosts[hostIndex]
                            visible: hostIndex !== consolePane.selectedIndex
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            implicitHeight: 104
                            radius: C.Theme.radiusLarge - 4
                            color: otherMouse.containsMouse ? C.Theme.surfaceRaised : C.Theme.surface
                            border.width: 1
                            border.color: C.Theme.border

                            MouseArea {
                                id: otherMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: consolePane.select(other.hostIndex)
                                onDoubleClicked: consolePane.connectToHost(other.hostIndex)
                            }

                            RowLayout {
                                anchors { fill: parent; leftMargin: 24; rightMargin: 24 }
                                spacing: 18
                                Rectangle {
                                    Layout.preferredWidth: 60
                                    Layout.preferredHeight: 60
                                    radius: C.Theme.radius
                                    color: C.Theme.surfaceRaised
                                    Image {
                                        anchors.centerIn: parent
                                        width: 28
                                        height: 28
                                        sourceSize: Qt.size(28, 28)
                                        source: "qrc:/icons/ui/power.svg"
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Label {
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        text: other.h ? other.h.name : ""
                                        font.pixelSize: C.Theme.fontBody
                                        font.weight: Font.DemiBold
                                        color: C.Theme.text
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        text: other.h ? consolePane.statusText(other.h) + " · " + consolePane.connectionText(other.h) : ""
                                        font.pixelSize: C.Theme.fontCaption
                                        color: C.Theme.textSecondary
                                    }
                                }
                                C.Button {
                                    focusPolicy: Qt.NoFocus
                                    flat: true
                                    outlined: true
                                    text: consolePane.canWake(other.h) ? qsTr("Wake") : qsTr("Select")
                                    onClicked: {
                                        if (consolePane.canWake(other.h))
                                            consolePane.wakeUpHost(other.hostIndex);
                                        else
                                            consolePane.select(other.hostIndex);
                                    }
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    wrapMode: Text.Wrap
                    text: qsTr("↑ ↓ choose a console  ·  Options opens settings")
                    font.pixelSize: C.Theme.fontCaption
                    color: C.Theme.textDisabled
                }
            }
        }
    }
}
