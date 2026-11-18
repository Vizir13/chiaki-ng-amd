import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Segmented control: one focus stop for the whole group. Left/Right pick a
// segment (applied at once, like a radio group), Up/Down leave the group.
FocusScope {
    id: control
    property var model: []              // list of labels
    property var enabledSegments: []    // optional list of bools, same length as model
    property int currentIndex: 0
    property bool firstInFocusChain: false
    property bool lastInFocusChain: false
    // Clicking a segment moves the keyboard focus to the control
    property bool takeFocus: true
    signal activated(int index)

    activeFocusOnTab: true
    implicitWidth: row.implicitWidth + 10
    implicitHeight: 54
    opacity: enabled ? 1.0 : 0.45

    function segmentEnabled(i) {
        return !enabledSegments || enabledSegments.length <= i || enabledSegments[i];
    }
    function step(dir) {
        let i = currentIndex + dir;
        while (i >= 0 && i < model.length && !segmentEnabled(i))
            i += dir;
        if (i >= 0 && i < model.length && i !== currentIndex)
            activated(i);
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        color: Theme.bg
        border.width: control.activeFocus ? Theme.focusWidth : 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 4
        Repeater {
            model: control.model
            delegate: Rectangle {
                id: seg
                required property var modelData
                required property int index
                readonly property bool selected: index === control.currentIndex
                readonly property bool usable: control.segmentEnabled(index)
                width: Math.max(84, segLabel.implicitWidth + 36)
                height: control.height - 10
                radius: Theme.radiusSmall
                color: selected ? Theme.accent : (segMouse.containsMouse && usable ? Theme.surfaceRaised : "transparent")
                Label {
                    id: segLabel
                    anchors.centerIn: parent
                    text: seg.modelData
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    font.weight: Font.DemiBold
                    color: seg.selected ? Theme.textOnAccent : (seg.usable ? Theme.textSegment : Theme.textDisabled)
                }
                MouseArea {
                    id: segMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: seg.usable && control.enabled
                    onClicked: {
                        if (control.takeFocus)
                            control.forceActiveFocus(Qt.MouseFocusReason);
                        if (seg.index !== control.currentIndex)
                            control.activated(seg.index);
                    }
                }
            }
        }
    }

    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Left:
            step(-1);
            event.accepted = true;
            break;
        case Qt.Key_Right:
            step(1);
            event.accepted = true;
            break;
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
        }
    }
}
