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

    title: qsTr("Advanced")
    subtitle: qsTr("Registered consoles, what happens on disconnect, profiles, backups and logs.")
    firstFocusItem: disconnectAction

    C.Card {
        title: qsTr("Behaviour")
        GridLayout {
            Layout.alignment: Qt.AlignLeft
            columns: 3
            rowSpacing: 5
            columnSpacing: 20
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Action On Disconnect:")
            }
        C.ComboBox {
            id: disconnectAction
            Layout.preferredWidth: 400
            model: [qsTr("Do Nothing"), qsTr("Enter Sleep Mode"), qsTr("Ask")]
            currentIndex: Chiaki.settings.disconnectAction
            onActivated: index => Chiaki.settings.disconnectAction = index
        }
        Label {
            text: qsTr("(Ask)")
        }
        Label {
            text: qsTr("Action On Suspend:")
        }
        C.ComboBox {
            Layout.preferredWidth: 400
            model: [qsTr("Do Nothing"), qsTr("Enter Sleep Mode")]
            currentIndex: Chiaki.settings.suspendAction
            onActivated: index => Chiaki.settings.suspendAction = index
        }
        Label {
            text: qsTr("(Do Nothing)")
        }
        Label {
            text: qsTr("Audio/Video:")
        }
        C.ComboBox {
            Layout.preferredWidth: 400
            model: [qsTr("Audio and Video Enabled"), qsTr("Audio Disabled"), qsTr("Video Disabled"), qsTr("Audio and Video Disabled")]
            currentIndex: Chiaki.settings.audioVideoDisabled
            onActivated: index => Chiaki.settings.audioVideoDisabled = index
        }
        Label {
            text: qsTr("(Audio and Video Enabled)")
        }
        Label {
            text: qsTr("Log Directory:")
        }
        Label {
            Layout.maximumWidth: 600
            text: Chiaki.settings.logDirectory
            verticalAlignment: Text.AlignVCenter
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 10

            C.Button {
                id: openButton
                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: parent.paintedWidth + 20
                }
                text: qsTr("Open")
                onClicked: Qt.openUrlExternally("file://" + parent.text);
                Material.roundedScale: Material.SmallScale
            }
        }
        Label {

        }
        Label {
            text: qsTr("Streamer Mode (Hides Info)")
        }
        C.CheckBox {
            id: streamerMode
            checked: Chiaki.settings.streamerMode
            onToggled: Chiaki.settings.streamerMode = !Chiaki.settings.streamerMode
        }
        Label {
            text: qsTr("(Unchecked)")
        }
        }
    }

    C.Card {
        title: qsTr("Consoles")
        subtitle: qsTr("Register a console with this PC, or bring back a hidden one.")
        ColumnLayout {
            Layout.alignment: Qt.AlignLeft
            id: consolesLayout
            spacing: 10
            C.Button {
                id: registerNewButton
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 10
                topPadding: 26
                leftPadding: 30
                rightPadding: 30
                bottomPadding: 26
                text: qsTr("Register New")
                onClicked: root.showRegistDialog("255.255.255.255", true)
                Material.roundedScale: Material.SmallScale
            }
            Label {
                id: consolesLabel
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Registered Consoles")
                font.bold: true
            }
            ListView {
                id: consolesView
                Layout.alignment: Qt.AlignHCenter
                height: 170
                onCountChanged: {
                    consolesView.contentHeight = consolesView.count * 80 + consolesView.anchors.topMargin;
                }
                keyNavigationEnabled: false
                width: 700
                ScrollBar.vertical: ScrollBar {
                    id: consolesScrollbar
                    policy: ScrollBar.AlwaysOn
                    visible: consolesView.contentHeight > consolesView.height
                }
                clip: true
                model: Chiaki.settings.registeredHosts
                delegate: ItemDelegate {
                    text: "%1 (%2, %3)".arg(Chiaki.settings.streamerMode ? "hidden" : modelData.mac).arg(modelData.ps5 ? "PS5" : "PS4").arg(modelData.name)
                    height: 80
                    width: parent ? parent.width : 0
                    leftPadding: autoConnectButton.width + 40

                    CheckBox {
                        property bool firstInFocusChain: false
                        property bool lastInFocusChain: false
                        property bool lastDownInFocusChain: index > consolesView.count + hiddenConsolesView.count - 2

                        id: autoConnectButton
                        anchors {
                            left: parent.left
                            verticalCenter: parent.verticalCenter
                            leftMargin: 20
                        }
                        text: qsTr("Auto-Connect")
                        checked: Chiaki.settings.autoConnectMac == modelData.mac
                        onToggled: Chiaki.settings.autoConnectMac = checked ? modelData.mac : "";

                        Keys.onPressed: (event) => {
                            switch (event.key) {
                            case Qt.Key_Right:
                                if (!lastInFocusChain) {
                                    let item = nextItemInFocusChain();
                                    if (item)
                                        item.forceActiveFocus(Qt.TabFocusReason);
                                    event.accepted = true;
                                }
                                break;
                            case Qt.Key_Up:
                                if (!firstInFocusChain) {
                                    let item = nextItemInFocusChain(false);
                                    if (item) {
                                        item.forceActiveFocus(Qt.TabFocusReason);
                                        dialog.ensureItemVisibleInFlick(consolesView, item);
                                    }
                                    let count = index > 0 ? 2: 0;
                                    for(var i = 0; i < count; i++)
                                    {
                                        let item2 = item.nextItemInFocusChain(false);
                                        if (item)
                                        {
                                            item.forceActiveFocus(Qt.TabFocusReason);
                                            dialog.ensureItemVisibleInFlick(consolesView, item);
                                            item = item2;
                                        }
                                    }
                                    event.accepted = true;
                                }
                                break;
                            case Qt.Key_Down:
                                if (!lastDownInFocusChain) {
                                    let item = nextItemInFocusChain();
                                    if (item) {
                                        item.forceActiveFocus(Qt.TabFocusReason);
                                        dialog.ensureItemVisibleInFlick(consolesView, item);
                                    }
                                    let count = 2;
                                    for(var i = 0; i < count; i++)
                                    {
                                        let item2 = item.nextItemInFocusChain();
                                        if (item)
                                        {
                                            item.forceActiveFocus(Qt.TabFocusReason);
                                            dialog.ensureItemVisibleInFlick(consolesView, item);
                                            item = item2;
                                        }
                                    }
                                    event.accepted = true;
                                }
                                break;
                            case Qt.Key_Return:
                                if (visualFocus) {
                                    toggle();
                                    toggled();
                                }
                                event.accepted = true;
                                break;
                            }
                        }
                    }

                    Button {
                        property bool firstInFocusChain: false
                        property bool lastInFocusChain: index > consolesView.count + hiddenConsolesView.count - 2
                        Material.background: visualFocus ? Material.accent : undefined

                        Component.onDestruction: {
                            if (visualFocus) {
                                let item = nextItemInFocusChain();
                                if (item)
                                    item.forceActiveFocus(Qt.TabFocusReason);
                            }
                        }
                        Keys.onPressed: (event) => {
                            switch (event.key) {
                                case Qt.Key_Left:
                                    if (!firstInFocusChain) {
                                        let item = nextItemInFocusChain(false);
                                        if (item)
                                            item.forceActiveFocus(Qt.TabFocusReason);
                                        event.accepted = true;
                                    }
                                    break;
                                case Qt.Key_Up:
                                    if (!firstInFocusChain)
                                    {
                                        let item = nextItemInFocusChain(false);
                                        if (item) {
                                            item.forceActiveFocus(Qt.TabFocusReason);
                                            dialog.ensureItemVisibleInFlick(consolesView, item);
                                        }
                                        let count = 2;
                                        for(var i = 0; i < count; i++)
                                        {
                                            let item2 = item.nextItemInFocusChain(false);
                                            if (item)
                                            {
                                                item.forceActiveFocus(Qt.TabFocusReason);
                                                dialog.ensureItemVisibleInFlick(consolesView, item);
                                                item = item2;
                                            }
                                        }
                                        event.accepted = true;
                                    }
                                    break;
                                case Qt.Key_Down:
                                    if (!lastInFocusChain) {
                                        let item = nextItemInFocusChain();
                                        if (item) {
                                            item.forceActiveFocus(Qt.TabFocusReason);
                                            dialog.ensureItemVisibleInFlick(consolesView, item);
                                        }
                                        let count = index < consolesView.count - 1 ? 2: 0;
                                        for(var i = 0; i < count; i++)
                                        {
                                            let item2 = item.nextItemInFocusChain();
                                            if (item)
                                            {
                                                item.forceActiveFocus(Qt.TabFocusReason);
                                                dialog.ensureItemVisibleInFlick(consolesView, item);
                                                item = item2;
                                            }
                                        }
                                        event.accepted = true;
                                    }
                                    break;
                                case Qt.Key_Return:
                                    if (visualFocus) {
                                        clicked();
                                    }
                                    event.accepted = true;
                                    break;
                            }
                        }
                        anchors {
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                            rightMargin: 20
                        }
                        text: qsTr("Delete")
                        onClicked: root.showConfirmDialog(qsTr("Delete Console"), qsTr("Are you sure you want to delete this console?"), () => Chiaki.settings.deleteRegisteredHost(index));
                        Material.roundedScale: Material.SmallScale
                        Material.accent: Material.Red
                    }
                }
            }
            Label {
                id: hiddenConsolesLabel
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Hidden Consoles")
                font.bold: true
            }
            ListView {
                id: hiddenConsolesView
                Layout.alignment: Qt.AlignHCenter
                keyNavigationEnabled: false
                height: 170
                onCountChanged: {
                    hiddenConsolesView.contentHeight = hiddenConsolesView.count * 80 + hiddenConsolesView.anchors.topMargin;
                }
                clip: true
                width: 500
                ScrollBar.vertical: ScrollBar {
                    id: hiddenConsolesScrollbar
                    policy: ScrollBar.AlwaysOn
                    visible: hiddenConsolesView.contentHeight > hiddenConsolesView.height
                }
                model: Chiaki.hiddenHosts
                delegate: ItemDelegate {
                    text: "%1 (%2)".arg(Chiaki.settings.streamerMode ? "hidden" : modelData.mac).arg(modelData.name)
                    height: 80
                    width: parent ? parent.width : 0

                    Button {
                        property bool firstInFocusChain: false
                        property bool lastInFocusChain: index > hiddenConsolesView.count - 2
                        Material.background: visualFocus ? Material.accent : undefined

                        Component.onDestruction: {
                            if (visualFocus) {
                                let item = nextItemInFocusChain();
                                if (item)
                                    item.forceActiveFocus(Qt.TabFocusReason);
                            }
                        }
                        Keys.onPressed: (event) => {
                            switch (event.key) {
                                case Qt.Key_Up:
                                    if (!firstInFocusChain)
                                    {
                                        let item = nextItemInFocusChain(false);
                                        if (item) {
                                            item.forceActiveFocus(Qt.TabFocusReason);
                                            dialog.ensureItemVisibleInFlick(hiddenConsolesView, item);
                                        }
                                        event.accepted = true;
                                    }
                                    break;
                                case Qt.Key_Down:
                                    if (!lastInFocusChain) {
                                        let item = nextItemInFocusChain();
                                        if (item) {
                                            item.forceActiveFocus(Qt.TabFocusReason);
                                            dialog.ensureItemVisibleInFlick(hiddenConsolesView, item);
                                        }
                                        event.accepted = true;
                                    }
                                    break;
                                case Qt.Key_Return:
                                    if (visualFocus) {
                                        clicked();
                                    }
                                    event.accepted = true;
                                    break;
                            }
                        }
                        anchors {
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                            rightMargin: 20
                        }
                        text: qsTr("Unhide")
                        onClicked: root.showConfirmDialog(qsTr("Unhide Console"), qsTr("Are you sure you want to unhide this console?"), () => Chiaki.unhideHost(modelData.mac));
                        Material.roundedScale: Material.SmallScale
                        Material.accent: Material.Red
                    }
                }
            }
        }
    }

    C.Card {
        title: qsTr("Profiles, backup and logs")
        GridLayout {
            Layout.alignment: Qt.AlignLeft
            id: configGrid
            columns: 1
            rowSpacing: 20
            columnSpacing: 10
            Label {
                text: {
                    if(Chiaki.settings.currentProfile)
                        qsTr("Current Profile: ") + Chiaki.settings.currentProfile
                    else
                        qsTr("Current Profile: default")
                }
            }
            C.Button {
                id: profile
                text: qsTr("Manage Profiles")
                onClicked: {
                    root.showProfileDialog()
                }
                Material.roundedScale: Material.SmallScale
            }
        C.Button {
            id: exportButton
            text: qsTr("Export settings to file")
            onClicked: {
                Chiaki.settings.exportSettings();
            }
            Material.roundedScale: Material.SmallScale
        }
        C.Button {
            id: importButton
            text: qsTr("Import settings from file")
            onClicked: {
                Chiaki.settings.importSettings();
            }
            Material.roundedScale: Material.SmallScale
        }
        C.Button {
            id: aboutButton
            text: qsTr("About %1-ng").arg(Qt.application.name)
            onClicked: aboutDialog.open()
            Material.roundedScale: Material.SmallScale
        }
        C.CheckBox {
            text: qsTr("Sanitize Logs (checked)")
            checked: Chiaki.settings.logSanitize
            onToggled: Chiaki.settings.logSanitize = checked
        }
            C.CheckBox {
                text: qsTr("Verbose Logging (unchecked)")
                checked: Chiaki.settings.logVerbose
                onToggled: Chiaki.settings.logVerbose = checked
            }
        }
    }

    C.Dialog {
        id: aboutDialog
        parent: Overlay.overlay
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        title: qsTr("About %1-ng").arg(Qt.application.name)
        modal: true
        standardButtons: Dialog.Ok
        Material.roundedScale: Material.MediumScale
        onAboutToHide: aboutButton.forceActiveFocus(Qt.TabFocusReason)

        RowLayout {
            spacing: 50
            onVisibleChanged: if (visible) forceActiveFocus(Qt.TabFocusReason)
            Keys.onReturnPressed: aboutDialog.close()
            Keys.onEscapePressed: aboutDialog.close()

            Image {
                Layout.preferredWidth: 200
                fillMode: Image.PreserveAspectFit
                verticalAlignment: Image.AlignTop
                source: "qrc:icons/chiaking-logo.svg"
            }

            Label {
                Layout.preferredWidth: 400
                verticalAlignment: Text.AlignTop
                wrapMode: Text.Wrap
                text: "<h1>chiaki-ng</h1> by Street Pea, version %1
                    <h2>Fork of Chiaki</h2> by Florian Markl at version 2.1.1

                    <p>This program is free software: you can redistribute it and/or modify
                    it under the terms of the GNU Affero General Public License version 3
                    as published by the Free Software Foundation.</p>

                    <p>This program is distributed in the hope that it will be useful,
                    but WITHOUT ANY WARRANTY; without even the implied warranty of
                    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
                    GNU General Public License for more details.</p>".arg(Qt.application.version)
            }
        }
    }

}
