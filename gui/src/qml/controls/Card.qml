import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// A rounded surface holding a group of settings, with an optional title.
Rectangle {
    id: card
    property string title
    property string subtitle
    default property alias content: column.data
    property alias spacing: column.spacing
    property int padding: Theme.pad + 4

    Layout.fillWidth: true
    implicitHeight: outer.implicitHeight + padding * 2
    radius: Theme.radiusLarge - 4
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    ColumnLayout {
        id: outer
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: card.padding
        }
        spacing: 18

        ColumnLayout {
            visible: card.title.length > 0
            spacing: 4
            Label {
                text: card.title
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLabel + 1
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Label {
                visible: card.subtitle.length > 0
                Layout.fillWidth: true
                text: card.subtitle
                wrapMode: Text.Wrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                color: Theme.textSecondary
            }
        }

        ColumnLayout {
            id: column
            Layout.fillWidth: true
            spacing: 18
        }
    }
}
