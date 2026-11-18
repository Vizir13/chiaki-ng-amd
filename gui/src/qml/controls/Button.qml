import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Button {
    id: control
    property bool firstInFocusChain: false
    property bool lastInFocusChain: false
    property bool sendOutput: false
    // Filled with the accent color (the main action of a screen)
    property bool primary: false
    property real radius: Theme.radiusSmall
    // Controller button drawn at the left (e.g. root.controllerButton("cross"))
    property string glyph
    // Keep the border on a flat button
    property bool outlined: false

    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBody
    font.weight: Font.DemiBold
    leftPadding: glyph.length ? 56 : 22
    rightPadding: 22
    topPadding: 12
    bottomPadding: 12
    implicitHeight: Math.max(Theme.controlHeight, implicitContentHeight + topPadding + bottomPadding)
    Material.foreground: primary ? Theme.textOnAccent : (enabled ? Theme.text : Theme.textDisabled)

    background: Rectangle {
        implicitHeight: Theme.controlHeight
        radius: control.radius
        color: {
            if (control.primary)
                return control.down ? Theme.accentHover : Theme.accent;
            if (control.visualFocus || control.down)
                return Theme.surfaceSelected;
            if (control.flat)
                return control.hovered ? Theme.surface : "transparent";
            return control.hovered ? Theme.surfaceRaised : Theme.surface;
        }
        border.width: control.visualFocus ? Theme.focusWidth : (control.primary || (control.flat && !control.hovered && !control.outlined) ? 0 : 1)
        border.color: control.visualFocus ? (control.primary ? Theme.text : Theme.accent) : Theme.border
        opacity: control.enabled ? 1.0 : 0.5
    }

    Image {
        visible: control.glyph.length > 0
        anchors {
            left: parent.left
            leftMargin: 16
            verticalCenter: parent.verticalCenter
        }
        width: 28
        height: 28
        sourceSize: Qt.size(width, height)
        source: control.glyph
    }

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
            if (!firstInFocusChain) {
                let item = nextItemInFocusChain(false);
                if (item)
                    item.forceActiveFocus(Qt.TabFocusReason);
                if(!sendOutput)
                    event.accepted = true;
            }
            break;
        case Qt.Key_Down:
            if (!lastInFocusChain) {
                let item = nextItemInFocusChain();
                if (item)
                    item.forceActiveFocus(Qt.TabFocusReason);
                if(!sendOutput)
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
}
