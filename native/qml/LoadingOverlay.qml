import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Rectangle {
    id: root

    property bool active: false
    property string text: "Loading..."

    visible: active
    color: Theme.alpha(Theme.palette.bg, 0.7)
    z: Theme.zOverlay

    Accessible.role: Accessible.Indicator
    Accessible.name: root.text

    Column {
        anchors.centerIn: parent
        spacing: Theme.s3

        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: root.active
            width: 36
            height: 36
            palette.dark: Theme.palette.primary
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.text
            color: Theme.palette.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsSmall
            font.weight: Font.Medium
            visible: root.text.length > 0
        }
    }

    Behavior on opacity {
        NumberAnimation { duration: Theme.durFast; easing.type: Theme.easeOut }
    }

    // Prevent interaction with elements behind the overlay
    MouseArea {
        anchors.fill: parent
        enabled: root.active
        hoverEnabled: true
    }
}
