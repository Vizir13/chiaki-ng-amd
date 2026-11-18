pragma Singleton
import QtQuick

// The one place that defines chiaki-ng's look. Everything else refers to
// these names instead of literal colors, sizes or font families.
// qtquickcontrols2.conf repeats the four Material colors for windows that
// are created before any QML item can set them.
QtObject {
    // Surfaces, darkest to lightest
    readonly property color bgDeep: "#0A0E13"      // side navigation
    readonly property color bg: "#0D1117"          // page background
    readonly property color surface: "#151B23"     // cards
    readonly property color surfaceInset: "#101722" // picture panel inside a card
    readonly property color surfaceRaised: "#1C2430" // tiles inside cards
    readonly property color surfaceSelected: "#172130" // selected nav item
    readonly property color surfaceAccent: "#12263A"   // selected preset card
    readonly property color border: "#263041"
    readonly property color borderSubtle: "#1F2733"
    readonly property color divider: "#222B38"
    readonly property color track: "#2A3444"       // switch track when off

    // Text
    readonly property color text: "#E8EDF4"
    readonly property color textSecondary: "#9AA7B8"
    readonly property color textSegment: "#C4CEDB"
    readonly property color textDisabled: "#5B6778"

    // Accent
    readonly property color accent: "#4CC2FF"
    readonly property color accentHover: "#8AD8FF"
    readonly property color textOnAccent: "#06121C"

    // Status
    readonly property color good: "#7FD99A"
    readonly property color goodBg: "#12301F"
    readonly property color warn: "#F5A524"
    readonly property color warnBg: "#33260F"
    readonly property color bad: "#FF8A8A"
    readonly property color badBg: "#3A1A1C"

    // Overlay on top of the video
    readonly property color scrim: Qt.rgba(10 / 255, 14 / 255, 19 / 255, 0.80)
    readonly property color scrimStrong: Qt.rgba(10 / 255, 14 / 255, 19 / 255, 0.88)
    readonly property color scrimBorder: Qt.rgba(1, 1, 1, 0.12)

    // Fonts. Qt falls back to the system UI font when a family is missing.
    readonly property string fontFamily: "Inter"
    readonly property string displayFamily: "Space Grotesk"

    // Type scale (pixels; the app is used from a couch, so it runs large)
    readonly property int fontCaption: 14
    readonly property int fontSmall: 16
    readonly property int fontBody: 18
    readonly property int fontLabel: 19
    readonly property int fontTitle: 22
    readonly property int fontHeading: 30
    readonly property int fontDisplay: 38

    // Shape
    readonly property int radiusSmall: 10
    readonly property int radius: 14
    readonly property int radiusLarge: 20
    readonly property int controlHeight: 52
    readonly property int focusWidth: 3

    // Spacing
    readonly property int gap: 12
    readonly property int pad: 24
    readonly property int pagePad: 48
}
