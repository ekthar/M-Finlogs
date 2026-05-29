import QtQuick
import MFinlogs

// Global toast stack, anchored top-right. Call show(message, kind).
Item {
    id: root
    anchors.fill: parent
    z: 9999

    function show(message, kind) {
        toastModel.insert(0, { message: message, kind: kind || "info" })
    }

    Column {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Theme.s6
        anchors.rightMargin: Theme.s6
        spacing: Theme.s2

        Repeater {
            model: ListModel { id: toastModel }
            delegate: Item {
                id: toast
                width: toastRect.width
                height: toastRect.height
                property color tint: kind === "success" ? Theme.success
                                    : kind === "error" ? Theme.danger
                                    : Theme.accent

                GlassPanel {
                    id: toastRect
                    width: Math.min(360, Math.max(220, label.implicitWidth + 64))
                    height: 52
                    fillColor: Theme.bg2
                    radius: Theme.rMd

                    Rectangle {
                        width: 4
                        height: parent.height - 16
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 2
                        color: toast.tint
                    }
                    Text {
                        id: glyph
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 22
                        text: kind === "success" ? "\u2713" : kind === "error" ? "\u2715" : "\u2139"
                        color: toast.tint
                        font.pixelSize: 16
                        font.weight: Font.Bold
                    }
                    Text {
                        id: label
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: glyph.right
                        anchors.leftMargin: 12
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        text: message
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSmall
                        wrapMode: Text.WordWrap
                    }
                }

                // Entrance
                opacity: 0
                x: 40
                Component.onCompleted: {
                    opacity = 1
                    x = 0
                }
                Behavior on opacity { NumberAnimation { duration: Theme.durBase } }
                Behavior on x { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeOut } }

                Timer {
                    interval: 3200
                    running: true
                    onTriggered: removeAnim.start()
                }
                SequentialAnimation {
                    id: removeAnim
                    ParallelAnimation {
                        NumberAnimation { target: toast; property: "opacity"; to: 0; duration: Theme.durBase }
                        NumberAnimation { target: toast; property: "x"; to: 40; duration: Theme.durBase }
                    }
                    ScriptAction { script: toastModel.remove(index) }
                }
            }
        }
    }
}
