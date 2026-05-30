import QtQuick
import QtQuick.Effects
import MFinlogs

// A frosted-glass surface: translucent fill, hairline border, soft drop
// shadow and an optional top sheen.
//
// The chrome (shadow + surface) is drawn behind via negative z, so any child
// declared by callers becomes a normal child of this Item and renders on top —
// avoiding the default-property-alias redirection pitfall.
Item {
    id: root

    property int radius: Theme.rLg
    property color fillColor: Theme.glass
    property color borderColor: Theme.glassBorder
    property bool elevated: true
    property bool sheen: true

    implicitWidth: 200
    implicitHeight: 120

    // Drop shadow (rendered behind)
    Rectangle {
        id: shadowSource
        anchors.fill: parent
        radius: root.radius
        color: "#000000"
        visible: false
    }
    MultiEffect {
        source: shadowSource
        anchors.fill: shadowSource
        visible: root.elevated
        z: -2
        shadowEnabled: true
        shadowColor: Qt.rgba(0, 0, 0, 0.45)
        shadowBlur: 1.0
        shadowVerticalOffset: 18
        shadowHorizontalOffset: 0
        autoPaddingEnabled: true
        opacity: 0.9
    }

    // Surface
    Rectangle {
        id: surface
        anchors.fill: parent
        radius: root.radius
        color: root.fillColor
        border.width: 1
        border.color: root.borderColor
        z: -1

        // Top sheen
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: root.sheen
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.10) }
                GradientStop { position: 0.18; color: Qt.rgba(1, 1, 1, 0.02) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }
}
