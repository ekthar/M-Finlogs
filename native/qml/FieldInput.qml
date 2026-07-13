import QtQuick
import QtQuick.Controls.Basic
import MFinlogs

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

    function rebuildSuggestions(txt, forceOpen) {
        if (!showCompletions || completions.length === 0) {
            suggest.visible = false
            return
        }
        var matches = []
        var q = String(txt).toLowerCase()
        for (var i = 0; i < completions.length; i++) {
            var c = String(completions[i])
            if (q.length === 0 || c.toLowerCase().indexOf(q) >= 0) matches.push(c)
            if (matches.length >= 8) break
        }
        suggestModel.clear()
        for (var j = 0; j < matches.length; j++) suggestModel.append({ name: matches[j] })
        suggestList.currentIndex = matches.length > 0 ? 0 : -1
        var showIt = matches.length > 0 && !(matches.length === 1 && matches[0] === txt)
        suggest.visible = showIt || (forceOpen && matches.length > 0)
    }
    function acceptSuggestion(idx) {
        if (idx >= 0 && idx < suggestModel.count) {
            field.text = suggestModel.get(idx).name
        }
        suggest.visible = false
        root.accepted()
    }

    implicitHeight: (label.length > 0 ? 20 : 0) + 44
    implicitWidth: 200

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

        Rectangle {
            id: box
            width: parent.width
            height: 44
            radius: Theme.rMd
            color: field.activeFocus ? Theme.alpha(Theme.palette.primary, 0.10) : "transparent"
            border.width: field.activeFocus ? 1.5 : Theme.bwDefault
            border.color: field.activeFocus ? Theme.palette.primary : Theme.palette.border
            Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
            Behavior on color { ColorAnimation { duration: Theme.durFast } }

            TextField {
                id: field
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                verticalAlignment: TextInput.AlignVCenter
                color: Theme.palette.fg
                placeholderTextColor: Theme.palette.fgSubtle
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsBody
                selectionColor: Theme.alpha(Theme.palette.primary, 0.45)
                selectedTextColor: Theme.palette.fg
                background: Item {}
                activeFocusOnTab: true
                inputMethodHints: root.numeric ? Qt.ImhFormattedNumbersOnly : Qt.ImhNone

                onAccepted: {
                    if (suggest.visible && suggestList.currentIndex >= 0) {
                        root.acceptSuggestion(suggestList.currentIndex)
                    } else {
                        suggest.visible = false
                        root.accepted()
                    }
                }

                Keys.onDownPressed: function(event) {
                    if (suggest.visible && suggestModel.count > 0) {
                        suggestList.currentIndex = Math.min(suggestList.currentIndex + 1, suggestModel.count - 1)
                        event.accepted = true
                    } else if (root.showCompletions && root.completions.length > 0) {
                        root.rebuildSuggestions(field.text, true)
                        event.accepted = true
                    } else {
                        event.accepted = false
                    }
                }
                Keys.onUpPressed: function(event) {
                    if (suggest.visible && suggestModel.count > 0) {
                        suggestList.currentIndex = Math.max(suggestList.currentIndex - 1, 0)
                        event.accepted = true
                    } else {
                        event.accepted = false
                    }
                }
                Keys.onEscapePressed: suggest.visible = false

                onTextEdited: root.rebuildSuggestions(text, false)
            }
        }
    }

    Popup {
        id: suggest
        y: root.height + 4
        width: root.width
        padding: 6
        visible: false
        focus: false
        closePolicy: Popup.CloseOnPressOutside
        background: Rectangle {
            radius: Theme.rMd
            color: Theme.palette.bg
            border.width: 1
            border.color: Theme.palette.border
        }
        contentItem: ListView {
            id: suggestList
            implicitHeight: Math.min(suggestModel.count * 34, 272)
            model: ListModel { id: suggestModel }
            clip: true
            currentIndex: -1
            delegate: Rectangle {
                width: suggestList.width
                height: 32
                radius: Theme.rSm
                color: (hover.hovered || suggestList.currentIndex === index)
                       ? Theme.alpha(Theme.palette.fg, 0.08) : "transparent"
                border.width: suggestList.currentIndex === index ? 1 : 0
                border.color: Theme.alpha(Theme.palette.primary, 0.5)
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    text: name
                    color: suggestList.currentIndex === index ? Theme.palette.fg : Theme.palette.fgMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsSmall
                    font.weight: suggestList.currentIndex === index ? Theme.wSemibold : Font.Normal
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
