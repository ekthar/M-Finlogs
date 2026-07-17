import QtQuick
import MFinlogs

Item {
    id: root

    property int radius: Theme.rLg
    property color fillColor: Theme.palette.surface
    property color borderColor: Theme.palette.border
    property bool elevated: true

    implicitWidth: 200
    implicitHeight: 120

    // ── Shadow layers (siblings declared first → paint behind) ──
    Rectangle {
        x: -4; y: 10
        width: parent.width + 8
        height: parent.height + 12
        radius: root.radius + 4
        visible: root.elevated
        color: Theme.dark ? Qt.rgba(0,0,0,0.18) : Qt.rgba(0,0,0,0.05)
    }
    Rectangle {
        x: -2; y: 5
        width: parent.width + 4
        height: parent.height + 8
        radius: root.radius + 2
        visible: root.elevated
        color: Theme.dark ? Qt.rgba(0,0,0,0.12) : Qt.rgba(0,0,0,0.03)
    }
    Rectangle {
        x: -1; y: 2
        width: parent.width + 2
        height: parent.height + 4
        radius: root.radius + 1
        visible: root.elevated
        color: Theme.dark ? Qt.rgba(0,0,0,0.06) : Qt.rgba(0,0,0,0.015)
    }

    // ── Card surface (declared last → paints on top) ──
    Rectangle {
        id: surface
        anchors.fill: parent
        radius: root.radius
        color: root.fillColor
        border.width: 1
        border.color: root.borderColor

        // Top sheen
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: Theme.dark
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1,1,1,0.10) }
                GradientStop { position: 0.18; color: Qt.rgba(1,1,1,0.02) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }
}
