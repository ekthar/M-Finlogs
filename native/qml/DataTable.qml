import QtQuick
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: root
    property var columns: []
    property var rows: []
    property string emptyText: "No records yet"
    property bool loading: false
    property bool gridLines: true
    property var totals: ({})
    property string totalsLabel: "Total"
    property bool checkable: false
    property var checkedRows: []
    signal rowActivated(var row)
    signal checkChanged(var checked, var row)
    signal inlineEdit(var row, var columnKey, var newValue)

    readonly property int hPad: 16
    readonly property int cellGap: 12
    readonly property int checkColWidth: 44

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
        return (fullWidth - hPad * 2 - (checkable ? checkColWidth : 0)) * (c.weight ? c.weight : 1) / totalWeight
    }
    property bool hasTotals: {
        if (!totals) return false
        for (var k in totals) return true
        return false
    }

    function isChecked(rowId) {
        for (var i = 0; i < checkedRows.length; i++) {
            if (checkedRows[i] === rowId) return true
        }
        return false
    }

    function toggleCheck(rowId) {
        var idx = -1
        for (var i = 0; i < checkedRows.length; i++) {
            if (checkedRows[i] === rowId) { idx = i; break }
        }
        if (idx >= 0) {
            checkedRows.splice(idx, 1)
        } else {
            checkedRows.push(rowId)
        }
        checkChanged(isChecked(rowId), rowId)
    }

    function selectAll() {
        checkedRows = []
        for (var i = 0; i < rows.length; i++) {
            checkedRows.push(rows[i].id)
        }
    }

    function deselectAll() {
        checkedRows = []
    }

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
                // Check-all column (if checkable)
                Item {
                    visible: root.checkable
                    width: root.checkColWidth
                    height: parent.height
                    Rectangle {
                        anchors.centerIn: parent
                        width: 18; height: 18; radius: 4
                        color: "transparent"
                        border.width: 2
                        border.color: Theme.textFaint
                        Text {
                            anchors.centerIn: parent
                            text: "\u2713"
                            color: Theme.accent
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            visible: checkedRows.length > 0 && checkedRows.length === rows.length
                        }
                        TapHandler {
                            onTapped: checkedRows.length === rows.length ? deselectAll() : selectAll()
                        }
                    }
                }
                Repeater {
                    model: root.columns
                    delegate: Item {
                        width: root.colWidth(modelData, root.width)
                        height: parent.height

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
            model: root.loading ? 8 : root.rows
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick

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

            Keys.onReturnPressed: { if (currentIndex >= 0 && root.rows[currentIndex]) root.rowActivated(root.rows[currentIndex]) }
            Keys.onEnterPressed: { if (currentIndex >= 0 && root.rows[currentIndex]) root.rowActivated(root.rows[currentIndex]) }

            delegate: Rectangle {
                id: rowItem
                width: list.width
                height: root.loading ? 46 : 46
                property var rowData: root.loading ? null : modelData
                property bool isCurrent: list.currentIndex === index

                // Skeleton shimmer
                Rectangle {
                    anchors.fill: parent
                    visible: root.loading
                    color: Theme.bg2
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: root.hPad
                        anchors.rightMargin: root.hPad
                        spacing: root.cellGap
                        Repeater {
                            model: root.columns.length
                            Rectangle {
                                width: (parent.width - (root.columns.length - 1) * root.cellGap) / Math.max(1, root.columns.length)
                                height: 14
                                y: 16
                                radius: 4
                                color: Theme.alpha(Theme.textFaint, 0.1)
                                SequentialAnimation on opacity {
                                    loops: Animation.Infinite
                                    PropertyAnimation { from: 0.3; to: 0.7; duration: 1000; easing.type: Easing.InOutSine }
                                    PropertyAnimation { from: 0.7; to: 0.3; duration: 1000; easing.type: Easing.InOutSine }
                                }
                            }
                        }
                    }
                }

                color: root.loading ? "transparent"
                     : isCurrent ? Theme.alpha(Theme.accent, 0.14)
                     : rowHover.hovered ? Theme.rowHover
                     : (index % 2 === 0 ? "transparent" : Theme.rowAlt)
                Behavior on color { ColorAnimation { duration: Theme.durFast } }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    visible: !root.loading
                    color: Theme.glassBorderSoft
                }

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: root.hPad
                    anchors.rightMargin: root.hPad
                    visible: !root.loading

                    // Checkbox column
                    Item {
                        visible: root.checkable
                        width: root.checkColWidth
                        height: parent.height
                        Rectangle {
                            anchors.centerIn: parent
                            width: 18; height: 18; radius: 4
                            color: isChecked(rowData.id) ? Theme.accent : "transparent"
                            border.width: 2
                            border.color: isChecked(rowData.id) ? Theme.accent : Theme.textFaint
                            Text {
                                anchors.centerIn: parent
                                text: "\u2713"
                                color: Theme.accentInk
                                font.pixelSize: 12
                                font.weight: Font.Bold
                                visible: isChecked(rowData.id)
                            }
                            TapHandler {
                                onTapped: toggleCheck(rowData.id)
                            }
                        }
                    }

                    Repeater {
                        model: root.columns
                        delegate: Item {
                            id: cell
                            property var col: modelData
                            property string value: root.cellText(modelData, rowItem.rowData)
                            property bool editing: false
                            width: root.colWidth(modelData, list.width)
                            height: parent.height

                            Rectangle {
                                visible: root.gridLines && index > 0
                                width: 1
                                height: parent.height
                                anchors.left: parent.left
                                color: Theme.gridLine
                            }

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

                            // Inline editor (for editable columns)
                            TextField {
                                id: inlineEditor
                                anchors.fill: parent
                                anchors.leftMargin: root.cellGap - 4
                                anchors.rightMargin: root.cellGap - 4
                                anchors.verticalCenter: parent.verticalCenter
                                visible: cell.editing && cell.col.editable
                                height: 30
                                color: Theme.text
                                font.family: cell.col.money ? Theme.monoFamily : Theme.fontFamily
                                font.pixelSize: Theme.fsSmall
                                font.weight: cell.col.money ? Font.DemiBold : Font.Normal
                                background: Rectangle {
                                    radius: Theme.rSm
                                    color: Theme.bg2
                                    border.width: 2
                                    border.color: Theme.accent
                                }
                                onAccepted: {
                                    root.inlineEdit(rowItem.rowData, cell.col.key, text)
                                    cell.editing = false
                                }
                                onEditingFinished: {
                                    cell.editing = false
                                }
                            }

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: root.cellGap
                                anchors.rightMargin: root.cellGap
                                visible: !cell.editing && cell.col.chip !== true
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
                TapHandler {
                    onDoubleTapped: {
                        if (!root.loading && rowItem.rowData) {
                            // Check if double-clicked column supports inline editing
                            root.rowActivated(rowItem.rowData)
                        }
                    }
                }
                TapHandler {
                    onTapped: {
                        if (!root.loading && rowItem.rowData) {
                            list.currentIndex = index
                        }
                    }
                }
            }

            // Empty state (when not loading)
            Column {
                anchors.centerIn: parent
                visible: (!root.loading) && (!root.rows || root.rows.length === 0)
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

            // Loading skeleton
            Column {
                anchors.centerIn: parent
                visible: root.loading
                spacing: 8
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "\u23F3"
                    color: Theme.textFaint
                    font.pixelSize: 24
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Loading..."
                    color: Theme.textFaint
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsSmall
                }
            }
        }

        // ---------- Totals footer (optional) ----------
        Rectangle {
            visible: root.hasTotals && !root.loading
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
                Item {
                    visible: root.checkable
                    width: root.checkColWidth
                }
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
