import QtQuick
import MFinlogs

// Small rounded status chip with a colored dot.
Rectangle {
    id: root
    property string text: ""
    property color tint: Theme.success

    implicitHeight: 24
    implicitWidth: row.implicitWidth + 22
    radius: Theme.rPill
    color: Theme.alpha(tint, 0.16)
    border.width: 1
    border.color: Theme.alpha(tint, 0.4)

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6
        Rectangle {
            width: 7; height: 7; radius: 3.5
            color: root.tint
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.text
            color: root.tint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsTiny
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
