import QtQuick
import QtQuick.Controls.Basic
import MFinlogs

Button {
    id: control

    property string glyph: ""
    property color tint: Theme.palette.fgMuted

    implicitWidth: 40
    implicitHeight: 40
    padding: 0

    scale: control.pressed ? 0.92 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.durFast; easing.type: Theme.easeOut } }

    background: Rectangle {
        radius: Theme.rMd
        color: control.hovered ? Theme.alpha(control.tint, 0.12) : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }

    contentItem: Text {
        text: control.glyph
        color: control.hovered ? control.tint : Theme.palette.fgMuted
        font.pixelSize: Theme.iconMd
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }

    Accessible.name: control.glyph + " button"
    Accessible.role: Accessible.Button
    Accessible.onPressAction: control.clicked()
    Accessible.onFocusAction: control.forceActiveFocus()
}
