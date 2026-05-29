import QtQuick
import QtQuick.Controls.Basic
import MFinlogs

// Premium data table.
//   columns: [{ title, key, align?("left"|"right"|"center"), weight?, money?, chip? }]
//   rows: array of objects (QVariantMap from backend)
// Emits rowActivated(row) on double-tap.
Item {
    id: root
    property var columns: []
    property var rows: []
    property string emptyText: "No records yet"
    property bool loading: false
    signal rowActivated(var row)

    function alignFor(c) {
        return c.align === "right" ? Text.AlignRight
             : c.align === "center" ? Text.AlignHCenter
             : Text.AlignLeft
    }
    function cellText(c, row) {
        var v = row[c.key]
        if (v === undefined || v === null) return ""
        if (c.money) return backend.formatMoney(Number(v))
        if (c.date) return backend.formatDate(String(v))
        return String(v)
    }

    Column {
        anchors.fill: parent
        spacing: 0

        // Header
        Rectangle {
            width: parent.width
            height: 44
            radius: Theme.rMd
            color: Theme.glassStrong
            Row {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                Repeater {
                    model: root.columns
                    delegate: Item {
                        width: (root.width - 32) * (modelData.weight ? modelData.weight : 1) / root.totalWeight
                        height: parent.height
                        Text {
                            anchors.fill: parent
                            anchors.rightMargin: 8
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: root.alignFor(modelData)
                            text: modelData.title
                            color: Theme.textDim
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsTiny
                            font.weight: Font.Bold
                            font.capitalization: Font.AllUppercase
                            font.letterSpacing: 0.5
                        }
                    }
                }
            }
        }

        // Body
        ListView {
            id: list
            width: parent.width
            height: root.height - 44
            clip: true
            model: root.rows
            boundsBehavior: Flickable.StopAtBounds
            ScrollIndicator.vertical: ScrollIndicator {}

            delegate: Rectangle {
                id: rowItem
                width: list.width
                height: 46
                property var rowData: modelData
                color: rowHover.hovered ? Theme.glassStrong
                     : (index % 2 === 0 ? "transparent" : Theme.alpha(Qt.rgba(1,1,1,1), 0.02))
                Behavior on color { ColorAnimation { duration: Theme.durFast } }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.glassBorderSoft
                }

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    Repeater {
                        model: root.columns
                        delegate: Item {
                            id: cell
                            property var col: modelData
                            property string value: root.cellText(modelData, rowItem.rowData)
                            width: (list.width - 32) * (col.weight ? col.weight : 1) / root.totalWeight
                            height: parent.height

                            // Chip rendering for type/status columns
                            Rectangle {
                                visible: cell.col.chip === true && cell.value.length > 0
                                anchors.verticalCenter: parent.verticalCenter
                                height: 22
                                width: chipText.implicitWidth + 20
                                radius: Theme.rPill
                                color: Theme.alpha(Theme.typeColor(cell.value), 0.16)
                                Text {
                                    id: chipText
                                    anchors.centerIn: parent
                                    text: cell.value
                                    color: Theme.typeColor(cell.value)
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fsTiny
                                    font.weight: Font.DemiBold
                                }
                            }

                            Text {
                                anchors.fill: parent
                                anchors.rightMargin: 8
                                visible: cell.col.chip !== true
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: root.alignFor(cell.col)
                                text: cell.value
                                color: cell.col.money ? Theme.text : Theme.textDim
                                font.family: cell.col.money ? Theme.monoFamily : Theme.fontFamily
                                font.pixelSize: Theme.fsSmall
                                font.weight: cell.col.money ? Font.DemiBold : Font.Normal
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                HoverHandler { id: rowHover }
                TapHandler { onDoubleTapped: root.rowActivated(rowItem.rowData) }

                // Staggered entrance
                opacity: 0
                Component.onCompleted: appear.start()
                NumberAnimation {
                    id: appear
                    target: rowItem; property: "opacity"; from: 0; to: 1
                    duration: Theme.durBase
                    easing.type: Theme.easeOut
                }
            }

            // Empty state
            Column {
                anchors.centerIn: parent
                visible: (!root.rows || root.rows.length === 0) && !root.loading
                spacing: 10
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "\u25CB"
                    color: Theme.textFaint
                    font.pixelSize: 34
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.emptyText
                    color: Theme.textFaint
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsSmall
                }
            }
        }
    }

    property real totalWeight: {
        var t = 0
        for (var i = 0; i < columns.length; i++) t += (columns[i].weight ? columns[i].weight : 1)
        return t > 0 ? t : 1
    }
}
