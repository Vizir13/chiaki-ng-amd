import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// A scrolling settings page: title, one-line explanation, then cards.
// SettingsDialog keeps the focused control in view and moves focus to
// `firstFocusItem` when the page opens.
Flickable {
    id: page
    property string title
    property string subtitle
    property Item firstFocusItem
    default property alias content: column.data
    property int maxContentWidth: 1180

    clip: true
    contentWidth: width
    contentHeight: outer.implicitHeight + 80
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick
    ScrollBar.vertical: ScrollBar {
        policy: ScrollBar.AsNeeded
        visible: page.contentHeight > page.height
    }

    // Scrolls so that `item` (any descendant) is fully visible
    function ensureItemVisible(item) {
        if (!item)
            return;
        const top = item.mapToItem(page.contentItem, 0, 0).y;
        const bottom = top + item.height;
        if (top - 24 < contentY)
            contentY = Math.max(0, top - 24);
        else if (bottom + 24 > contentY + height)
            contentY = Math.min(Math.max(0, contentHeight - height), bottom + 24 - height);
    }

    ColumnLayout {
        id: outer
        x: Theme.pagePad
        y: 40
        width: Math.max(400, Math.min(page.width - Theme.pagePad * 2, page.maxContentWidth))
        spacing: 28

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                text: page.title
                font.family: Theme.displayFamily
                font.weight: Font.Bold
                font.pixelSize: Theme.fontHeading + 4
                color: Theme.text
            }
            Label {
                Layout.fillWidth: true
                visible: page.subtitle.length > 0
                text: page.subtitle
                wrapMode: Text.Wrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                color: Theme.textSecondary
            }
        }

        ColumnLayout {
            id: column
            Layout.fillWidth: true
            spacing: 24
        }
    }
}
