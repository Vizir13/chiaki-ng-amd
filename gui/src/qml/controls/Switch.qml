import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// On/off switch drawn like the mockup: a pill track with a round knob.
// Up/Down move along the focus chain, Return toggles (TV remote / controller).
AbstractButton {
    id: control
    property bool firstInFocusChain: false
    property bool lastInFocusChain: false
    checkable: true
    focusPolicy: Qt.StrongFocus
    implicitWidth: 64
    implicitHeight: 38
    padding: 0

    Accessible.role: Accessible.CheckBox
    Accessible.checked: checked

    indicator: Rectangle {
        anchors.centerIn: parent
        width: 64
        height: 36
        radius: height / 2
        color: control.checked ? Theme.accent : Theme.track
        opacity: control.enabled ? 1.0 : 0.45
        border.width: control.visualFocus ? Theme.focusWidth : 0
        border.color: control.checked ? Theme.text : Theme.accent
        Behavior on color { ColorAnimation { duration: 120 } }

        Rectangle {
            width: 28
            height: 28
            radius: 14
            y: 4
            x: control.checked ? parent.width - width - 4 : 4
            color: control.checked ? Theme.textOnAccent : Theme.textSecondary
            Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }
    }
    contentItem: Item {}
    background: Item {}

    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Up:
            if (!firstInFocusChain) {
                let item = nextItemInFocusChain(false);
                if (item)
                    item.forceActiveFocus(Qt.TabFocusReason);
                event.accepted = true;
            }
            break;
        case Qt.Key_Down:
            if (!lastInFocusChain) {
                let item = nextItemInFocusChain();
                if (item)
                    item.forceActiveFocus(Qt.TabFocusReason);
                event.accepted = true;
            }
            break;
        case Qt.Key_Return:
        case Qt.Key_Enter:
            toggle();
            toggled();
            clicked();
            event.accepted = true;
            break;
        }
    }
}
