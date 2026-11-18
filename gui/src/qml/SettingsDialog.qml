import QtCore
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs

import org.streetpea.chiaking

import "controls" as C

DialogView {
    enum Console {
        PS4,
        PS5
    }
    property string initialPage
    id: dialog
    title: qsTr("Settings")
    header: qsTr("Defaults are shown in ( ) or after the description")
    buttonVisible: false
    function flickContainsItem(flick, item) {
        let current = item;
        while (current) {
            if (current === flick || current === flick.contentItem)
                return true;
            current = current.parent;
        }
        return false;
    }
    function flickVisibilityTarget(flick, item) {
        let current = item;
        while (current && current !== flick && current !== flick.contentItem) {
            if (current.parent === flick.contentItem)
                return current;
            if (current.parent && current.parent.parent === flick.contentItem)
                return current;
            current = current.parent;
        }
        return item;
    }
    function ensureItemVisibleInFlick(flick, item) {
        if (!flick || !item || !flickContainsItem(flick, item))
            return;
        const target = flickVisibilityTarget(flick, item);
        const top = target.mapToItem(flick.contentItem, 0, 0).y;
        const bottom = top + target.height;
        if (top < flick.contentY)
            flick.contentY = top;
        else if (bottom > flick.contentY + flick.height)
            flick.contentY = bottom - flick.height;
    }
    function itemIsInsideNestedScrollable(flick, item) {
        let current = item ? item.parent : null;
        while (current && current !== flick && current !== flick.contentItem) {
            if (current.contentY !== undefined && current.contentHeight !== undefined && current !== flick)
                return true;
            current = current.parent;
        }
        return false;
    }
    function nestedScrollableTarget(flick, item) {
        let current = item;
        while (current && current !== flick && current !== flick.contentItem) {
            if (current.contentY !== undefined && current.contentHeight !== undefined && current !== flick)
                return current;
            current = current.parent;
        }
        return item;
    }

    readonly property var pageIds: ["picture", "smooth", "stream", "controls", "audio", "advanced"]
    readonly property var pageNames: [qsTr("Picture quality"), qsTr("Smoothness"), qsTr("Stream & network"), qsTr("Controls"), qsTr("Audio"), qsTr("Advanced")]
    property int currentPage: Math.max(0, pageIds.indexOf(initialPage))

    function currentPageItem() {
        return pages.children[currentPage] || null;
    }
    function activeSettingsFlick() {
        return currentPageItem();
    }
    // The SettingRow around a control, so its label scrolls into view too
    function rowOf(item, flick) {
        let current = item;
        while (current && current !== flick && current !== flick.contentItem) {
            if (current.defaultText !== undefined && current.description !== undefined)
                return current;
            current = current.parent;
        }
        return item;
    }
    function ensureActiveFocusVisible() {
        const flick = activeSettingsFlick();
        const window = dialog.Window.window;
        if (!flick || !window || !window.activeFocusItem || !flickContainsItem(flick, window.activeFocusItem))
            return;
        const target = itemIsInsideNestedScrollable(flick, window.activeFocusItem)
            ? nestedScrollableTarget(flick, window.activeFocusItem)
            : rowOf(window.activeFocusItem, flick);
        flick.ensureItemVisible(target);
    }
    function focusCurrentPageFirstItem() {
        const p = currentPageItem();
        if (!p)
            return;
        let item = p.firstFocusItem;
        if (!item || !item.visible)
            item = p.nextItemInFocusChain();
        if (item)
            item.forceActiveFocus(Qt.TabFocusReason);
        p.contentY = 0;
    }
    function showPage(index) {
        if (index < 0 || index >= pageIds.length)
            return;
        searchField.text = "";
        currentPage = index;
        Qt.callLater(focusCurrentPageFirstItem);
    }

    // Search: the texts of the pages are read when searching, so nothing
    // has to be kept in sync by hand.
    property var searchResults: []
    function cleanText(t) {
        t = t.trim();
        while (t.length && (t.endsWith(":") || t.endsWith("*")))
            t = t.substring(0, t.length - 1).trim();
        return t;
    }
    function collectTexts(item, pageIndex, out, seen) {
        if (!item)
            return;
        let t = "";
        if (typeof item.label === "string" && item.description !== undefined)
            t = item.label;                       // SettingRow
        else if (typeof item.title === "string" && item.subtitle !== undefined && item.padding !== undefined)
            t = item.title;                       // Card
        else if (typeof item.text === "string" && item.font !== undefined)
            t = item.text;                        // Label, Button, CheckBox
        t = cleanText(t);
        if (t.length >= 3 && t.length <= 70 && !(t.startsWith("(") && t.endsWith(")")) && !seen[t]) {
            seen[t] = true;
            out.push({ text: t, page: pageIndex, item: item });
        }
        const kids = item.children;
        for (let i = 0; i < kids.length; ++i)
            collectTexts(kids[i], pageIndex, out, seen);
    }
    function runSearch(query) {
        const q = query.trim().toLowerCase();
        if (!q.length) {
            searchResults = [];
            return;
        }
        let all = [];
        for (let p = 0; p < pages.children.length; ++p) {
            if (pageNames[p].toLowerCase().indexOf(q) >= 0)
                all.push({ text: pageNames[p], page: p, item: null });
            collectTexts(pages.children[p].contentItem, p, all, {});
        }
        searchResults = all.filter(r => r.text.toLowerCase().indexOf(q) >= 0).slice(0, 12);
    }
    function openResult(r) {
        searchField.text = "";
        currentPage = r.page;
        Qt.callLater(() => {
            const p = currentPageItem();
            if (!r.item) {
                focusCurrentPageFirstItem();
                return;
            }
            const row = rowOf(r.item, p);
            let target = r.item.activeFocusOnTab ? r.item : r.item.nextItemInFocusChain();
            if (target && flickContainsItem(p, target))
                target.forceActiveFocus(Qt.TabFocusReason);
            Qt.callLater(() => p.ensureItemVisible(row));
        });
    }

    Keys.onPressed: (event) => {
        if (event.modifiers & ~Qt.KeypadModifier)
            return;
        switch (event.key) {
        case Qt.Key_PageUp:
            showPage((currentPage + pageIds.length - 1) % pageIds.length);
            event.accepted = true;
            break;
        case Qt.Key_PageDown:
            showPage((currentPage + 1) % pageIds.length);
            event.accepted = true;
            break;
        case Qt.Key_Up:
        {
            const flick = activeSettingsFlick();
            if (!flick || flick.contentHeight <= flick.height || flick.contentY <= 0.001)
                return;
            flick.flick(0, 500);
            event.accepted = true;
            break;
        }
        case Qt.Key_Down:
        {
            const flick = activeSettingsFlick();
            if (!flick || flick.contentHeight <= flick.height || flick.contentY >= flick.contentHeight - flick.height - 0.001)
                return;
            flick.flick(0, -500);
            event.accepted = true;
            break;
        }
        }
    }

    Component.onCompleted: Qt.callLater(focusCurrentPageFirstItem)

    Item {
        RowLayout {
            anchors.fill: parent
            spacing: 0

            // Section navigation
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 320
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
                        topMargin: 28
                        bottomMargin: 24
                    }
                    spacing: 4

                    // Search
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 18
                        implicitHeight: 52
                        radius: C.Theme.radiusSmall
                        color: C.Theme.surface
                        border.width: searchField.activeFocus ? C.Theme.focusWidth - 1 : 1
                        border.color: searchField.activeFocus ? C.Theme.accent : C.Theme.border
                        RowLayout {
                            anchors { fill: parent; leftMargin: 14; rightMargin: 10 }
                            spacing: 10
                            Image {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                sourceSize: Qt.size(20, 20)
                                source: "qrc:/icons/ui/search.svg"
                            }
                            TextInput {
                                id: searchField
                                Layout.fillWidth: true
                                activeFocusOnTab: false
                                clip: true
                                color: C.Theme.text
                                selectionColor: C.Theme.accent
                                selectedTextColor: C.Theme.textOnAccent
                                font.family: C.Theme.fontFamily
                                font.pixelSize: C.Theme.fontSmall
                                verticalAlignment: TextInput.AlignVCenter
                                Accessible.role: Accessible.EditableText
                                Accessible.name: qsTr("Search settings")
                                Label {
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: !searchField.text.length
                                    text: qsTr("Search settings")
                                    font: searchField.font
                                    color: C.Theme.textSecondary
                                }
                                onTextChanged: dialog.runSearch(text)
                                Keys.onEscapePressed: (event) => {
                                    if (text.length)
                                        text = "";
                                    else
                                        dialog.focusCurrentPageFirstItem();
                                    event.accepted = true;
                                }
                                Keys.onReturnPressed: {
                                    if (dialog.searchResults.length)
                                        dialog.openResult(dialog.searchResults[0]);
                                }
                                Keys.onDownPressed: {
                                    if (resultsRepeater.count)
                                        resultsRepeater.itemAt(0).forceActiveFocus(Qt.TabFocusReason);
                                }
                            }
                        }
                    }

                    // Sections
                    Repeater {
                        model: searchField.text.length ? 0 : dialog.pageNames
                        delegate: C.NavItem {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            text: modelData
                            current: index === dialog.currentPage
                            focusPolicy: Qt.NoFocus
                            onClicked: dialog.showPage(index)
                        }
                    }

                    // Search results
                    Label {
                        visible: searchField.text.length > 0
                        Layout.leftMargin: 8
                        Layout.bottomMargin: 6
                        text: dialog.searchResults.length ? qsTr("%1 found").arg(dialog.searchResults.length) : qsTr("Nothing found")
                        font.pixelSize: C.Theme.fontCaption
                        color: C.Theme.textSecondary
                    }
                    Repeater {
                        id: resultsRepeater
                        model: searchField.text.length ? dialog.searchResults : []
                        delegate: AbstractButton {
                            id: result
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            implicitHeight: resultColumn.implicitHeight + 18
                            focusPolicy: Qt.StrongFocus
                            activeFocusOnTab: false
                            onClicked: dialog.openResult(modelData)
                            Keys.onReturnPressed: dialog.openResult(modelData)
                            Keys.onUpPressed: {
                                if (index > 0)
                                    resultsRepeater.itemAt(index - 1).forceActiveFocus(Qt.TabFocusReason);
                                else
                                    searchField.forceActiveFocus(Qt.TabFocusReason);
                            }
                            Keys.onDownPressed: {
                                if (index < resultsRepeater.count - 1)
                                    resultsRepeater.itemAt(index + 1).forceActiveFocus(Qt.TabFocusReason);
                            }
                            Keys.onEscapePressed: searchField.forceActiveFocus(Qt.TabFocusReason)
                            background: Rectangle {
                                radius: C.Theme.radiusSmall
                                color: result.hovered || result.activeFocus ? C.Theme.surface : "transparent"
                                border.width: result.activeFocus ? C.Theme.focusWidth : 0
                                border.color: C.Theme.accent
                            }
                            contentItem: Column {
                                id: resultColumn
                                leftPadding: 12
                                spacing: 2
                                Label {
                                    width: result.width - 24
                                    elide: Text.ElideRight
                                    text: result.modelData.text
                                    font.pixelSize: C.Theme.fontSmall
                                    font.weight: Font.DemiBold
                                    color: C.Theme.text
                                }
                                Label {
                                    text: dialog.pageNames[result.modelData.page]
                                    font.pixelSize: C.Theme.fontCaption
                                    color: C.Theme.textSecondary
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.leftMargin: 8
                        spacing: 8
                        Image {
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 26
                            sourceSize: Qt.size(26, 26)
                            source: "qrc:/icons/l1.svg"
                        }
                        Image {
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 26
                            sourceSize: Qt.size(26, 26)
                            source: "qrc:/icons/r1.svg"
                        }
                        Label {
                            text: qsTr("switch sections")
                            font.pixelSize: C.Theme.fontCaption
                            color: C.Theme.textSecondary
                        }
                    }
                }
            }

            StackLayout {
                id: pages
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: dialog.currentPage
                onCurrentIndexChanged: Qt.callLater(dialog.ensureActiveFocusVisible)

                SettingsPicturePage { }
                SettingsSmoothnessPage { }
                SettingsStreamPage { }
                SettingsControlsPage { }
                SettingsAudioPage { }
                SettingsAdvancedPage { }
            }
        }

        Connections {
            target: dialog.Window.window
            function onActiveFocusItemChanged() {
                dialog.ensureActiveFocusVisible()
            }
        }
    }
}
