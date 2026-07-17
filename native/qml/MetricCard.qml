import QtQuick
import MFinlogs

GlassPanel {
    id: root
    property string label: ""
    property real value: 0
    property string prefix: "\u20b9 "
    property color accent: Theme.palette.primary
    property string glyph: "\u25C9"
    property string deltaText: ""
    property bool deltaUp: true

    implicitHeight: 116
    radius: Theme.rLg

    Accessible.role: Accessible.StaticText
    Accessible.name: root.label + ": " + root.prefix + backend.formatMoney(root.value)

    property real shown: 0
    Behavior on shown { NumberAnimation { duration: Theme.durSlow; easing.type: Theme.easeOut } }
    onValueChanged: shown = value
    Component.onCompleted: shown = value

    property bool hovered: hoverArea.hovered
    y: 0
    transform: Translate { y: root.hovered ? -4 : 0; Behavior on y { NumberAnimation { duration: Theme.durFast; easing.type: Theme.easeOut } } }
    HoverHandler { id: hoverArea }

    Rectangle {
        width: 3
        height: parent.height - Theme.s5
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.verticalCenter: parent.verticalCenter
        radius: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.accent }
            GradientStop { position: 1.0; color: Theme.alpha(root.accent, 0.2) }
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: Theme.s5
        spacing: Theme.s2

        Row {
            width: parent.width
            spacing: Theme.s2
            Text {
                text: root.glyph
                color: root.accent
                font.pixelSize: 16
            }
            Text {
                text: root.label
                color: Theme.palette.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsTiny
                font.weight: Theme.wSemibold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.5
            }
        }

        Text {
            text: root.prefix + backend.formatMoney(root.shown)
            color: Theme.palette.fg
            font.family: Theme.fontFamily
            font.pixelSize: 26
            font.weight: Font.Bold
        }

        Row {
            spacing: 4
            visible: root.deltaText.length > 0
            Text {
                text: root.deltaUp ? "\u25B2" : "\u25BC"
                color: root.deltaUp ? Theme.palette.success : Theme.palette.danger
                font.pixelSize: 9
            }
            Text {
                text: root.deltaText
                color: root.deltaUp ? Theme.palette.success : Theme.palette.danger
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsTiny
            }
        }
    }
}
