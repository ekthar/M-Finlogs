pragma Singleton
import QtQuick
import QtCore

// "Aurora" design system — global design tokens with light/dark palettes
// and a global animation on/off switch. Preferences persist via QtCore Settings.
QtObject {
    id: theme

    // ---- User preferences (persisted) ------------------------------------
    property bool dark: true
    property bool animationsEnabled: true

    property Settings _prefs: Settings {
        category: "ui"
        property alias dark: theme.dark
        property alias animationsEnabled: theme.animationsEnabled
    }

    // ---- Palette (switches on `dark`) ------------------------------------
    // Dark = Aurora indigo night. Light = soft, low-glare off-white (easy on eyes).
    property color bg0: dark ? "#0b1020" : "#e8ebf3"   // deepest backdrop
    property color bg1: dark ? "#0f1530" : "#eef1f7"   // base
    property color bg2: dark ? "#141b3a" : "#f4f6fb"   // raised

    property color surface: dark ? "#171f42" : "#ffffff"
    // Light mode panels are (near) opaque so the aurora gradient/blobs don't
    // bleed through and grey them out. Dark mode stays translucent for glass.
    property color glass: dark ? Qt.rgba(1,1,1,0.06) : "#ffffff"
    property color glassStrong: dark ? Qt.rgba(1,1,1,0.10) : "#eef1f8"
    property color glassBorder: dark ? Qt.rgba(1,1,1,0.14) : "#d4d9e6"
    property color glassBorderSoft: dark ? Qt.rgba(1,1,1,0.08) : "#e6e9f2"
    // Subtle row-stripe + grid line tints that read correctly in both themes
    property color rowAlt: dark ? Qt.rgba(1,1,1,0.025) : Qt.rgba(20/255,27/255,58/255,0.025)
    property color rowHover: dark ? Qt.rgba(1,1,1,0.07) : Qt.rgba(91/255,99/255,230/255,0.08)
    property color gridLine: dark ? Qt.rgba(1,1,1,0.04) : Qt.rgba(20/255,27/255,58/255,0.06)

    property color text: dark ? "#eef2ff" : "#16203c"
    property color textDim: dark ? "#aab3d4" : "#47506e"
    property color textFaint: dark ? "#6f7aa3" : "#6b7494"

    property color accent: dark ? "#7c8cff" : "#5b63e6"        // indigo
    property color accent2: dark ? "#a78bfa" : "#8b5cf6"       // violet
    property color accent3: dark ? "#56ccf2" : "#0ea5e9"       // sky
    property color accentInk: "#ffffff"

    property color success: dark ? "#34d399" : "#0f9d6b"
    property color warning: dark ? "#fbbf24" : "#d97706"
    property color danger: dark ? "#fb7185" : "#e11d48"

    // Gradient stops used across the app shell + hero surfaces
    property color grad0: "#6366f1"
    property color grad1: "#8b5cf6"
    property color grad2: "#38bdf8"

    // ---- Typography ------------------------------------------------------
    readonly property string fontFamily: "Inter Tight"
    readonly property string monoFamily: "Space Mono"

    readonly property int fsDisplay: 34
    readonly property int fsTitle: 22
    readonly property int fsSection: 15
    readonly property int fsBody: 13
    readonly property int fsSmall: 12
    readonly property int fsTiny: 11

    // ---- Spacing scale (4pt grid) ---------------------------------------
    readonly property int s1: 4
    readonly property int s2: 8
    readonly property int s3: 12
    readonly property int s4: 16
    readonly property int s5: 20
    readonly property int s6: 24
    readonly property int s8: 32
    readonly property int s10: 40

    // ---- Radius ----------------------------------------------------------
    readonly property int rSm: 8
    readonly property int rMd: 12
    readonly property int rLg: 16
    readonly property int rXl: 22
    readonly property int rPill: 999

    // ---- Motion (durations become 0 when animations are disabled) --------
    readonly property int durFast: animationsEnabled ? 140 : 0
    readonly property int durBase: animationsEnabled ? 240 : 0
    readonly property int durSlow: animationsEnabled ? 420 : 0
    readonly property int easeOut: Easing.OutCubic
    readonly property int easeInOut: Easing.InOutCubic
    readonly property int easeSpring: animationsEnabled ? Easing.OutBack : Easing.OutCubic

    // ---- Helpers ---------------------------------------------------------
    function alpha(c, a) {
        return Qt.rgba(c.r, c.g, c.b, a)
    }

    // Color for transaction type chips
    function typeColor(type) {
        switch (type) {
        case "Sale": return success
        case "Receipt": return accent3
        case "Expense": return danger
        case "Sale Return": return warning
        case "Purchase": return accent2
        case "Critical": return danger
        case "High": return warning
        case "Normal": return success
        case "No receipt": return textDim
        default: return textDim
        }
    }
}
