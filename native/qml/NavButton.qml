import QtQuick
import MFinlogs

// Sidebar navigation entry with an animated glass highlight and glyph.
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
        color: root.active ? Theme.alpha(Theme.accent, 0.0) : (hover.hovered ? Theme.glass : "transparent")
        Behavior on color { ColorAnimation { duration: Theme.durFast } }

        // Active gradient fill
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: root.active
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.alpha(Theme.grad0, 0.9) }
                GradientStop { position: 1.0; color: Theme.alpha(Theme.grad1, 0.85) }
            }
            opacity: root.active ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.durBase } }
        }
    }

    // Active indicator bar
    Rectangle {
        width: 3
        height: root.active ? 22 : 0
        radius: 2
        color: "#ffffff"
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
            color: root.active ? Theme.accentInk : Theme.textDim
            font.pixelSize: 15
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: !root.collapsed
            text: root.label
            color: root.active ? Theme.accentInk : Theme.textDim
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsBody
            font.weight: root.active ? Font.DemiBold : Font.Medium
            anchors.verticalCenter: parent.verticalCenter
            Behavior on color { ColorAnimation { duration: Theme.durFast } }
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }
}
