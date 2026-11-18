import QtQuick
import QtQuick.Controls

// Small rounded label, optionally with a status dot.
Rectangle {
    id: chip
    property alias text: label.text
    property color textColor: Theme.textSecondary
    property color dotColor: textColor
    property bool dot: false
    implicitWidth: row.implicitWidth + 26
    implicitHeight: 34
    radius: height / 2
    color: Theme.surfaceRaised

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 8
        Rectangle {
            visible: chip.dot
            anchors.verticalCenter: parent.verticalCenter
            width: 9
            height: 9
            radius: 4.5
            color: chip.dotColor
        }
        Label {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontCaption
            font.weight: Font.DemiBold
            color: chip.textColor
        }
    }
}
