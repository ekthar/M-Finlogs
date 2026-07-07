import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: root
    anchors.fill: parent
    z: 10001

    property bool open: false

    property var searchResults: []
    property string searchQuery: ""

    function toggle() {
        open = !open
        console.log("[SEARCH] toggle() open:", open)
        if (open) {
            queryField.forceActiveFocus()
        }
    }

    function doSearch(query) {
        searchQuery = query.trim().toLowerCase()
        console.log("[SEARCH] doSearch query:", searchQuery, "length:", searchQuery.length)
        if (searchQuery.length < 2) {
            searchResults = []
            return
        }
        var results = []
        var parties = backend.partyNames()
        console.log("[SEARCH] partyNames returned:", parties ? parties.length : "null")
        for (var i = 0; i < parties.length; i++) {
            if (parties[i].toLowerCase().indexOf(searchQuery) >= 0) {
                results.push({ kind: "party", label: parties[i], detail: "Party" })
            }
        }
        var txns = backend.transactions(1, 200, 365)
        console.log("[SEARCH] transactions returned:", txns ? txns.length : "null")
        for (var j = 0; j < txns.length; j++) {
            var t = txns[j]
            if (t.party && t.party.toLowerCase().indexOf(searchQuery) >= 0 ||
                t.bill_no && t.bill_no.toLowerCase().indexOf(searchQuery) >= 0) {
                if (results.length < 20) {
                    results.push({ kind: "txn", label: t.party + " (" + t.bill_no + ")", detail: t.type + " \u20B9" + backend.formatMoney(t.amount) })
                }
            }
        }
        searchResults = results
        console.log("[SEARCH] results:", results.length)
    }

    opacity: open ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeOut } }

    Rectangle {
        anchors.fill: parent
        color: Theme.alpha(Theme.bg0, 0.55)
        TapHandler { onTapped: root.open = false }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 80
        width: Math.min(520, root.width * 0.8)
        radius: Theme.rLg
        color: Theme.bg2
        border.width: 1
        border.color: Theme.glassBorder

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.s4
                Layout.rightMargin: Theme.s4
                Layout.topMargin: Theme.s3
                Layout.bottomMargin: Theme.s3
                spacing: Theme.s3

                Text {
                    text: "\u2315"
                    color: Theme.textDim
                    font.pixelSize: 18
                }

                TextField {
                    id: queryField
                    Layout.fillWidth: true
                    implicitHeight: 34
                    placeholderText: "Search parties, bills..."
                    color: Theme.text
                    placeholderTextColor: Theme.textFaint
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsBody
                    background: Rectangle {
                        color: "transparent"
                    }
                    onTextChanged: root.doSearch(text)
                }

                Rectangle {
                    width: 1
                    height: 24
                    color: Theme.glassBorder
                    anchors.verticalCenter: parent.verticalCenter
                    visible: queryField.text.length > 0
                }

                Text {
                    text: "\u2715"
                    color: Theme.textFaint
                    font.pixelSize: 14
                    visible: queryField.text.length > 0
                    TapHandler { onTapped: queryField.text = "" }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.glassBorder
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(320, 52 * Math.max(1, searchResults.length + 1) + 16)
                clip: true

                ListView {
                    id: resultsList
                    anchors.fill: parent
                    anchors.margins: Theme.s2
                    model: searchResults.length > 0 ? searchResults : [{ kind: "empty", label: "", detail: "" }]
                    spacing: 2

                    delegate: Rectangle {
                        width: resultsList.width
                        height: searchResults.length > 0 ? 44 : 60
                        radius: Theme.rSm
                        color: resultsHover.containsMouse ? Theme.rowHover : "transparent"
                        visible: searchResults.length > 0 || modelData.kind !== "empty"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.s4
                            anchors.rightMargin: Theme.s4
                            spacing: Theme.s3
                            visible: searchResults.length > 0

                            Text {
                                text: modelData.kind === "party" ? "\u263A" : "\u270E"
                                color: modelData.kind === "party" ? Theme.accent3 : Theme.accent
                                font.pixelSize: 16
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fsSmall
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.detail
                                    color: Theme.textDim
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fsTiny
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "Type at least 2 characters to search"
                            color: Theme.textFaint
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsSmall
                            visible: searchResults.length === 0
                        }

                        HoverHandler { id: resultsHover }
                    }
                }
            }
        }
    }

    Keys.onEscapePressed: root.open = false
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_K && (event.modifiers & Qt.ControlModifier)) {
            root.open = false
            event.accepted = true
        }
    }
}
