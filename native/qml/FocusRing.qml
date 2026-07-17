import QtQuick
import MFinlogs

Rectangle {
    anchors.fill: parent
    radius: parent.radius + 2
    color: "transparent"
    border.width: Theme.bwFocus
    border.color: Theme.palette.primary
    opacity: parent.activeFocus ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Theme.durFast } }
}
