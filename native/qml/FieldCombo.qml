import QtQuick
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: root
    property string label: ""
    property var options: []
    property alias currentIndex: combo.currentIndex
    property alias currentText: combo.currentText
    signal nextField()
    signal prevField()

    function focusCombo() { combo.forceActiveFocus() }

    implicitHeight: (label.length > 0 ? 20 : 0) + 44
    implicitWidth: 160

    Column {
        anchors.fill: parent
        spacing: 6

        Text {
            visible: root.label.length > 0
            text: root.label
            color: Theme.palette.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsSmall
            font.weight: Theme.wSemibold
        }

        ComboBox {
            id: combo
            width: parent.width
            height: 44
            model: root.options
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsBody
            activeFocusOnTab: true

            Keys.onDownPressed: function(event) {
                if (!popup.visible) {
                    popup.open()
                } else {
                    combo.currentIndex = Math.min(combo.currentIndex + 1, combo.count - 1)
                }
                event.accepted = true
            }
            Keys.onUpPressed: function(event) {
                if (popup.visible) {
                    combo.currentIndex = Math.max(combo.currentIndex - 1, 0)
                }
                event.accepted = true
            }
            Keys.onReturnPressed: function(event) {
                if (popup.visible) { popup.close() }
                else { root.nextField() }
                event.accepted = true
            }
            Keys.onEnterPressed: function(event) {
                if (popup.visible) { popup.close() }
                else { root.nextField() }
                event.accepted = true
            }
            Keys.onSpacePressed: function(event) {
                if (!popup.visible) popup.open(); else popup.close()
                event.accepted = true
            }

            background: Rectangle {
                radius: Theme.rMd
                color: combo.activeFocus || combo.hovered ? Theme.alpha(Theme.palette.primary, 0.08) : "transparent"
                border.width: combo.activeFocus ? 1.5 : Theme.bwDefault
                border.color: combo.activeFocus ? Theme.palette.primary : Theme.palette.border
                Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
            }

            contentItem: Text {
                leftPadding: 14
                rightPadding: 36
                text: combo.displayText
                font: combo.font
                color: Theme.palette.fg
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            indicator: Canvas {
                x: combo.width - 26
                y: combo.height / 2 - 3
                width: 12
                height: 7
                rotation: combo.popup.visible ? 180 : 0
                Behavior on rotation { NumberAnimation { duration: Theme.durFast } }
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = Theme.palette.fgMuted
                    ctx.lineWidth = 2
                    ctx.beginPath()
                    ctx.moveTo(0, 0); ctx.lineTo(6, 6); ctx.lineTo(12, 0)
                    ctx.stroke()
                }
            }

            popup: Popup {
                y: combo.height + 6
                width: combo.width
                padding: 6
                background: Rectangle {
                    radius: Theme.rMd
                    color: Theme.palette.bg
                    border.width: 1
                    border.color: Theme.palette.border
                }
                enter: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durFast }
                    NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.durBase; easing.type: Theme.easeOut }
                }
                contentItem: ListView {
                    implicitHeight: Math.min(contentHeight, 260)
                    model: combo.popup.visible ? combo.delegateModel : null
                    clip: true
                    currentIndex: combo.highlightedIndex
                    ScrollIndicator.vertical: ScrollIndicator {}
                }
            }

            delegate: ItemDelegate {
                width: combo.width - 12
                height: 34
                contentItem: Text {
                    text: modelData
                    color: Theme.palette.fg
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsSmall
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }
                background: Rectangle {
                    radius: Theme.rSm
                    color: highlighted ? Theme.alpha(Theme.palette.fg, 0.08) : "transparent"
                }
                highlighted: combo.highlightedIndex === index
            }
        }
    }
}
