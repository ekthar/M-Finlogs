import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Window
import MFinlogs

Item {
    id: page

    // State
    property var rows: []
    property var stockRows: []
    property var fyList: []
    property string selectedFy: ""
    property int selectedMonth: new Date().getMonth() + 1
    property int viewMode: 0   // 0=Both, 1=Stock Quantities, 2=Purchase Entries
    property int purchaseDay: new Date().getDate()
    property string purchaseQty: ""
    property int purchaseRowIdx: -1
    property bool purchaseDialogOpen: false

    readonly property var monthNames: ["January","February","March","April","May",
        "June","July","August","September","October","November","December"]
    readonly property int today: new Date().getDate()
    property int cachedDaysInMonth: 28

    // Cached metrics (recomputed in recalcMetrics to avoid O(n) function calls in bindings)
    property real cachedTotalQty: 0
    property real cachedPurchaseToday: 0
    property real cachedAvgMovement: 0
    property int cachedReorderCount: 0

    // Layout constants for the virtualized grid
    readonly property int rowH: 34
    readonly property int headerH: 36
    readonly property int nameW: 160
    readonly property int costW: 80
    readonly property int minW: 80
    readonly property int cellW: 62
    readonly property int frozenW: nameW + costW + minW

    // Computed helpers
    function daysInMonth() {
        var y = parseInt(selectedFy.split("-")[0]) || new Date().getFullYear()
        return new Date(y, selectedMonth, 0).getDate()
    }
    function pad2(n) { return n < 10 ? "0" + n : "" + n }
    function qtyKey(day) { return "qty_" + pad2(day) }
    function purKey(day) { return "purchase_" + pad2(day) }
    function showQty() { return viewMode === 0 || viewMode === 1 }
    function showPurchase() { return viewMode === 0 || viewMode === 2 }
    function colsPerDay() { return (showQty() ? 1 : 0) + (showPurchase() ? 1 : 0) }
    function scrollWidth() { return daysInMonth() * colsPerDay() * cellW }

    // Metrics
    function totalQuantity() {
        var t = 0
        for (var i = 0; i < rows.length; i++) t += Number(rows[i][qtyKey(today)]) || 0
        return t
    }
    function purchaseToday() {
        var t = 0
        for (var i = 0; i < rows.length; i++) t += Number(rows[i][purKey(today)]) || 0
        return t
    }
    function avgDailyMovement() {
        var total = 0, count = 0, dm = daysInMonth()
        for (var i = 0; i < rows.length; i++) {
            for (var d = 1; d <= dm; d++) {
                var v = Number(rows[i][purKey(d)]) || 0
                total += v
                if (v > 0) count++
            }
        }
        return count > 0 ? Math.round(total / count) : 0
    }
    function reorderCount() {
        var c = 0
        for (var i = 0; i < rows.length; i++) {
            var qty = Number(rows[i][qtyKey(today)]) || 0
            var min = Number(rows[i].min_stock) || 0
            if (min > 0 && qty <= min) c++
        }
        return c
    }

    function recalcMetrics() {
        var tQty = 0, tPur = 0, movTotal = 0, movCount = 0, reorder = 0
        var dm = cachedDaysInMonth
        var dToday = today
        for (var i = 0; i < rows.length; i++) {
            var row = rows[i]
            tQty += Number(row[qtyKey(dToday)]) || 0
            tPur += Number(row[purKey(dToday)]) || 0
            var qty = Number(row[qtyKey(dToday)]) || 0
            var min = Number(row.min_stock) || 0
            if (min > 0 && qty <= min) reorder++
            for (var d = 1; d <= dm; d++) {
                var v = Number(row[purKey(d)]) || 0
                movTotal += v
                if (v > 0) movCount++
            }
        }
        cachedTotalQty = tQty
        cachedPurchaseToday = tPur
        cachedReorderCount = reorder
        cachedAvgMovement = movCount > 0 ? Math.round(movTotal / movCount) : 0
    }

    // Data operations
    Component.onCompleted: {
        fyList = backend.financialYears()
        if (fyList.length > 0) selectedFy = String(fyList[0])
        load()
    }
    Connections { target: backend; function onDataChanged() { page.load() } }

    function load() {
        if (!selectedFy) return
        cachedDaysInMonth = daysInMonth()
        rows = backend.inventorySnapshot(selectedFy, selectedMonth)
        stockRows = backend.stockValue(selectedFy, selectedMonth)
        recalcMetrics()
    }

    // Mutate a single field of a single row WITHOUT replacing the whole array,
    // so the ListView delegate doesn't rebuild on every keystroke.
    function setCell(rowIdx, key, value) {
        if (rowIdx < 0 || rowIdx >= rows.length) return
        rows[rowIdx][key] = value
    }

    function saveAll() {
        var res = backend.saveInventory(selectedFy, selectedMonth, rows)
        if (res && res.error) console.warn("Save failed:", res.error)
    }

    function blankRow(name) {
        var r = { row_id: -1, name: name || "", cost: 0, min_stock: 0 }
        var dm = daysInMonth()
        for (var d = 1; d <= dm; d++) { r[qtyKey(d)] = 0; r[purKey(d)] = 0 }
        return r
    }
    function addProduct(name) {
        if (!name || name.trim().length === 0) return
        var tmp = rows.slice(); tmp.push(blankRow(name.trim())); rows = tmp
        recalcMetrics()
    }
    function addGapRow() { var tmp = rows.slice(); tmp.push(blankRow("")); rows = tmp; recalcMetrics() }
    function cleanEmptyRows() {
        var tmp = []
        for (var i = 0; i < rows.length; i++)
            if (rows[i].name && rows[i].name.trim().length > 0) tmp.push(rows[i])
        rows = tmp
        recalcMetrics()
    }
    function recordPurchase() {
        if (purchaseRowIdx < 0 || purchaseRowIdx >= rows.length) return
        var qty = parseInt(purchaseQty) || 0
        if (qty <= 0) return
        var tmp = rows.slice()
        tmp[purchaseRowIdx][purKey(purchaseDay)] = (Number(tmp[purchaseRowIdx][purKey(purchaseDay)]) || 0) + qty
        tmp[purchaseRowIdx][qtyKey(purchaseDay)] = (Number(tmp[purchaseRowIdx][qtyKey(purchaseDay)]) || 0) + qty
        rows = tmp
        recalcMetrics()
        purchaseDialogOpen = false
        purchaseQty = ""
    }

    function exportExcel() { backend.exportTableToExcel("Inventory " + monthNames[selectedMonth-1] + " " + selectedFy, buildExportCols(), buildExportRows()) }
    function exportPdf() { page.saveAll(); backend.inventoryPdfPreview(selectedFy, selectedMonth, false) }
    function buildExportCols() {
        var c = ["Name","Cost","Min Stock"], dm = daysInMonth()
        for (var d = 1; d <= dm; d++) { if (showQty()) c.push("Q"+d); if (showPurchase()) c.push("P"+d) }
        return c
    }
    function buildExportRows() {
        var out = [], dm = daysInMonth()
        for (var i = 0; i < rows.length; i++) {
            var r = [rows[i].name, rows[i].cost, rows[i].min_stock]
            for (var d = 1; d <= dm; d++) { if (showQty()) r.push(rows[i][qtyKey(d)]||0); if (showPurchase()) r.push(rows[i][purKey(d)]||0) }
            out.push(r)
        }
        return out
    }

    // UI Layout
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s8
        anchors.rightMargin: Theme.s8
        anchors.topMargin: Theme.s5
        anchors.bottomMargin: Theme.s6
        spacing: Theme.s5

        SectionHeader {
            title: "Inventory"
            subtitle: "Daily stock quantities and purchase tracking \u2014 " + monthNames[selectedMonth - 1]
        }

        // Toolbar
        GlassPanel {
            Layout.fillWidth: true
            implicitHeight: toolbarCol.implicitHeight + Theme.s8
            radius: Theme.rLg

            ColumnLayout {
                id: toolbarCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.s5
                spacing: Theme.s3

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s3
                    FieldCombo {
                        id: monthCombo
                        Layout.preferredWidth: 150
                        label: "Month"
                        options: page.monthNames
                        currentIndex: page.selectedMonth - 1
                        onCurrentIndexChanged: { page.selectedMonth = currentIndex + 1; page.load() }
                    }
                    FieldCombo {
                        id: fyCombo
                        Layout.preferredWidth: 180
                        label: "Financial Year"
                        options: page.fyList
                        onCurrentTextChanged: { page.selectedFy = currentText; page.load() }
                    }
                    FieldCombo {
                        id: viewModeCombo
                        Layout.preferredWidth: 180
                        label: "View Mode"
                        options: ["Both", "Stock Quantities", "Purchase Entries"]
                        currentIndex: page.viewMode
                        onCurrentIndexChanged: page.viewMode = currentIndex
                    }
                    Item { Layout.fillWidth: true }
                    FieldInput {
                        id: newProductField
                        Layout.preferredWidth: 180
                        label: "Add Product"
                        placeholder: "Product name"
                        showCompletions: false
                        onAccepted: { page.addProduct(newProductField.text); newProductField.text = "" }
                    }
                    PrimaryButton {
                        Layout.alignment: Qt.AlignBottom
                        text: "Add"
                        onClicked: { page.addProduct(newProductField.text); newProductField.text = "" }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s3
                    GhostButton { text: "Record Purchase"; tint: Theme.palette.primary; onClicked: page.purchaseDialogOpen = true }
                    GhostButton { text: "Add Gap"; onClicked: page.addGapRow() }
                    GhostButton { text: "Clean Empty"; tint: Theme.warning; onClicked: page.cleanEmptyRows() }
                    Item { Layout.fillWidth: true }
                    GhostButton { text: "Refresh"; tint: Theme.palette.info; onClicked: page.load() }
                    PrimaryButton { text: "Save"; onClicked: page.saveAll() }
                    GhostButton { text: "Excel"; tint: Theme.success; onClicked: page.exportExcel() }
                    GhostButton { text: "PDF"; tint: Theme.danger; onClicked: page.exportPdf() }
                }
            }
        }

        // Record Purchase Dialog (inline)
        GlassPanel {
            Layout.fillWidth: true
            implicitHeight: purchaseRow.implicitHeight + Theme.s8
            radius: Theme.rLg
            visible: page.purchaseDialogOpen
            fillColor: Theme.alpha(Theme.palette.primary, 0.08)
            borderColor: Theme.alpha(Theme.palette.primary, 0.3)

            RowLayout {
                id: purchaseRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.s5
                spacing: Theme.s3
                Text {
                    text: "\u26A1 Record Purchase"
                    color: Theme.palette.primary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsSection
                    font.weight: Font.Bold
                }
                FieldCombo {
                    id: purchaseProductCombo
                    Layout.preferredWidth: 200
                    label: "Product"
                    options: {
                        var names = []
                        for (var i = 0; i < page.rows.length; i++) names.push(page.rows[i].name || ("Row " + (i+1)))
                        return names
                    }
                    onCurrentIndexChanged: page.purchaseRowIdx = currentIndex
                }
                FieldCombo {
                    id: purchaseDayCombo
                    Layout.preferredWidth: 100
                    label: "Day"
                    options: { var days = []; for (var d = 1; d <= page.cachedDaysInMonth; d++) days.push(String(d)); return days }
                    currentIndex: page.purchaseDay - 1
                    onCurrentIndexChanged: page.purchaseDay = currentIndex + 1
                }
                FieldInput {
                    id: purchaseQtyField
                    Layout.preferredWidth: 120
                    label: "Quantity"
                    placeholder: "0"
                    numeric: true
                    text: page.purchaseQty
                    showCompletions: false
                    onAccepted: page.recordPurchase()
                    Binding { target: page; property: "purchaseQty"; value: purchaseQtyField.text }
                }
                PrimaryButton { Layout.alignment: Qt.AlignBottom; text: "Record"; onClicked: page.recordPurchase() }
                GhostButton { Layout.alignment: Qt.AlignBottom; text: "Cancel"; tint: Theme.danger; onClicked: page.purchaseDialogOpen = false }
            }
        }

        // Metrics Panel
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s4
            MetricCard { Layout.fillWidth: true; label: "TOTAL QUANTITY"; value: page.cachedTotalQty; prefix: ""; accent: Theme.palette.primary; glyph: "\u25A3" }
            MetricCard { Layout.fillWidth: true; label: "PURCHASE TODAY"; value: page.cachedPurchaseToday; prefix: ""; accent: Theme.palette.primary; glyph: "\u25B2" }
            MetricCard { Layout.fillWidth: true; label: "AVG DAILY MOVEMENT"; value: page.cachedAvgMovement; prefix: ""; accent: Theme.palette.info; glyph: "\u25C8" }
            MetricCard { Layout.fillWidth: true; label: "REORDER PRODUCTS"; value: page.cachedReorderCount; prefix: ""; accent: Theme.danger; glyph: "\u26A0" }
        }

        // Editable Grid (virtualized via ListView)
        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rLg

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.s4
                spacing: Theme.s3

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Inventory Grid"
                        color: Theme.palette.fg
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSection
                        font.weight: Font.Bold
                    }
                    Item { Layout.fillWidth: true }
                    StatusPill { text: page.rows.length + " products"; tint: Theme.palette.primary }
                    StatusPill { text: page.daysInMonth() + " days"; tint: Theme.palette.info }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    // Shared horizontal scroll offset for header + all body rows
                    property real hScroll: 0

                    Text {
                        anchors.centerIn: parent
                        visible: page.rows.length === 0
                        text: "No inventory items \u2014 add products above"
                        color: Theme.palette.fgSubtle
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsBody
                    }

                    // Horizontal scrollbar at the bottom (drives gridScope.hScroll)
                    ScrollBar {
                        id: hbar
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: page.frozenW
                        height: 8
                        policy: ScrollBar.AlwaysOn
                        orientation: Qt.Horizontal
                        visible: page.rows.length > 0 && page.scrollWidth() > (parent.width - page.frozenW)
                        size: (parent.width - page.frozenW) / Math.max(page.scrollWidth(), 1)
                        onPositionChanged: {
                            var maxOff = Math.max(0, page.scrollWidth() - (parent.width - page.frozenW))
                            parent.hScroll = position * page.scrollWidth()
                            if (parent.hScroll > maxOff) parent.hScroll = maxOff
                        }
                        contentItem: Rectangle { radius: 4; color: Theme.alpha(Theme.palette.primary, 0.5) }
                        background: Rectangle { radius: 4; color: Theme.alpha(Theme.alpha(Theme.palette.fg, 0.04), 0.3) }
                    }

                    id: gridScope

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.bottomMargin: hbar.visible ? 12 : 0
                        spacing: 0
                        visible: page.rows.length > 0

                        // Header row
                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: page.headerH
                            clip: true

                            Row {
                                // frozen labels
                                Rectangle {
                                    width: page.nameW; height: page.headerH
                                    color: Theme.alpha(Theme.palette.fg, 0.06); border.width: 1; border.color: Theme.palette.border
                                    Text { anchors.centerIn: parent; text: "Product"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold }
                                }
                                Rectangle {
                                    width: page.costW; height: page.headerH
                                    color: Theme.alpha(Theme.palette.fg, 0.06); border.width: 1; border.color: Theme.palette.border
                                    Text { anchors.centerIn: parent; text: "Cost"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold }
                                }
                                Rectangle {
                                    width: page.minW; height: page.headerH
                                    color: Theme.alpha(Theme.palette.fg, 0.06); border.width: 1; border.color: Theme.palette.border
                                    Text { anchors.centerIn: parent; text: "Min"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold }
                                }
                            }

                            // scrollable day headers
                            Item {
                                x: page.frozenW
                                width: parent.width - page.frozenW
                                height: page.headerH
                                clip: true
                                Row {
                                    x: -gridScope.hScroll
                                    height: page.headerH
                                    Repeater {
                                        model: page.cachedDaysInMonth
                                        Row {
                                            height: page.headerH
                                            property int dayNum: index + 1
                                            Rectangle {
                                                visible: page.showQty()
                                                width: page.cellW; height: page.headerH
                                                color: dayNum === page.today ? Theme.alpha(Theme.palette.primary, 0.15) : Theme.alpha(Theme.palette.fg, 0.06)
                                                border.width: 1; border.color: Theme.palette.border
                                                Text { anchors.centerIn: parent; text: "Q" + dayNum
                                                    color: dayNum === page.today ? Theme.palette.primary : Theme.palette.fgMuted
                                                    font.family: Theme.monoFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold }
                                            }
                                            Rectangle {
                                                visible: page.showPurchase()
                                                width: page.cellW; height: page.headerH
                                                color: dayNum === page.today ? Theme.alpha(Theme.palette.primary, 0.15) : Theme.alpha(Theme.palette.fg, 0.06)
                                                border.width: 1; border.color: Theme.palette.border
                                                Text { anchors.centerIn: parent; text: "P" + dayNum
                                                    color: dayNum === page.today ? Theme.palette.primary : Theme.palette.fgSubtle
                                                    font.family: Theme.monoFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Virtualized body
                        ListView {
                            id: bodyList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: page.rows.length
                            cacheBuffer: page.rowH * 6
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                                width: 8
                                contentItem: Rectangle { radius: 4; color: Theme.alpha(Theme.palette.primary, 0.5) }
                            }

                            delegate: Item {
                                id: rowDelegate
                                width: bodyList.width
                                height: page.rowH
                                property int rowIdx: index
                                property var rowData: page.rows[index]
                                property bool belowMin: {
                                    var qty = Number(rowData[page.qtyKey(page.today)]) || 0
                                    var min = Number(rowData.min_stock) || 0
                                    return min > 0 && qty <= min
                                }

                                // frozen left cells
                                Row {
                                    id: frozenRow
                                    height: page.rowH
                                    Rectangle {
                                        width: page.nameW; height: page.rowH
                                        color: rowDelegate.belowMin ? Theme.alpha(Theme.danger, 0.10) : (rowIdx % 2 === 0 ? Theme.alpha(Theme.alpha(Theme.palette.fg, 0.04), 0.3) : "transparent")
                                        border.width: 1; border.color: Theme.alpha(Theme.palette.border, 0.5)
                                        TextField {
                                            anchors.fill: parent; anchors.margins: 2
                                            text: rowData.name || ""
                                            color: rowDelegate.belowMin ? Theme.danger : Theme.palette.fg
                                            font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny
                                            background: Item {}
                                            verticalAlignment: Text.AlignVCenter
                                            activeFocusOnTab: true
                                            KeyNavigation.down: null
                                            onTextEdited: page.setCell(rowDelegate.rowIdx, "name", text)
                                        }
                                    }
                                    Rectangle {
                                        width: page.costW; height: page.rowH
                                        color: rowIdx % 2 === 0 ? Theme.alpha(Theme.alpha(Theme.palette.fg, 0.04), 0.3) : "transparent"
                                        border.width: 1; border.color: Theme.alpha(Theme.palette.border, 0.5)
                                        TextField {
                                            anchors.fill: parent; anchors.margins: 2
                                            text: String(rowData.cost || 0)
                                            color: Theme.palette.fg
                                            font.family: Theme.monoFamily; font.pixelSize: Theme.fsTiny
                                            background: Item {}
                                            horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                                            activeFocusOnTab: true
                                            validator: DoubleValidator { bottom: 0 }
                                            onTextEdited: page.setCell(rowDelegate.rowIdx, "cost", parseFloat(text) || 0)
                                        }
                                    }
                                    Rectangle {
                                        width: page.minW; height: page.rowH
                                        color: rowIdx % 2 === 0 ? Theme.alpha(Theme.alpha(Theme.palette.fg, 0.04), 0.3) : "transparent"
                                        border.width: 1; border.color: Theme.alpha(Theme.palette.border, 0.5)
                                        TextField {
                                            anchors.fill: parent; anchors.margins: 2
                                            text: String(rowData.min_stock || 0)
                                            color: Theme.palette.fgMuted
                                            font.family: Theme.monoFamily; font.pixelSize: Theme.fsTiny
                                            background: Item {}
                                            horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                                            activeFocusOnTab: true
                                            validator: IntValidator { bottom: 0 }
                                            onTextEdited: page.setCell(rowDelegate.rowIdx, "min_stock", parseInt(text) || 0)
                                        }
                                    }
                                }

                                // scrollable day cells (only the visible days are realized)
                                Item {
                                    x: page.frozenW
                                    width: rowDelegate.width - page.frozenW
                                    height: page.rowH
                                    clip: true
                                    Row {
                                        x: -gridScope.hScroll
                                        height: page.rowH
                                        Repeater {
                                            model: page.cachedDaysInMonth
                                            Row {
                                                height: page.rowH
                                                property int dayNum: index + 1
                                                Rectangle {
                                                    visible: page.showQty()
                                                    width: page.cellW; height: page.rowH
                                                    color: dayNum === page.today ? Theme.alpha(Theme.palette.primary, 0.08)
                                                         : (rowDelegate.rowIdx % 2 === 0 ? Theme.alpha(Theme.alpha(Theme.palette.fg, 0.04), 0.2) : "transparent")
                                                    border.width: 1; border.color: Theme.alpha(Theme.palette.border, 0.4)
                                                    TextField {
                                                        anchors.fill: parent; anchors.margins: 1
                                                        text: String(rowDelegate.rowData[page.qtyKey(dayNum)] || 0)
                                                        color: {
                                                            var v = parseInt(text) || 0
                                                            var min = Number(rowDelegate.rowData.min_stock) || 0
                                                            if (min > 0 && v <= min) return Theme.danger
                                                            return v > 0 ? Theme.palette.fg : Theme.palette.fgSubtle
                                                        }
                                                        font.family: Theme.monoFamily; font.pixelSize: Theme.fsTiny
                                                        background: Item {}
                                                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                                        validator: IntValidator { bottom: 0 }
                                                        onTextEdited: page.setCell(rowDelegate.rowIdx, page.qtyKey(dayNum), parseInt(text) || 0)
                                                    }
                                                }
                                                Rectangle {
                                                    visible: page.showPurchase()
                                                    width: page.cellW; height: page.rowH
                                                    color: dayNum === page.today ? Theme.alpha(Theme.palette.primary, 0.08)
                                                         : (rowDelegate.rowIdx % 2 === 0 ? Theme.alpha(Theme.alpha(Theme.palette.fg, 0.04), 0.15) : "transparent")
                                                    border.width: 1; border.color: Theme.alpha(Theme.palette.border, 0.4)
                                                    TextField {
                                                        anchors.fill: parent; anchors.margins: 1
                                                        text: String(rowDelegate.rowData[page.purKey(dayNum)] || 0)
                                                        color: (parseInt(text) || 0) > 0 ? Theme.palette.primary : Theme.palette.fgSubtle
                                                        font.family: Theme.monoFamily; font.pixelSize: Theme.fsTiny
                                                        background: Item {}
                                                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                                        validator: IntValidator { bottom: 0 }
                                                        onTextEdited: page.setCell(rowDelegate.rowIdx, page.purKey(dayNum), parseInt(text) || 0)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Stock Value Summary
        GlassPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            radius: Theme.rLg
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.s5
                spacing: Theme.s3
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Stock Value Summary"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSection; font.weight: Font.Bold }
                    Item { Layout.fillWidth: true }
                    StatusPill { text: page.stockRows.length + " entries"; tint: Theme.success }
                }
                Rectangle {
                    Layout.fillWidth: true; height: 30; radius: Theme.rSm; color: Theme.alpha(Theme.palette.fg, 0.06)
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: Theme.s4; anchors.rightMargin: Theme.s4
                        Text { Layout.preferredWidth: 80; text: "Day"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold; horizontalAlignment: Text.AlignHCenter }
                        Text { Layout.fillWidth: true; text: "Total Qty"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold; horizontalAlignment: Text.AlignRight }
                        Text { Layout.fillWidth: true; text: "Stock Value"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold; horizontalAlignment: Text.AlignRight }
                    }
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: page.stockRows.length
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 8 }
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 28
                        color: index % 2 === 0 ? Theme.alpha(Theme.alpha(Theme.palette.fg, 0.04), 0.3) : "transparent"
                        radius: 4
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: Theme.s4; anchors.rightMargin: Theme.s4
                            Text { Layout.preferredWidth: 80; text: String(page.stockRows[index].day || ""); color: Theme.palette.fg; font.family: Theme.monoFamily; font.pixelSize: Theme.fsTiny; horizontalAlignment: Text.AlignHCenter }
                            Text { Layout.fillWidth: true; text: String(page.stockRows[index].quantity || 0); color: Theme.palette.fg; font.family: Theme.monoFamily; font.pixelSize: Theme.fsTiny; horizontalAlignment: Text.AlignRight }
                            Text { Layout.fillWidth: true; text: "\u20B9 " + backend.formatMoney(page.stockRows[index].stock_value || 0); color: Theme.success; font.family: Theme.monoFamily; font.pixelSize: Theme.fsTiny; horizontalAlignment: Text.AlignRight }
                        }
                    }
                }
            }
        }
    }
}
