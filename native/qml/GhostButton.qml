import QtQuick
import QtQuick.Controls.Basic
import MFinlogs

// Subtle, outlined glass button for secondary actions.
Button {
    id: control
    implicitHeight: 40
    implicitWidth: Math.max(110, contentItem.implicitWidth + 40)
    padding: 0
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsBody
    font.weight: Font.DemiBold

    property color tint: Theme.accent

    scale: control.pressed ? 0.97 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.durFast; easing.type: Theme.easeOut } }

    background: Rectangle {
        radius: Theme.rMd
        color: control.hovered ? Theme.alpha(control.tint, 0.16) : Theme.glass
        border.width: 1
        border.color: control.hovered ? Theme.alpha(control.tint, 0.6) : Theme.glassBorder
        opacity: control.enabled ? 1.0 : 0.4
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.hovered ? Theme.text : Theme.textDim
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        leftPadding: 20
        rightPadding: 20
        elide: Text.ElideRight
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }
}
