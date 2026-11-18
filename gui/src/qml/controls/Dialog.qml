import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Popup dialog in the app theme: a raised, rounded panel with a border, a
// heading in the display font and a dim scrim behind it.
Dialog {
    id: control

    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBody
    padding: 28
    topPadding: 18
    Material.background: Theme.surface
    Material.foreground: Theme.text
    Material.accent: Theme.accent

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLarge
        border.width: 1
        border.color: Theme.border
    }

    header: Label {
        text: control.title
        visible: text.length > 0
        topPadding: 28
        leftPadding: 28
        rightPadding: 28
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        font.family: Theme.displayFamily
        font.weight: Font.Bold
        font.pixelSize: Theme.fontTitle + 4
        color: Theme.text
        background: null
    }

    // Dim the window behind the dialog. The overlay layer has no size in
    // chiaki-ng's offscreen-rendered window, so the popup dimmer (which
    // fills it) never shows: this one is sized to the window instead.
    Rectangle {
        id: scrim
        parent: control.parent
        x: 0
        y: 0
        width: Window.width
        height: Window.height
        z: -1
        visible: opacity > 0
        opacity: control.visible && control.modal ? 1.0 : 0.0
        color: Qt.rgba(0.02, 0.03, 0.05, 0.72)
        Behavior on opacity { NumberAnimation { duration: 150 } }
    }
}
