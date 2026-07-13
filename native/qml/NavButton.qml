import QtQuick
import MFinlogs

Item {
    id: root
    property string label: ""
    property string glyph: "\u25C9"
    property bool active: false
    property bool collapsed: false
    signal clicked()

    implicitHeight: 44
    implicitWidth: 200

    Rectangle {
        id: bg
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        radius: Theme.rMd
        color: root.active ? Theme.alpha(Theme.palette.primary, 0.15) : (hover.hovered ? Theme.alpha(Theme.palette.fg, 0.06) : "transparent")
        Behavior on color { ColorAnimation { duration: Theme.durFast } }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: root.active
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.alpha(Theme.palette.primary, 0.20) }
                GradientStop { position: 1.0; color: Theme.alpha(Theme.palette.primary, 0.08) }
            }
            opacity: root.active ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.durBase } }
        }
    }

    Rectangle {
        width: 3
        height: root.active ? 22 : 0
        radius: 2
        color: Theme.palette.primary
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 4
        Behavior on height { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeSpring } }
    }

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 24
        spacing: 12

        Text {
            text: root.glyph
            color: root.active ? Theme.palette.primaryFg : Theme.palette.fgMuted
            font.pixelSize: 15
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: !root.collapsed
            text: root.label
            color: root.active ? Theme.palette.primaryFg : Theme.palette.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsBody
            font.weight: root.active ? Theme.wSemibold : Theme.wMedium
            anchors.verticalCenter: parent.verticalCenter
            Behavior on color { ColorAnimation { duration: Theme.durFast } }
        }
    }

    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()
    Keys.onEnterPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()

    Accessible.role: Accessible.Button
    Accessible.name: root.label
    Accessible.onPressAction: root.clicked()
    Accessible.onFocusAction: root.forceActiveFocus()

    FocusRing {
        visible: root.activeFocus
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }
}
