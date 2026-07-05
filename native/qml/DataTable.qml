import QtQuick
import QtQuick.Controls.Basic
import MFinlogs

// Premium data table with precise column alignment and an optional grid.
//   columns: [{ title, key, align?("left"|"right"|"center"),
//               weight?, money?, date?, chip? }]
//   rows: array of objects (QVariantMap from backend)
//   totals: optional object keyed by column.key -> value (renders a footer)
// Emits rowActivated(row) on double-tap.
Item {
    id: root
    property var columns: []
    property var rows: []
    property string emptyText: "No records yet"
    property bool loading: false
    property bool gridLines: true          // vertical column separators
    property var totals: ({})              // { key: value } -> sticky footer
    property string totalsLabel: "Total"
    signal rowActivated(var row)

    // Shared horizontal insets so header, body and footer align perfectly.
    readonly property int hPad: 16
    readonly property int cellGap: 12      // inner right padding inside a cell

    function alignFor(c) {
        return c.align === "right" ? Text.AlignRight
             : c.align === "center" ? Text.AlignHCenter
             : Text.AlignLeft
    }
    function fmt(c, v) {
        if (v === undefined || v === null) return ""
        if (c.money) return backend.formatMoney(Number(v))
        if (c.date) return backend.formatDate(String(v))
        return String(v)
    }
    function cellText(c, row) { return fmt(c, row[c.key]) }

    property real totalWeight: {
        var t = 0
        for (var i = 0; i < columns.length; i++) t += (columns[i].weight ? columns[i].weight : 1)
        return t > 0 ? t : 1
    }
    function colWidth(c, fullWidth) {
        return (fullWidth - hPad * 2) * (c.weight ? c.weight : 1) / totalWeight
    }
    property bool hasTotals: {
        if (!totals) return false
        for (var k in totals) return true
        return false
    }

    // Rounded clip container (version-safe: avoids per-corner radius which is
    // Qt 6.7+ only). Header/footer are square; the container masks corners.
    Rectangle {
        anchors.fill: parent
        radius: Theme.rMd
        color: "transparent"
        clip: true

    Column {
        anchors.fill: parent
        spacing: 0

        // ---------- Header ----------
        Rectangle {
            id: header
            width: parent.width
            height: 44
            color: Theme.glassStrong

            Row {
                anchors.fill: parent
                anchors.leftMargin: root.hPad
                anchors.rightMargin: root.hPad
                Repeater {
                    model: root.columns
                    delegate: Item {
                        width: root.colWidth(modelData, root.width)
                        height: parent.height

                        // vertical separator (skip first column)
                        Rectangle {
                            visible: root.gridLines && index > 0
                            width: 1
                            height: parent.height * 0.5
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            color: Theme.glassBorderSoft
                        }
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: root.cellGap
                            anchors.rightMargin: root.cellGap
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: root.alignFor(modelData)
                            text: modelData.title
                            color: Theme.textDim
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsTiny
                            font.weight: Font.Bold
                            font.capitalization: Font.AllUppercase
                            font.letterSpacing: 0.5
                            elide: Text.ElideRight
                        }
                    }
                }
            }
            // header bottom rule
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.glassBorder
            }
        }

        // ---------- Body ----------
        ListView {
            id: list
            width: parent.width
            height: root.height - 44 - (root.hasTotals ? 46 : 0)
            clip: true
            model: root.rows
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick

            // Visible scrollbar (not just an indicator) so content is obviously scrollable
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                width: 8
                contentItem: Rectangle {
                    implicitWidth: 8
                    radius: 4
                    color: Theme.alpha(Theme.accent, 0.45)
                }
                background: Rectangle {
                    implicitWidth: 8
                    radius: 4
                    color: Theme.alpha(Theme.glass, 0.2)
                }
            }

            focus: true
            keyNavigationEnabled: true
            keyNavigationWraps: false
            highlightMoveDuration: Theme.durFast
            currentIndex: -1

            // Keyboard: Enter activates row, Up/Down moves
            Keys.onReturnPressed: { if (currentIndex >= 0 && root.rows[currentIndex]) root.rowActivated(root.rows[currentIndex]) }
            Keys.onEnterPressed: { if (currentIndex >= 0 && root.rows[currentIndex]) root.rowActivated(root.rows[currentIndex]) }

            delegate: Rectangle {
                id: rowItem
                width: list.width
                height: 46
                property var rowData: modelData
                property bool isCurrent: list.currentIndex === index
                color: isCurrent ? Theme.alpha(Theme.accent, 0.14)
                     : rowHover.hovered ? Theme.rowHover
                     : (index % 2 === 0 ? "transparent" : Theme.rowAlt)
                Behavior on color { ColorAnimation { duration: Theme.durFast } }

                // row bottom separator
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.glassBorderSoft
                }

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: root.hPad
                    anchors.rightMargin: root.hPad
                    Repeater {
                        model: root.columns
                        delegate: Item {
                            id: cell
                            property var col: modelData
                            property string value: root.cellText(modelData, rowItem.rowData)
                            width: root.colWidth(modelData, list.width)
                            height: parent.height

                            // vertical grid line (skip first column)
                            Rectangle {
                                visible: root.gridLines && index > 0
                                width: 1
                                height: parent.height
                                anchors.left: parent.left
                                color: Theme.gridLine
                            }

                            // Chip rendering for type/status columns
                            Rectangle {
                                visible: cell.col.chip === true && cell.value.length > 0
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: root.cellGap
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
                                anchors.leftMargin: root.cellGap
                                anchors.rightMargin: root.cellGap
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

        // ---------- Totals footer (optional) ----------
        Rectangle {
            visible: root.hasTotals
            width: parent.width
            height: 46
            color: Theme.glassStrong

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 2
                color: Theme.alpha(Theme.accent, 0.5)
            }

            Row {
                anchors.fill: parent
                anchors.leftMargin: root.hPad
                anchors.rightMargin: root.hPad
                Repeater {
                    model: root.columns
                    delegate: Item {
                        property var col: modelData
                        property bool isFirst: index === 0
                        property bool hasVal: root.totals && (root.totals[col.key] !== undefined)
                        width: root.colWidth(modelData, root.width)
                        height: parent.height

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: root.cellGap
                            anchors.rightMargin: root.cellGap
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: isFirst && !hasVal ? Text.AlignLeft : root.alignFor(col)
                            text: hasVal ? root.fmt(col, root.totals[col.key])
                                         : (isFirst ? root.totalsLabel : "")
                            color: Theme.text
                            font.family: col.money || hasVal ? Theme.monoFamily : Theme.fontFamily
                            font.pixelSize: Theme.fsSmall
                            font.weight: Font.Bold
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }
    }
}
