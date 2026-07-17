import QtQuick
import MFinlogs

Item {
    id: root

    property bool checked: false
    property string label: ""
    signal toggled(bool checked)

    implicitWidth: Math.max(20, labelRow.implicitWidth)
    implicitHeight: 20

    Row {
        id: labelRow
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.s2

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 20
            height: 20
            radius: Theme.rSm
            color: root.checked ? Theme.palette.primary : "transparent"
            border.width: 2
            border.color: root.checked ? Theme.palette.primary : Theme.palette.fgSubtle
            Behavior on color { ColorAnimation { duration: Theme.durFast } }

            Text {
                anchors.centerIn: parent
                text: "\u2713"
                color: Theme.palette.primaryFg
                font.pixelSize: 12
                font.weight: Font.Bold
                visible: root.checked
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.label.length > 0
            text: root.label
            color: Theme.palette.fg
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsSmall
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

    Accessible.role: Accessible.CheckBox
    Accessible.name: root.label
    Accessible.checked: root.checked
}
