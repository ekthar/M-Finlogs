import QtQuick
import QtQuick.Controls.Basic
import MFinlogs

// Labeled, glassy dropdown with an animated popup and custom-drawn items.
Item {
    id: root
    property string label: ""
    property var options: []
    property alias currentIndex: combo.currentIndex
    property alias currentText: combo.currentText

    implicitHeight: (label.length > 0 ? 20 : 0) + 44
    implicitWidth: 160

    Column {
        anchors.fill: parent
        spacing: 6

        Text {
            visible: root.label.length > 0
            text: root.label
            color: Theme.textDim
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsSmall
            font.weight: Font.DemiBold
        }

        ComboBox {
            id: combo
            width: parent.width
            height: 44
            model: root.options
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsBody
            activeFocusOnTab: true

            // Keyboard: Enter/Return opens dropdown, arrow keys navigate items
            Keys.onReturnPressed: { if (!popup.visible) popup.open(); else popup.close() }
            Keys.onEnterPressed: { if (!popup.visible) popup.open(); else popup.close() }
            // Space also toggles (built-in behavior), just ensure it works
            Keys.onSpacePressed: { if (!popup.visible) popup.open(); else popup.close() }

            background: Rectangle {
                radius: Theme.rMd
                color: combo.activeFocus || combo.hovered ? Theme.alpha(Theme.accent, 0.08) : Theme.glass
                border.width: combo.activeFocus ? 1.5 : 1
                border.color: combo.activeFocus ? Theme.accent : Theme.glassBorder
                Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
            }

            contentItem: Text {
                leftPadding: 14
                rightPadding: 36
                text: combo.displayText
                font: combo.font
                color: Theme.text
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
                    ctx.strokeStyle = Theme.textDim
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
                    color: Theme.bg2
                    border.width: 1
                    border.color: Theme.glassBorder
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
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsSmall
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }
                background: Rectangle {
                    radius: Theme.rSm
                    color: highlighted ? Theme.glassStrong : "transparent"
                }
                highlighted: combo.highlightedIndex === index
            }
        }
    }
}
