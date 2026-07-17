import QtQuick
import MFinlogs

Item {
    id: root

    property bool checked: false
    property string label: ""
    property string description: ""
    signal toggled(bool checked)

    implicitWidth: label.length > 0 ? 240 : 52
    implicitHeight: 30

    Row {
        anchors.fill: parent
        spacing: Theme.s3
        anchors.verticalCenter: parent.verticalCenter

        Column {
            visible: root.label.length > 0
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 60
            spacing: 2
            Text {
                width: parent.width
                text: root.label
                color: Theme.palette.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsBody
                font.weight: Theme.wSemibold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                visible: root.description.length > 0
                text: root.description
                color: Theme.palette.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsTiny
                elide: Text.ElideRight
            }
        }

        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 52
            height: 30
            radius: 15
            color: root.checked ? Theme.palette.primary : Theme.alpha(Theme.palette.fg, 0.04)
            border.width: 1
            border.color: root.checked ? Theme.palette.primary : Theme.palette.border
            Behavior on color { ColorAnimation { duration: Theme.durFast } }

            Rectangle {
                width: 24
                height: 24
                radius: 12
                color: "#ffffff"
                y: 3
                x: root.checked ? parent.width - width - 3 : 3
                Behavior on x { NumberAnimation { duration: Theme.durFast; easing.type: Theme.easeOut } }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.checked = !root.checked
                    root.toggled(root.checked)
                }
            }
        }
    }

    Accessible.role: Accessible.Switch
    Accessible.name: root.label
    Accessible.checked: root.checked
}
