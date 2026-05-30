pragma Singleton
import QtQuick

// "Aurora" design system — global design tokens.
// A single source of truth for color, type, spacing, radius and motion.
QtObject {
    id: theme

    // ---- Palette: Aurora (deep indigo night with violet/sky aurora) ------
    readonly property color bg0: "#0b1020"          // deepest backdrop
    readonly property color bg1: "#0f1530"          // base
    readonly property color bg2: "#141b3a"          // raised

    readonly property color surface: "#171f42"      // opaque card fallback
    readonly property color glass: Qt.rgba(1, 1, 1, 0.06)
    readonly property color glassStrong: Qt.rgba(1, 1, 1, 0.10)
    readonly property color glassBorder: Qt.rgba(1, 1, 1, 0.14)
    readonly property color glassBorderSoft: Qt.rgba(1, 1, 1, 0.08)

    readonly property color text: "#eef2ff"
    readonly property color textDim: "#aab3d4"
    readonly property color textFaint: "#6f7aa3"

    readonly property color accent: "#7c8cff"        // indigo
    readonly property color accent2: "#a78bfa"       // violet
    readonly property color accent3: "#56ccf2"       // sky
    readonly property color accentInk: "#ffffff"

    readonly property color success: "#34d399"
    readonly property color warning: "#fbbf24"
    readonly property color danger: "#fb7185"

    // Gradient stops used across the app shell + hero surfaces
    readonly property color grad0: "#6366f1"
    readonly property color grad1: "#8b5cf6"
    readonly property color grad2: "#38bdf8"

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

    // ---- Motion ----------------------------------------------------------
    readonly property int durFast: 140
    readonly property int durBase: 240
    readonly property int durSlow: 420
    readonly property int easeOut: Easing.OutCubic
    readonly property int easeInOut: Easing.InOutCubic
    readonly property int easeSpring: Easing.OutBack

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
        // Outstanding risk statuses
        case "Critical": return danger
        case "High": return warning
        case "Normal": return success
        case "No receipt": return textDim
        default: return textDim
        }
    }
}
