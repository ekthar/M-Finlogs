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
    property color bg0: dark ? "#0b1020" : "#eceef5"   // deepest backdrop
    property color bg1: dark ? "#0f1530" : "#f2f4f9"   // base
    property color bg2: dark ? "#141b3a" : "#ffffff"   // raised

    property color surface: dark ? "#171f42" : "#ffffff"
    property color glass: dark ? Qt.rgba(1,1,1,0.06) : Qt.rgba(255/255,255/255,255/255,0.72)
    property color glassStrong: dark ? Qt.rgba(1,1,1,0.10) : Qt.rgba(1,1,1,0.92)
    property color glassBorder: dark ? Qt.rgba(1,1,1,0.14) : Qt.rgba(20/255,27/255,58/255,0.12)
    property color glassBorderSoft: dark ? Qt.rgba(1,1,1,0.08) : Qt.rgba(20/255,27/255,58/255,0.07)

    property color text: dark ? "#eef2ff" : "#1d2440"
    property color textDim: dark ? "#aab3d4" : "#566085"
    property color textFaint: dark ? "#6f7aa3" : "#8b93ad"

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
