pragma Singleton
import QtQuick
import QtCore

QtObject {
    id: theme

    // ---- User preferences ------------------------------------------------
    property bool dark: true
    property bool animationsEnabled: true

    // Follow the OS dark/light mode preference via Qt.styleHints.colorScheme
    property bool followSystemTheme: true

    property QtObject _systemTheme: QtObject {
        Component.onCompleted: {
            Qt.styleHints.colorSchemeChanged.connect(function() {
                if (theme.followSystemTheme) {
                    theme.dark = Qt.styleHints.colorScheme === Qt.Dark
                }
            })
            if (theme.followSystemTheme) {
                theme.dark = Qt.styleHints.colorScheme === Qt.Dark
            }
        }
    }
    onFollowSystemThemeChanged: {
        if (followSystemTheme) {
            dark = Qt.styleHints.colorScheme === Qt.Dark
        }
    }

    property Settings _prefs: Settings {
        category: "ui"
        property alias dark: theme.dark
        property alias animationsEnabled: theme.animationsEnabled
    }

    // ======================================================================
    // PALETTE — two scopes, switched via `palette` property
    // ======================================================================
    readonly property QtObject lightPalette: QtObject {
        property color bg:              "#ffffff"
        property color bgSubtle:        "#fafafa"
        property color bgMuted:         "#f4f4f5"
        property color surface:         "#ffffff"
        property color surfaceHover:    "#f4f4f5"
        property color popover:         "#ffffff"
        property color border:          "#e4e4e7"
        property color borderSubtle:    "#f4f4f5"
        property color inputBg:         "#ffffff"
        property color inputBorder:     "#d4d4d8"
        property color fg:              "#18181b"
        property color fgMuted:         "#71717a"
        property color fgSubtle:        "#a1a1aa"
        property color fgInverse:       "#fafafa"
        property color primary:         "#18181b"
        property color primaryFg:       "#fafafa"
        property color secondary:       "#f4f4f5"
        property color secondaryFg:     "#18181b"
        property color success:         "#16a34a"
        property color warning:         "#d97706"
        property color danger:          "#dc2626"
        property color info:            "#2563eb"
    }

    readonly property QtObject darkPalette: QtObject {
        property color bg:              "#09090b"
        property color bgSubtle:        "#18181b"
        property color bgMuted:         "#27272a"
        property color surface:         "#18181b"
        property color surfaceHover:    "#27272a"
        property color popover:         "#18181b"
        property color border:          "#27272a"
        property color borderSubtle:    "#18181b"
        property color inputBg:         "#09090b"
        property color inputBorder:     "#3f3f46"
        property color fg:              "#fafafa"
        property color fgMuted:         "#a1a1aa"
        property color fgSubtle:        "#71717a"
        property color fgInverse:       "#18181b"
        property color primary:         "#fafafa"
        property color primaryFg:       "#18181b"
        property color secondary:       "#27272a"
        property color secondaryFg:     "#fafafa"
        property color success:         "#22c55e"
        property color warning:         "#f59e0b"
        property color danger:          "#ef4444"
        property color info:            "#3b82f6"
    }

    readonly property QtObject palette: dark ? darkPalette : lightPalette

    // ---- Backward-compatible aliases ----------------------------------------
    property color bg0:             palette.bg
    property color bg1:             palette.bgSubtle
    property color bg2:             palette.bgMuted
    property color surface:         palette.surface
    property color glass:           dark ? Qt.rgba(1,1,1,0.06) : "#ffffff"
    property color glassStrong:     dark ? Qt.rgba(1,1,1,0.10) : "#eef1f8"
    property color glassBorder:     palette.border
    property color glassBorderSoft: palette.borderSubtle
    property color rowAlt:          dark ? Qt.rgba(1,1,1,0.025) : Qt.rgba(0,0,0,0.025)
    property color rowHover:        dark ? Qt.rgba(1,1,1,0.07) : Qt.rgba(0,0,0,0.04)
    property color gridLine:        palette.borderSubtle
    property color text:            palette.fg
    property color textDim:         palette.fgMuted
    property color textFaint:       palette.fgSubtle
    property color accent:          palette.primary
    property color accent2:         dark ? "#a78bfa" : "#8b5cf6"
    property color accent3:         dark ? "#56ccf2" : "#0ea5e9"
    property color accentInk:       palette.primaryFg
    property color grad0:           "#6366f1"
    property color grad1:           "#8b5cf6"
    property color grad2:           "#38bdf8"
    property color success:         palette.success
    property color warning:         palette.warning
    property color danger:          palette.danger

    // ---- Typography ---------------------------------------------------------
    readonly property string fontFamily:   "Inter"
    readonly property string monoFamily:   "JetBrains Mono"

    readonly property int fsDisplay:  36
    readonly property int fsHero:     30
    readonly property int fsHeading:  24
    readonly property int fsTitle:    20
    readonly property int fsSection:  15
    readonly property int fsBody:     14
    readonly property int fsSmall:    13
    readonly property int fsCaption:  12
    readonly property int fsTiny:     11
    readonly property int fsOverline: 10

    readonly property int wBold:       Font.Bold
    readonly property int wSemibold:   Font.DemiBold
    readonly property int wMedium:     Font.Medium
    readonly property int wNormal:     Font.Normal
    readonly property int wHeavy:      Font.Black

    readonly property real lhTight:   1.15
    readonly property real lhNormal:  1.4
    readonly property real lhLoose:   1.6

    // ---- Spacing (8px grid) -------------------------------------------------
    readonly property int s1:  4
    readonly property int s2:  8
    readonly property int s3:  12
    readonly property int s4:  16
    readonly property int s5:  20
    readonly property int s6:  24
    readonly property int s7:  28
    readonly property int s8:  32
    readonly property int s9:  36
    readonly property int s10: 40
    readonly property int s12: 48
    readonly property int s14: 56
    readonly property int s16: 64

    // ---- Radius -------------------------------------------------------------
    readonly property int rNone: 0
    readonly property int rSm:   4
    readonly property int rMd:   6
    readonly property int rLg:   8
    readonly property int rXl:   12
    readonly property int r2xl:  16
    readonly property int r3xl:  24
    readonly property int rPill: 999

    // ---- Elevation ----------------------------------------------------------
    readonly property int elevationFlat:    0
    readonly property int elevationRaised:  1
    readonly property int elevationOverlay: 2
    readonly property int elevationModal:   3

    // ---- Opacity ------------------------------------------------------------
    readonly property real opDisabled:  0.4
    readonly property real opMuted:     0.6
    readonly property real opHover:     0.85
    readonly property real opActive:    1.0
    readonly property real opSubtle:    0.08
    readonly property real opFaint:     0.04

    // ---- Icon sizes ---------------------------------------------------------
    readonly property int iconSm: 14
    readonly property int iconMd: 16
    readonly property int iconLg: 20
    readonly property int iconXl: 24

    // ---- Z-index ------------------------------------------------------------
    readonly property int zBase:   0
    readonly property int zSticky: 100
    readonly property int zOverlay: 1000
    readonly property int zModal:  5000
    readonly property int zToast:  10000
    readonly property int zDrag:   10001

    // ---- Border widths ------------------------------------------------------
    readonly property int bwDefault: 1
    readonly property int bwFocus:   2

    // ---- Blur ---------------------------------------------------------------
    readonly property int blurSubtle:  4
    readonly property int blurMd:     12
    readonly property int blurStrong: 24

    // ---- Motion -------------------------------------------------------------
    readonly property int _durInstant:     60
    readonly property int _durFast:        150
    readonly property int _durBase:        250
    readonly property int _durSlow:        400
    readonly property int _durExpressive:  600

    readonly property int durInstant:     animationsEnabled ? _durInstant     : 0
    readonly property int durFast:        animationsEnabled ? _durFast        : 0
    readonly property int durBase:        animationsEnabled ? _durBase        : 0
    readonly property int durSlow:        animationsEnabled ? _durSlow        : 0
    readonly property int durExpressive:  animationsEnabled ? _durExpressive  : 0

    readonly property int staggerRow:     animationsEnabled ? 30 : 8
    readonly property int staggerCard:    animationsEnabled ? 50 : 12

    readonly property int easeOutQuad:    Easing.OutQuad
    readonly property int easeOutCubic:   Easing.OutCubic
    readonly property int easeOutQuart:   Easing.OutQuart
    readonly property int easeOutQuint:   Easing.OutQuint
    readonly property int easeOutExpo:    Easing.OutExpo
    readonly property int easeOutBack:    Easing.OutBack
    readonly property int easeOutBounce:  Easing.OutBounce
    readonly property int easeInOutQuad:  Easing.InOutQuad
    readonly property int easeInOutCubic: Easing.InOutCubic
    readonly property int easeOutSine:    Easing.OutSine
    readonly property int easeInSine:     Easing.InSine

    readonly property int easeOut:        easeOutCubic
    readonly property int easeInOut:      easeInOutCubic
    readonly property int easeSpring:     easeOutBack

    // ---- Helpers ------------------------------------------------------------
    function alpha(c, a) {
        return Qt.rgba(c.r, c.g, c.b, a)
    }

    function typeColor(type) {
        switch (type) {
        case "Sale":         return success
        case "Receipt":      return info
        case "Expense":      return danger
        case "Sale Return":  return warning
        case "Purchase":     return accent2
        case "Critical":     return danger
        case "High":         return warning
        case "Normal":       return success
        case "No receipt":   return textDim
        default:             return textDim
        }
    }
}
