import QtQuick
import QtQuick.Controls.Basic
import MFinlogs

Button {
    id: control

    property string variant: "primary"
    property color from: Theme.palette.primary
    property color to: Theme.alpha(Theme.palette.primary, 0.8)

    implicitHeight: 40
    implicitWidth: variant === "icon" ? 40 : Math.max(120, contentItem.implicitWidth + 44)
    padding: 0
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsBody
    font.weight: Theme.wSemibold

    scale: control.pressed ? 0.97 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.durFast; easing.type: Theme.easeOut } }

    background: Rectangle {
        radius: Theme.rMd
        clip: true

        color: {
            if (!control.enabled) return "transparent"
            switch (control.variant) {
            case "primary":
            case "danger":
                return "transparent"
            case "ghost":
                return control.hovered ? Theme.alpha(Theme.palette.primary, 0.16) : "transparent"
            case "outline":
                return control.hovered ? Theme.alpha(Theme.palette.primary, 0.08) : "transparent"
            default:
                return "transparent"
            }
        }
        border.width: control.variant === "outline" ? Theme.bwDefault : 0
        border.color: control.variant === "outline"
            ? (control.hovered ? Theme.palette.primary : Theme.palette.border)
            : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }

        opacity: control.enabled ? (control.hovered ? 1.0 : 0.92) : Theme.opDisabled

        gradient: Gradient {
            orientation: Gradient.Horizontal
            // Gradient has no visible property. Use transparent stops for
            // ghost/outline variants while keeping the background available
            // for their hover and border states.
            GradientStop {
                position: 0.0
                color: control.variant === "danger"
                    ? Theme.danger
                    : (control.variant === "primary" ? control.from : "transparent")
            }
            GradientStop {
                position: 1.0
                color: control.variant === "danger"
                    ? Theme.alpha(Theme.danger, 0.85)
                    : (control.variant === "primary" ? control.to : "transparent")
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: control.hovered && (control.variant === "primary" || control.variant === "danger") ? 1 : 0
            border.color: Qt.rgba(1, 1, 1, 0.35)
            Behavior on border.width { NumberAnimation { duration: Theme.durFast } }
        }

        Rectangle {
            id: ripple
            anchors.centerIn: parent
            radius: 0
            width: radius * 2
            height: radius * 2
            color: Qt.rgba(1, 1, 1, 0.30)
            opacity: 0
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.variant === "primary" || control.variant === "danger"
            ? Theme.palette.primaryFg
            : (control.hovered ? Theme.palette.fg : Theme.palette.fgMuted)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.variant === "icon" ? 0 : 22
        rightPadding: control.variant === "icon" ? 0 : 22
        elide: Text.ElideRight
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }

    Accessible.role: Accessible.Button
    Accessible.name: control.text
    Accessible.onPressAction: control.clicked()
    Accessible.onFocusAction: control.forceActiveFocus()

    onPressed: rippleGrow.restart()

    ParallelAnimation {
        id: rippleGrow
        NumberAnimation { target: ripple; property: "radius"; from: 0; to: control.width; duration: Theme.durSlow; easing.type: Theme.easeOut }
        NumberAnimation { target: ripple; property: "opacity"; from: 0.4; to: 0; duration: Theme.durSlow; easing.type: Theme.easeOut }
    }
}
