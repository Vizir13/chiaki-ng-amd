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

    title: qsTr("Controls")
    subtitle: qsTr("Controller mapping, keyboard keys, the stream menu shortcut and handheld options.")
    firstFocusItem: controllerMappingChange
    property bool quitControllerMapping: true

    C.Card {
        title: qsTr("Controller")
        ColumnLayout {
            Layout.alignment: Qt.AlignLeft
            id: controllersLayout
            spacing: 10
            C.Button {
                Layout.alignment: Qt.AlignHCenter
                id: controllerMappingChange
                text: "Change Controller Mapping"
                onClicked: controllerMappingDialog.show({
                    reset: false
                });
            }
            C.Button {
                Layout.alignment: Qt.AlignHCenter
                id: controllerMappingReset
                text: "Reset Controller Mapping"
                onClicked: controllerMappingDialog.show({
                    reset: true
                });
            }
            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignHCenter
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Background Controller Events:")
                }
                C.CheckBox {
                    id: backgroundController
                    text: qsTr("Process controller input when application is in background")
                    checked: {
                        Chiaki.settings.allowJoystickBackgroundEvents
                    }
                    onToggled: Chiaki.settings.allowJoystickBackgroundEvents = checked
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("(Checked)")
                }
            }
            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignHCenter
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Dpad Touchpad Emulation")
                }
                C.CheckBox {
                    id: dpadTouch
                    checked: Chiaki.settings.dpadTouchEnabled
                    onToggled: Chiaki.settings.dpadTouchEnabled = !Chiaki.settings.dpadTouchEnabled
                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    KeyNavigation.up: backgroundController
                    KeyNavigation.left: dpadTouch
                    KeyNavigation.right: dpadTouch
                    KeyNavigation.down: {
                        if(touchIncrement.visible)
                            touchIncrement
                        else
                            posButtons
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("(Checked)")
                }
            }
            RowLayout {
                id: touchIncrementLayout
                spacing: 10
                Layout.alignment: Qt.AlignHCenter
                visible: Chiaki.settings.dpadTouchEnabled
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Dpad Touch Increment:")
                }

                C.Slider {
                    id: touchIncrement
                    Layout.preferredWidth: 250
                    from: 1
                    to: 1079
                    stepSize: 1
                    value: Chiaki.settings.dpadTouchIncrement
                    onMoved: Chiaki.settings.dpadTouchIncrement = value

                    Label {
                        anchors {
                            left: parent.right
                            verticalCenter: parent.verticalCenter
                            leftMargin: 10
                        }
                        text: qsTr("%1 mm").arg(parent.value / 100)
                    }
                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    KeyNavigation.up: dpadTouch
                    KeyNavigation.down: {
                        if(dpadShortcut1.visible)
                            dpadShortcut1
                        else
                            posButtons;
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    Layout.leftMargin: 100
                    text: qsTr("(0.3 mm)")
                }
            }
            RowLayout {
                spacing: 10
                visible: Chiaki.settings.dpadTouchEnabled
                Layout.alignment: Qt.AlignHCenter
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Dpad Regular/Touch Combo:")
                }

                C.ComboBox {
                    id: dpadShortcut1
                    implicitContentWidthPolicy: ComboBox.WidestText
                    model: [qsTr("Not Used"), qsTr("Cross"), qsTr("Moon"), qsTr("Box"), qsTr("Pyramid"), qsTr("Dpad Left"), qsTr("Dpad Right"), qsTr("Dpad Up"), qsTr("Dpad Down"), qsTr("L1"), qsTr("R1"), qsTr("L3"), qsTr("R3"), qsTr("Options"), qsTr("Share"), qsTr("Touchpad"), qsTr("PS")]
                    currentIndex: Chiaki.settings.dpadTouchShortcut1
                    onActivated: index => Chiaki.settings.dpadTouchShortcut1 = index
                    KeyNavigation.priority: {
                        if(!popup.visible)
                            KeyNavigation.BeforeItem
                        else
                            KeyNavigation.AfterItem
                    }
                    KeyNavigation.up: touchIncrement
                    KeyNavigation.down: posButtons
                    KeyNavigation.left: dpadShortcut1
                    KeyNavigation.right: dpadShortcut2
                }

                C.ComboBox {
                    id: dpadShortcut2
                    implicitContentWidthPolicy: ComboBox.WidestText
                    model: [qsTr("Not Used"), qsTr("Cross"), qsTr("Moon"), qsTr("Box"), qsTr("Pyramid"), qsTr("Dpad Left"), qsTr("Dpad Right"), qsTr("Dpad Up"), qsTr("Dpad Down"), qsTr("L1"), qsTr("R1"), qsTr("L3"), qsTr("R3"), qsTr("Options"), qsTr("Share"), qsTr("Touchpad"), qsTr("PS")]
                    currentIndex: Chiaki.settings.dpadTouchShortcut2
                    onActivated: index => Chiaki.settings.dpadTouchShortcut2 = index
                    KeyNavigation.priority: {
                        if(!popup.visible)
                            KeyNavigation.BeforeItem
                        else
                            KeyNavigation.AfterItem
                    }
                    KeyNavigation.up: touchIncrement
                    KeyNavigation.down: posButtons
                    KeyNavigation.left: dpadShortcut1
                    KeyNavigation.right: dpadShortcut3
                }

                C.ComboBox {
                    id: dpadShortcut3
                    implicitContentWidthPolicy: ComboBox.WidestText
                    model: [qsTr("Not Used"), qsTr("Cross"), qsTr("Moon"), qsTr("Box"), qsTr("Pyramid"), qsTr("Dpad Left"), qsTr("Dpad Right"), qsTr("Dpad Up"), qsTr("Dpad Down"), qsTr("L1"), qsTr("R1"), qsTr("L3"), qsTr("R3"), qsTr("Options"), qsTr("Share"), qsTr("Touchpad"), qsTr("PS")]
                    currentIndex: Chiaki.settings.dpadTouchShortcut3
                    onActivated: index => Chiaki.settings.dpadTouchShortcut3 = index
                    KeyNavigation.priority: {
                        if(!popup.visible)
                            KeyNavigation.BeforeItem
                        else
                            KeyNavigation.AfterItem
                    }
                    KeyNavigation.up: touchIncrement
                    KeyNavigation.down: posButtons
                    KeyNavigation.left: dpadShortcut2
                    KeyNavigation.right: dpadShortcut4
                }

                C.ComboBox {
                    id: dpadShortcut4
                    implicitContentWidthPolicy: ComboBox.WidestText
                    model: [qsTr("Not Used"), qsTr("Cross"), qsTr("Moon"), qsTr("Box"), qsTr("Pyramid"), qsTr("Dpad Left"), qsTr("Dpad Right"), qsTr("Dpad Up"), qsTr("Dpad Down"), qsTr("L1"), qsTr("R1"), qsTr("L3"), qsTr("R3"), qsTr("Options"), qsTr("Share"), qsTr("Touchpad"), qsTr("PS")]
                    currentIndex: Chiaki.settings.dpadTouchShortcut4
                    onActivated: index => Chiaki.settings.dpadTouchShortcut4 = index
                    KeyNavigation.priority: {
                        if(!popup.visible)
                            KeyNavigation.BeforeItem
                        else
                            KeyNavigation.AfterItem
                    }
                    KeyNavigation.up: touchIncrement
                    KeyNavigation.down: posButtons
                    KeyNavigation.left: dpadShortcut3
                    KeyNavigation.right: dpadShortcut4
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("(L1+R1+dpad Up)")
                }
            }
            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignHCenter
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Buttons By Position:")
                }

                C.CheckBox {
                    id: posButtons
                    text: qsTr("Use buttons by position instead of by label")
                    checked: Chiaki.settings.buttonsByPosition
                    onToggled: Chiaki.settings.buttonsByPosition = checked
                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    KeyNavigation.up: {
                        if(dpadShortcut1.visible)
                            dpadShortcut1
                        else
                            dpadTouch;
                    }
                    KeyNavigation.down: rumbleHaptics
                    KeyNavigation.left: posButtons
                    KeyNavigation.right: posButtons
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("(Unchecked)")
                }
            }
            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignHCenter
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Rumble Haptics:")
                }

                C.ComboBox {
                    id: rumbleHaptics
                    Layout.preferredWidth: 400
                    model: [qsTr("Off"), qsTr("Very Weak"), qsTr("Weak"), qsTr("Normal"), qsTr("Strong"), qsTr("Very Strong")]
                    currentIndex: Chiaki.settings.rumbleHapticsIntensity
                    onActivated: (index) => Chiaki.settings.rumbleHapticsIntensity = index;
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("(Normal)")
                }
            }
            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignHCenter
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("True Haptics Intensity:")
                }

                C.Slider {
                    id: hapticOverride
                    Layout.preferredWidth: 250
                    from: 0
                    to: 2
                    stepSize: 0.1
                    value: Chiaki.settings.hapticOverride
                    onMoved: Chiaki.settings.hapticOverride = value;
                    Label {
                        anchors {
                            left: parent.right
                            verticalCenter: parent.verticalCenter
                            leftMargin: 10
                        }
                        text: {
                            if(parent.value > 0.99 && parent.value < 1.01)
                                qsTr("console setting")
                            else
                                (parent.value * 100).toFixed(0) + qsTr(" % console setting")
                        }
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    Layout.leftMargin: 250
                    text: qsTr("(console setting)")
                }
            }
        }
    }

    C.Card {
        title: qsTr("Stream menu and handheld")
        subtitle: qsTr("Hold the combo during a stream to open the stream menu.")
        GridLayout {
            Layout.alignment: Qt.AlignLeft
            columns: 3
            rowSpacing: 5
            columnSpacing: 20
        Label {
            text: qsTr("Steam Deck Haptics:")
            visible: (typeof Chiaki.settings.steamDeckHaptics !== "undefined")
        }
        C.CheckBox {
            text: qsTr("True haptics for SteamDeck, better quality but noisier")
            checked: {
                if(typeof Chiaki.settings.steamDeckHaptics !== "undefined")
                    Chiaki.settings.steamDeckHaptics
                else
                    false
            }
            onToggled: Chiaki.settings.steamDeckHaptics = checked
            visible: (typeof Chiaki.settings.steamDeckHaptics !== "undefined")
        }
        Label {
            text: qsTr("(Unchecked)")
            visible: (typeof Chiaki.settings.steamDeckHaptics !== "undefined")
        }
        Label {
            text: qsTr("Steam Deck Vertical:")
            visible: typeof Chiaki.settings.verticalDeck !== "undefined"
        }
        C.CheckBox {
            text: qsTr("Use Steam Deck in vertical orientation (motion controls)")
            checked: {
                if(typeof Chiaki.settings.verticalDeck !== "undefined")
                    Chiaki.settings.verticalDeck
                else
                    false
            }
            onToggled: Chiaki.settings.verticalDeck = checked
            visible: typeof Chiaki.settings.verticalDeck !== "undefined"
        }
        Label {
            text: qsTr("(Unchecked)")
            visible: typeof Chiaki.settings.verticalDeck !== "undefined"
        }
        Label {
            text: qsTr("Stream Menu Shortcut Enabled")
        }
        C.CheckBox {
            id: streamMenu
            checked: Chiaki.settings.streamMenuEnabled
            onToggled: Chiaki.settings.streamMenuEnabled = !Chiaki.settings.streamMenuEnabled
            KeyNavigation.priority: KeyNavigation.BeforeItem
            KeyNavigation.left: streamMenu
            KeyNavigation.right: streamMenu
            KeyNavigation.down: {
                if(streamMenuShortcut1.visible)
                    streamMenuShortcut1
                else
                    streamMenu
            }
        }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(Checked)")
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignLeft
            spacing: 10
            visible: Chiaki.settings.streamMenuEnabled
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Stream Menu Combo:")
            }

        C.ComboBox {
            id: streamMenuShortcut1
            implicitContentWidthPolicy: ComboBox.WidestText
            model: [qsTr("Not Used"), qsTr("Cross"), qsTr("Moon"), qsTr("Box"), qsTr("Pyramid"), qsTr("Dpad Left"), qsTr("Dpad Right"), qsTr("Dpad Up"), qsTr("Dpad Down"), qsTr("L1"), qsTr("R1"), qsTr("L3"), qsTr("R3"), qsTr("Options"), qsTr("Share"), qsTr("Touchpad"), qsTr("PS")]
            currentIndex: Chiaki.settings.streamMenuShortcut1
            onActivated: index => Chiaki.settings.streamMenuShortcut1 = index
            KeyNavigation.priority: {
                if(!popup.visible)
                    KeyNavigation.BeforeItem
                else
                    KeyNavigation.AfterItem
            }
            KeyNavigation.up: streamMenu
            KeyNavigation.down: streamMenuShortcut1
            KeyNavigation.left: streamMenuShortcut1
            KeyNavigation.right: streamMenuShortcut2
        }

        C.ComboBox {
            id: streamMenuShortcut2
            implicitContentWidthPolicy: ComboBox.WidestText
            model: [qsTr("Not Used"), qsTr("Cross"), qsTr("Moon"), qsTr("Box"), qsTr("Pyramid"), qsTr("Dpad Left"), qsTr("Dpad Right"), qsTr("Dpad Up"), qsTr("Dpad Down"), qsTr("L1"), qsTr("R1"), qsTr("L3"), qsTr("R3"), qsTr("Options"), qsTr("Share"), qsTr("Touchpad"), qsTr("PS")]
            currentIndex: Chiaki.settings.streamMenuShortcut2
            onActivated: index => Chiaki.settings.streamMenuShortcut2 = index
            KeyNavigation.priority: {
                if(!popup.visible)
                    KeyNavigation.BeforeItem
                else
                    KeyNavigation.AfterItem
            }
            KeyNavigation.up: streamMenu
            KeyNavigation.down: streamMenuShortcut2
            KeyNavigation.left: streamMenuShortcut1
            KeyNavigation.right: streamMenuShortcut3
        }

        C.ComboBox {
            id: streamMenuShortcut3
            implicitContentWidthPolicy: ComboBox.WidestText
            model: [qsTr("Not Used"), qsTr("Cross"), qsTr("Moon"), qsTr("Box"), qsTr("Pyramid"), qsTr("Dpad Left"), qsTr("Dpad Right"), qsTr("Dpad Up"), qsTr("Dpad Down"), qsTr("L1"), qsTr("R1"), qsTr("L3"), qsTr("R3"), qsTr("Options"), qsTr("Share"), qsTr("Touchpad"), qsTr("PS")]
            currentIndex: Chiaki.settings.streamMenuShortcut3
            onActivated: index => Chiaki.settings.streamMenuShortcut3 = index
            KeyNavigation.priority: {
                if(!popup.visible)
                    KeyNavigation.BeforeItem
                else
                    KeyNavigation.AfterItem
            }
            KeyNavigation.up: streamMenu
            KeyNavigation.down: streamMenuShortcut3
            KeyNavigation.left: streamMenuShortcut2
            KeyNavigation.right: streamMenuShortcut4
        }

        C.ComboBox {
            id: streamMenuShortcut4
            implicitContentWidthPolicy: ComboBox.WidestText
            model: [qsTr("Not Used"), qsTr("Cross"), qsTr("Moon"), qsTr("Box"), qsTr("Pyramid"), qsTr("Dpad Left"), qsTr("Dpad Right"), qsTr("Dpad Up"), qsTr("Dpad Down"), qsTr("L1"), qsTr("R1"), qsTr("L3"), qsTr("R3"), qsTr("Options"), qsTr("Share"), qsTr("Touchpad"), qsTr("PS")]
            currentIndex: Chiaki.settings.streamMenuShortcut4
            onActivated: index => Chiaki.settings.streamMenuShortcut4 = index
            KeyNavigation.priority: {
                if(!popup.visible)
                    KeyNavigation.BeforeItem
                else
                    KeyNavigation.AfterItem
            }
            KeyNavigation.up: streamMenu
            KeyNavigation.down: streamMenuShortcut4
            KeyNavigation.left: streamMenuShortcut3
            KeyNavigation.right: streamMenuShortcut4
        }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("(L1+R1+L3+R3)")
            }
        }
    }

    C.Card {
        title: qsTr("Keyboard")
        subtitle: qsTr("Keys that stand in for controller buttons when you play with a keyboard.")
        GridLayout {
            Layout.alignment: Qt.AlignLeft
            id: keysGrid
            columns: 3
            rowSpacing: 3
            columnSpacing: 10
            Button {
                id: resetAllKeys
                text: "Reset All Keys"
                Layout.alignment: Qt.AlignRight
                property bool firstInFocusChain: true
                property bool lastInFocusChain: false
                onClicked: Chiaki.settings.clearKeyMapping()
                Material.roundedScale: Material.SmallScale
                Material.background: visualFocus ? Material.accent : undefined
                onActiveFocusChanged: if (activeFocus) page.ensureItemVisible(this)

                Component.onDestruction: {
                    if (visualFocus) {
                        let item = nextItemInFocusChain();
                        if (item)
                            item.forceActiveFocus(Qt.TabFocusReason);
                    }
                }
                Keys.onPressed: (event) => {
                    switch (event.key) {
                    case Qt.Key_Down:
                        if (!lastInFocusChain) {
                            let item = nextItemInFocusChain();
                            if (item) {
                                item.forceActiveFocus(Qt.TabFocusReason);
                                page.ensureItemVisible(item);
                            }
                            for(var i = 0; i < 3; i++)
                            {
                                let item2 = item.nextItemInFocusChain();
                                if (item)
                                {
                                    item.forceActiveFocus(Qt.TabFocusReason);
                                    page.ensureItemVisible(item);
                                    item = item2;
                                }
                            }
                            event.accepted = true;
                        }
                        break;
                    case Qt.Key_Right:
                        if (!lastInFocusChain) {
                            let item = nextItemInFocusChain();
                            if (item) {
                                item.forceActiveFocus(Qt.TabFocusReason);
                                page.ensureItemVisible(item);
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
            }
            CheckBox {
                text: qsTr("Keyboard as controller")
                checked: {
                    Chiaki.settings.keyboardEnabled
                }
                onToggled: Chiaki.settings.keyboardEnabled = checked
                Layout.alignment: Qt.AlignRight
                property bool firstInFocusChain: false
                property bool lastInFocusChain: false
                Material.roundedScale: Material.SmallScale
                Material.background: visualFocus ? Material.accent : undefined
                onActiveFocusChanged: if (activeFocus) page.ensureItemVisible(this)

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
                                if (item) {
                                    item.forceActiveFocus(Qt.TabFocusReason);
                                    page.ensureItemVisible(item);
                                }
                                event.accepted = true;
                            }
                            break;
                        case Qt.Key_Right:
                            if  (!lastInFocusChain) {
                                let item = nextItemInFocusChain();
                                if (item) {
                                    item.forceActiveFocus(Qt.TabFocusReason);
                                    page.ensureItemVisible(item);
                                }
                                event.accepted = true;
                            }
                            break;
                        case Qt.Key_Down:
                            if (!lastInFocusChain) {
                                let item = nextItemInFocusChain();
                                if (item) {
                                    item.forceActiveFocus(Qt.TabFocusReason);
                                    page.ensureItemVisible(item);
                                }
                                for(var i = 0; i < 3; i++)
                                {
                                    let item2 = item.nextItemInFocusChain();
                                    if (item)
                                    {
                                        item.forceActiveFocus(Qt.TabFocusReason);
                                        page.ensureItemVisible(item);
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
            CheckBox {
                text: qsTr("Enable Mouse Touchpad")
                checked: {
                    Chiaki.settings.mouseTouchEnabled
                }
                onToggled: Chiaki.settings.mouseTouchEnabled = checked
                Layout.alignment: Qt.AlignRight
                property bool firstInFocusChain: false
                property bool lastInFocusChain: false
                Material.roundedScale: Material.SmallScale
                Material.background: visualFocus ? Material.accent : undefined
                onActiveFocusChanged: if (activeFocus) page.ensureItemVisible(this)

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
                                if (item) {
                                    item.forceActiveFocus(Qt.TabFocusReason);
                                    page.ensureItemVisible(item);
                                }
                                event.accepted = true;
                            }
                            break;
                        case Qt.Key_Down:
                            if (!lastInFocusChain) {
                                let item = nextItemInFocusChain();
                                if (item) {
                                    item.forceActiveFocus(Qt.TabFocusReason);
                                    page.ensureItemVisible(item);
                                }
                                for(var i = 0; i < 3; i++)
                                {
                                    let item2 = item.nextItemInFocusChain();
                                    if (item)
                                    {
                                        item.forceActiveFocus(Qt.TabFocusReason);
                                        page.ensureItemVisible(item);
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
            Repeater {
                id: chiakiKeys
                model: Chiaki.settings.controllerMapping

                RowLayout {
                    spacing: 20

                    Label {
                        Layout.preferredWidth: 200
                        horizontalAlignment: Text.AlignRight
                        text: modelData.buttonName
                    }

                    Button {
                        property bool firstInFocusChain: false
                        property bool lastInFocusChain: index == (chiakiKeys.count - 1)
                        Layout.preferredWidth: 170
                        Layout.preferredHeight: 52
                        text: modelData.keyName
                        Material.roundedScale: Material.SmallScale
                        Material.background: visualFocus ? Material.accent : undefined
                        onActiveFocusChanged: if (activeFocus) page.ensureItemVisible(this)
                        Component.onDestruction: {
                            if (visualFocus) {
                                let item = nextItemInFocusChain();
                                if (item)
                                    item.forceActiveFocus(Qt.TabFocusReason);
                            }
                        }
                        onClicked: {
                            keyDialog.show({
                                value: modelData.buttonValue,
                                mappingIndex: index,
                                callback: (name) => text = name,
                            });
                        }
                        Keys.onPressed: (event) => {
                            switch (event.key) {
                                case Qt.Key_Left:
                                    if (!firstInFocusChain && ((index % 3) != 0)) {
                                        let item = nextItemInFocusChain(false);
                                        if (item) {
                                            item.forceActiveFocus(Qt.TabFocusReason);
                                            page.ensureItemVisible(item);
                                        }
                                        event.accepted = true;
                                    }
                                    break;
                                case Qt.Key_Right:
                                    if  (!lastInFocusChain && (index % 3) != 2) {
                                        let item = nextItemInFocusChain();
                                        if (item) {
                                            item.forceActiveFocus(Qt.TabFocusReason);
                                            page.ensureItemVisible(item);
                                        }
                                        event.accepted = true;
                                    }
                                    break;
                                case Qt.Key_Up:
                                    if (!firstInFocusChain)
                                    {
                                        let item = nextItemInFocusChain(false);
                                        if (item) {
                                            item.forceActiveFocus(Qt.TabFocusReason);
                                            page.ensureItemVisible(item);
                                        }
                                        for(var i = 0; i < 3; i++)
                                        {
                                            let item2 = item.nextItemInFocusChain(false);
                                            if (item)
                                            {
                                                item.forceActiveFocus(Qt.TabFocusReason);
                                                page.ensureItemVisible(item);
                                                item = item2;
                                            }
                                        }
                                        event.accepted = true;
                                    }
                                    break;
                                case Qt.Key_Down:
                                    if (!lastInFocusChain && index < (chiakiKeys.count - 3)) {
                                        let item = nextItemInFocusChain();
                                        if (item) {
                                            item.forceActiveFocus(Qt.TabFocusReason);
                                            page.ensureItemVisible(item);
                                        }
                                        for(var i = 0; i < 3; i++)
                                        {
                                            let item2 = item.nextItemInFocusChain();
                                            if (item)
                                            {
                                                item.forceActiveFocus(Qt.TabFocusReason);
                                                page.ensureItemVisible(item);
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
                                    break;
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: keyDialog
        focus: false
        property int buttonValue
        property var buttonCallback
        property var keysIndex
        parent: Overlay.overlay
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        title: qsTr("Key Capture")
        modal: true
        standardButtons: Dialog.Close
        closePolicy: Popup.CloseOnPressOutside
        onOpened: keyLabel.forceActiveFocus(Qt.TabFocusReason)
        onClosed: {
            let item = chiakiKeys.itemAt(keysIndex)
            if(item)
            {
                let item2 = item.children[1];
                if(item2)
                    item2.forceActiveFocus(Qt.TabFocusReason);
            }
            keyLabel.focus = false;
            focus = false;
        }
        Material.roundedScale: Material.MediumScale

        function show(opts) {
            buttonValue = opts.value;
            buttonCallback = opts.callback;
            keysIndex = opts.mappingIndex;
            open();
        }

        Label {
            id: keyLabel
            focus: true
            text: qsTr("Press any key to configure button or click close")
            Keys.onReleased: (event) => {
                var name = Chiaki.settings.changeControllerKey(keyDialog.buttonValue, event.key);
                keyDialog.buttonCallback(name);
                keyDialog.close();
            }
        }
    }

    Dialog {
        id: controllerMappingDialog
        property bool resetFocus: true
        property bool resetMapping: false
        parent: Overlay.overlay
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        title: qsTr("Controller Capture")
        modal: true
        standardButtons: Dialog.Close
        closePolicy: Popup.CloseOnPressOutside
        onOpened: {
            controllerLabel.forceActiveFocus(Qt.TabFocusReason);
            Chiaki.beginControllerMapping(resetMapping);
        }
        onClosed: {
            if(resetFocus)
            {
                if(resetMapping)
                    controllerMappingReset.forceActiveFocus(Qt.TabFocusReason);
                else
                    controllerMappingChange.forceActiveFocus(Qt.TabFocusReason);
                focus = false;
            }
            else
            {
                resetFocus = true;
                focus = false;
            }
            if(quitControllerMapping)
                Chiaki.controllerMappingQuit();
            else
                quitControllerMapping = true;
        }
        Material.roundedScale: Material.MediumScale

        function show(opts) {
            resetMapping = opts.reset;
            open();
        }
        Label {
            id: controllerLabel
            text: qsTr("Choose the controller by pressing any button on the controller")
        }
    }

    Dialog {
        id: steamControllerMappingDialog
        property bool resetMapping: false
        parent: Overlay.overlay
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        title: qsTr("Controller Managed by Steam")
        modal: true
        standardButtons: Dialog.Close
        closePolicy: Popup.NoAutoClose
        onOpened: {
            steamLabel.forceActiveFocus(Qt.TabFocusReason);
        }
        onClosed: {
            if(resetMapping)
                controllerMappingReset.forceActiveFocus(Qt.TabFocusReason);
            else
                controllerMappingChange.forceActiveFocus(Qt.TabFocusReason);
            focus = false;
        }
        Material.roundedScale: Material.MediumScale

        Label {
            id: steamLabel
            wrapMode: TextEdit.Wrap
            text: qsTr("This controller is managed by Steam.\nPlease use Steam to map controller or disable Steam Input for the controller before mapping here.")
            Keys.onReturnPressed: steamControllerMappingDialog.close();
            Keys.onEscapePressed: steamControllerMappingDialog.close();
        }
    }

    Timer {
        id: openTimer
        interval: 100
        running: false
        onTriggered: {
            if(controllerMappingDialog.resetMapping)
            {
                if(!Chiaki.controllerMappingDefaultMapping)
                {
                    quitControllerMapping = false;
                    Chiaki.controllerMappingReset();
                }
                controllerMappingDialog.close();
            }
            else
            {
                controllerMappingChange.forceActiveFocus(Qt.TabFocusReason);
                root.showControllerMappingDialog();
                quitControllerMapping = false;
                controllerMappingDialog.resetFocus = false;
                controllerMappingDialog.close();
            }
        }
    }

    Connections {
        target: Chiaki

        function onControllerMappingInProgressChanged()
        {
            if(Chiaki.controllerMappingInProgress)
                openTimer.start();
        }

        function onControllerMappingSteamControllerSelected()
        {
            controllerMappingDialog.resetFocus = false;
            quitControllerMapping = false;
            steamControllerMappingDialog.resetMapping = controllerMappingDialog.resetMapping;
            controllerMappingDialog.close();
            steamControllerMappingDialog.open();
        }
    }

}
