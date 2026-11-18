import QtQuick
import QtQuick.Controls

// Entry of a side navigation: highlighted when `current`, outlined when it
// has keyboard focus.
AbstractButton {
    id: control
    property bool current: false
    property string iconSource
    property string badge
    implicitHeight: 56
    implicitWidth: 240
    leftPadding: 16
    rightPadding: 16
    focusPolicy: Qt.StrongFocus
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBody
    font.weight: current ? Font.DemiBold : Font.Medium

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.current ? Theme.surfaceSelected : (control.hovered || control.visualFocus ? Theme.surface : "transparent")
        border.width: control.visualFocus ? Theme.focusWidth : 0
        border.color: Theme.accent
    }

    contentItem: Item {
        implicitHeight: 24
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 14
            Image {
                visible: control.iconSource.length > 0
                anchors.verticalCenter: parent.verticalCenter
                width: 24
                height: 24
                sourceSize: Qt.size(width, height)
                source: control.iconSource
                opacity: control.current ? 1.0 : 0.65
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: control.text
                font: control.font
                color: control.current ? Theme.text : Theme.textSecondary
            }
        }
        Label {
            visible: control.badge.length > 0
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: control.badge
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontCaption
            color: Theme.textSecondary
        }
    }
}
