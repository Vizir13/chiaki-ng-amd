import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// One setting: its name and a short explanation on the left, the control
// (the row's child) on the right, a divider line above unless `divider` is off.
Item {
    id: row
    property string label
    property string description
    property string defaultText   // appended to the description, e.g. "Default: Off"
    property bool divider: true
    default property alias control: controlSlot.data

    Layout.fillWidth: true
    implicitHeight: layout.implicitHeight + (divider ? 19 : 0)

    Rectangle {
        visible: row.divider
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 1
        color: Theme.divider
    }

    RowLayout {
        id: layout
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        spacing: 24

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 4
            Label {
                Layout.fillWidth: true
                text: row.label
                wrapMode: Text.Wrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLabel
                font.weight: Font.DemiBold
                color: row.enabled ? Theme.text : Theme.textDisabled
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: {
                    if (row.description && row.defaultText)
                        return row.description + "  ·  " + row.defaultText;
                    return row.description || row.defaultText;
                }
                wrapMode: Text.Wrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                color: Theme.textSecondary
            }
        }

        Item {
            id: controlSlot
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
