import QtQuick
import QtQuick.Controls.Basic
import MFinlogs

// Labeled, glassy text field with an animated focus glow and optional
// numeric / completion behavior.
Item {
    id: root
    property alias text: field.text
    property alias placeholder: field.placeholderText
    property alias echoMode: field.echoMode
    property alias inputField: field
    property string label: ""
    property bool numeric: false
    property var completions: []
    property bool showCompletions: true
    signal accepted()

    implicitHeight: (label.length > 0 ? 20 : 0) + 44
    implicitWidth: 200

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

        Rectangle {
            id: box
            width: parent.width
            height: 44
            radius: Theme.rMd
            color: field.activeFocus ? Theme.alpha(Theme.accent, 0.10) : Theme.glass
            border.width: field.activeFocus ? 1.5 : 1
            border.color: field.activeFocus ? Theme.accent : Theme.glassBorder
            Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
            Behavior on color { ColorAnimation { duration: Theme.durFast } }

            TextField {
                id: field
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                verticalAlignment: TextInput.AlignVCenter
                color: Theme.text
                placeholderTextColor: Theme.textFaint
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsBody
                selectionColor: Theme.alpha(Theme.accent, 0.45)
                selectedTextColor: Theme.text
                background: Item {}
                inputMethodHints: root.numeric ? Qt.ImhFormattedNumbersOnly : Qt.ImhNone
                onAccepted: root.accepted()

                onTextEdited: {
                    if (root.showCompletions && root.completions.length > 0 && text.length > 0) {
                        var matches = []
                        var q = text.toLowerCase()
                        for (var i = 0; i < root.completions.length; i++) {
                            var c = String(root.completions[i])
                            if (c.toLowerCase().indexOf(q) >= 0) matches.push(c)
                            if (matches.length >= 6) break
                        }
                        suggestModel.clear()
                        for (var j = 0; j < matches.length; j++) suggestModel.append({ name: matches[j] })
                        suggest.visible = matches.length > 0 && !(matches.length === 1 && matches[0] === text)
                    } else {
                        suggest.visible = false
                    }
                }
            }
        }
    }

    // Completion popup
    Popup {
        id: suggest
        y: root.height + 4
        width: root.width
        padding: 6
        visible: false
        background: Rectangle {
            radius: Theme.rMd
            color: Theme.bg2
            border.width: 1
            border.color: Theme.glassBorder
        }
        contentItem: ListView {
            id: suggestList
            implicitHeight: Math.min(suggestModel.count * 34, 204)
            model: ListModel { id: suggestModel }
            clip: true
            delegate: Rectangle {
                width: suggestList.width
                height: 32
                radius: Theme.rSm
                color: hover.hovered ? Theme.glassStrong : "transparent"
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    text: name
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsSmall
                }
                HoverHandler { id: hover }
                TapHandler {
                    onTapped: {
                        field.text = name
                        suggest.visible = false
                        root.accepted()
                    }
                }
            }
        }
    }
}
