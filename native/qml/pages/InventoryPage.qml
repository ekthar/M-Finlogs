import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Window
import MFinlogs

Item {
    id: page

    // ── State ──────────────────────────────────────────────────────────────
    property var rows: []
    property var stockRows: []
    property var fyList: []
    property string selectedFy: ""
    property int selectedMonth: new Date().getMonth() + 1
    property int viewMode: 0   // 0=Both, 1=Stock Quantities, 2=Purchase Entries
    property string newProductName: ""
    property int purchaseDay: new Date().getDate()
    property string purchaseQty: ""
    property int purchaseRowIdx: -1
    property bool purchaseDialogOpen: false

    // Focus tracking for grid keyboard navigation
    property int focusRow: 0
    property int focusCol: 0  // 0=name, 1=cost, 2=min_stock, 3+=day cells
    function gridColCount() {
        var n = 0
        if (showQty()) n++
        if (showPurchase()) n++
        return 3 + n * daysInMonth()  // 3 frozen + day cells
    }
    // Move focus down/up by walking the focus chain forward/backward
    // by exactly gridColCount() steps (each row has that many tab-stops).
    function moveFocusDown() {
        var item = page.Window.window ? page.Window.window.activeFocusItem : null
        if (!item) return
        var steps = gridColCount()
        for (var i = 0; i < steps; i++) {
            item = item.nextItemInFocusChain(true)
            if (!item) return
        }
        item.forceActiveFocus()
    }
    function moveFocusUp() {
        var item = page.Window.window ? page.Window.window.activeFocusItem : null
        if (!item) return
        var steps = gridColCount()
        for (var i = 0; i < steps; i++) {
            item = item.nextItemInFocusChain(false)
            if (!item) return
        }
        item.forceActiveFocus()
    }
    // Signal to notify cells to check if they should grab focus
    signal focusCellChanged()

    readonly property var monthNames: ["January","February","March","April","May",
        "June","July","August","September","October","November","December"]
    readonly property int today: new Date().getDate()


    // ── Computed helpers ───────────────────────────────────────────────────
    function daysInMonth() {
        var y = parseInt(selectedFy.split("-")[0]) || new Date().getFullYear()
        return new Date(y, selectedMonth, 0).getDate()
    }

    function pad2(n) { return n < 10 ? "0" + n : "" + n }

    function qtyKey(day) { return "qty_" + pad2(day) }
    function purKey(day) { return "purchase_" + pad2(day) }

    function showQty() { return viewMode === 0 || viewMode === 1 }
    function showPurchase() { return viewMode === 0 || viewMode === 2 }

    // ── Metrics ───────────────────────────────────────────────────────────
    function totalQuantity() {
        var t = 0
        for (var i = 0; i < rows.length; i++) {
            var k = qtyKey(today)
            t += Number(rows[i][k]) || 0
        }
        return t
    }
    function purchaseToday() {
        var t = 0
        for (var i = 0; i < rows.length; i++) {
            var k = purKey(today)
            t += Number(rows[i][k]) || 0
        }
        return t
    }

    function avgDailyMovement() {
        var total = 0; var count = 0
        var dm = daysInMonth()
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

    // ── Data operations ───────────────────────────────────────────────────
    Component.onCompleted: {
        fyList = backend.financialYears()
        if (fyList.length > 0) selectedFy = String(fyList[0])
        load()
    }
    Connections { target: backend; function onDataChanged() { page.load() } }

    function load() {
        if (!selectedFy) return
        rows = backend.inventorySnapshot(selectedFy, selectedMonth)
        stockRows = backend.stockValue(selectedFy, selectedMonth)
    }


    function saveAll() {
        var data = []
        for (var i = 0; i < rows.length; i++) data.push(rows[i])
        var res = backend.saveInventory(selectedFy, selectedMonth, data)
        if (res && res.error) console.warn("Save failed:", res.error)
    }

    function addProduct(name) {
        if (!name || name.trim().length === 0) return
        var r = { row_id: -1, name: name.trim(), cost: 0, min_stock: 0 }
        var dm = daysInMonth()
        for (var d = 1; d <= dm; d++) {
            r[qtyKey(d)] = 0
            r[purKey(d)] = 0
        }
        var tmp = rows.slice()
        tmp.push(r)
        rows = tmp
    }

    function addGapRow() {
        var r = { row_id: -1, name: "", cost: 0, min_stock: 0 }
        var dm = daysInMonth()
        for (var d = 1; d <= dm; d++) {
            r[qtyKey(d)] = 0
            r[purKey(d)] = 0
        }
        var tmp = rows.slice()
        tmp.push(r)
        rows = tmp
    }

    function cleanEmptyRows() {
        var tmp = []
        for (var i = 0; i < rows.length; i++) {
            if (rows[i].name && rows[i].name.trim().length > 0) tmp.push(rows[i])
        }
        rows = tmp
    }


    function recordPurchase() {
        if (purchaseRowIdx < 0 || purchaseRowIdx >= rows.length) return
        var qty = parseInt(purchaseQty) || 0
        if (qty <= 0) return
        var tmp = rows.slice()
        var key = purKey(purchaseDay)
        tmp[purchaseRowIdx][key] = (Number(tmp[purchaseRowIdx][key]) || 0) + qty
        // Also add to stock qty for that day
        var qk = qtyKey(purchaseDay)
        tmp[purchaseRowIdx][qk] = (Number(tmp[purchaseRowIdx][qk]) || 0) + qty
        rows = tmp
        purchaseDialogOpen = false
        purchaseQty = ""
    }

    function exportExcel() {
        var cols = buildExportCols()
        var expRows = buildExportRows()
        backend.exportTableToExcel("Inventory " + monthNames[selectedMonth-1] + " " + selectedFy, cols, expRows)
    }
    function exportPdf() {
        var cols = buildExportCols()
        var expRows = buildExportRows()
        backend.exportTableToPdf("Inventory " + monthNames[selectedMonth-1] + " " + selectedFy, cols, expRows)
    }
    function buildExportCols() {
        var c = ["Name","Cost","Min Stock"]
        var dm = daysInMonth()
        for (var d = 1; d <= dm; d++) {
            if (showQty()) c.push("Q" + d)
            if (showPurchase()) c.push("P" + d)
        }
        return c
    }
    function buildExportRows() {
        var out = []
        for (var i = 0; i < rows.length; i++) {
            var r = [rows[i].name, rows[i].cost, rows[i].min_stock]
            var dm = daysInMonth()
            for (var d = 1; d <= dm; d++) {
                if (showQty()) r.push(rows[i][qtyKey(d)] || 0)
                if (showPurchase()) r.push(rows[i][purKey(d)] || 0)
            }
            out.push(r)
        }
        return out
    }


    // ── UI Layout ─────────────────────────────────────────────────────────
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

        // ── Toolbar ──────────────────────────────────────────────────────
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
                        text: page.newProductName
                        showCompletions: false
                        onAccepted: {
                            page.addProduct(newProductField.text)
                            newProductField.text = ""
                        }
                    }
                    PrimaryButton {
                        Layout.alignment: Qt.AlignBottom
                        text: "Add"
                        onClicked: {
                            page.addProduct(newProductField.text)
                            newProductField.text = ""
                        }
                    }
                }


                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s3

                    GhostButton {
                        text: "Record Purchase"
                        tint: Theme.accent2
                        onClicked: { page.purchaseDialogOpen = true }
                    }
                    GhostButton {
                        text: "Add Gap"
                        onClicked: page.addGapRow()
                    }
                    GhostButton {
                        text: "Clean Empty"
                        tint: Theme.warning
                        onClicked: page.cleanEmptyRows()
                    }
                    Item { Layout.fillWidth: true }
                    GhostButton {
                        text: "Refresh"
                        tint: Theme.accent3
                        onClicked: page.load()
                    }
                    PrimaryButton {
                        text: "Save"
                        onClicked: page.saveAll()
                    }
                    GhostButton {
                        text: "Excel"
                        tint: Theme.success
                        onClicked: page.exportExcel()
                    }
                    GhostButton {
                        text: "PDF"
                        tint: Theme.danger
                        onClicked: page.exportPdf()
                    }
                }
            }
        }


        // ── Record Purchase Dialog (inline) ──────────────────────────────
        GlassPanel {
            Layout.fillWidth: true
            implicitHeight: purchaseRow.implicitHeight + Theme.s8
            radius: Theme.rLg
            visible: page.purchaseDialogOpen
            fillColor: Theme.alpha(Theme.accent2, 0.08)
            borderColor: Theme.alpha(Theme.accent2, 0.3)

            RowLayout {
                id: purchaseRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.s5
                spacing: Theme.s3

                Text {
                    text: "\u26A1 Record Purchase"
                    color: Theme.accent2
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
                        for (var i = 0; i < page.rows.length; i++)
                            names.push(page.rows[i].name || ("Row " + (i+1)))
                        return names
                    }
                    onCurrentIndexChanged: page.purchaseRowIdx = currentIndex
                }


                FieldCombo {
                    id: purchaseDayCombo
                    Layout.preferredWidth: 100
                    label: "Day"
                    options: {
                        var days = []
                        for (var d = 1; d <= page.daysInMonth(); d++) days.push(String(d))
                        return days
                    }
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
                PrimaryButton {
                    Layout.alignment: Qt.AlignBottom
                    text: "Record"
                    onClicked: page.recordPurchase()
                }
                GhostButton {
                    Layout.alignment: Qt.AlignBottom
                    text: "Cancel"
                    tint: Theme.danger
                    onClicked: page.purchaseDialogOpen = false
                }
            }
        }


        // ── Metrics Panel ────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s4

            MetricCard {
                Layout.fillWidth: true
                label: "TOTAL QUANTITY"
                value: page.totalQuantity()
                prefix: ""
                accent: Theme.accent
                glyph: "\u25A3"
            }
            MetricCard {
                Layout.fillWidth: true
                label: "PURCHASE TODAY"
                value: page.purchaseToday()
                prefix: ""
                accent: Theme.accent2
                glyph: "\u25B2"
            }
            MetricCard {
                Layout.fillWidth: true
                label: "AVG DAILY MOVEMENT"
                value: page.avgDailyMovement()
                prefix: ""
                accent: Theme.accent3
                glyph: "\u25C8"
            }
            MetricCard {
                Layout.fillWidth: true
                label: "REORDER PRODUCTS"
                value: page.reorderCount()
                prefix: ""
                accent: Theme.danger
                glyph: "\u26A0"
            }
        }


        // ── Editable Grid Table ──────────────────────────────────────────
        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rLg

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.s4
                spacing: Theme.s3

                // Grid header info
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Inventory Grid"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSection
                        font.weight: Font.Bold
                    }
                    Item { Layout.fillWidth: true }
                    StatusPill { text: page.rows.length + " products"; tint: Theme.accent }
                    StatusPill { text: page.daysInMonth() + " days"; tint: Theme.accent3 }
                }

                // Grid container: fixed left + scrollable right
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    // Empty state
                    Text {
                        anchors.centerIn: parent
                        visible: page.rows.length === 0
                        text: "No inventory items \u2014 add products above"
                        color: Theme.textFaint
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsBody
                    }


                    // Vertical scroll for rows
                    Flickable {
                        id: vertFlick
                        anchors.fill: parent
                        visible: page.rows.length > 0
                        contentHeight: gridBody.height + headerRowHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        readonly property int headerRowHeight: 36
                        readonly property int rowH: 34
                        readonly property int frozenW: 320
                        readonly property int cellW: 62
                        readonly property int colsVisible: {
                            var n = 0
                            if (page.showQty()) n++
                            if (page.showPurchase()) n++
                            return n * page.daysInMonth()
                        }

                        // Header row
                        Row {
                            id: headerRow
                            y: 0
                            z: 2
                            height: vertFlick.headerRowHeight

                            // Frozen header cells
                            Row {
                                height: parent.height
                                Rectangle {
                                    width: 160; height: parent.height
                                    color: Theme.glassStrong
                                    border.width: 1; border.color: Theme.glassBorder
                                    Text { anchors.centerIn: parent; text: "Product"
                                        color: Theme.textDim; font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fsTiny; font.weight: Font.Bold }
                                }
                                Rectangle {
                                    width: 80; height: parent.height
                                    color: Theme.glassStrong
                                    border.width: 1; border.color: Theme.glassBorder
                                    Text { anchors.centerIn: parent; text: "Cost"
                                        color: Theme.textDim; font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fsTiny; font.weight: Font.Bold }
                                }
                                Rectangle {
                                    width: 80; height: parent.height
                                    color: Theme.glassStrong
                                    border.width: 1; border.color: Theme.glassBorder
                                    Text { anchors.centerIn: parent; text: "Min"
                                        color: Theme.textDim; font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fsTiny; font.weight: Font.Bold }
                                }
                            }


                            // Scrollable header cells
                            Item {
                                width: vertFlick.colsVisible * vertFlick.cellW
                                height: parent.height
                                clip: true

                                Row {
                                    x: -horzFlick.contentX
                                    height: parent.height
                                    Repeater {
                                        model: page.daysInMonth()
                                        Row {
                                            height: vertFlick.headerRowHeight
                                            property int dayNum: index + 1
                                            // Qty header
                                            Rectangle {
                                                visible: page.showQty()
                                                width: vertFlick.cellW; height: parent.height
                                                color: dayNum === page.today ? Theme.alpha(Theme.accent, 0.15) : Theme.glassStrong
                                                border.width: 1; border.color: Theme.glassBorder
                                                Text { anchors.centerIn: parent
                                                    text: "Q" + dayNum
                                                    color: dayNum === page.today ? Theme.accent : Theme.textDim
                                                    font.family: Theme.monoFamily
                                                    font.pixelSize: Theme.fsTiny
                                                    font.weight: Font.Bold }
                                            }
                                            // Purchase header
                                            Rectangle {
                                                visible: page.showPurchase()
                                                width: vertFlick.cellW; height: parent.height
                                                color: dayNum === page.today ? Theme.alpha(Theme.accent2, 0.15) : Theme.glassStrong
                                                border.width: 1; border.color: Theme.glassBorder
                                                Text { anchors.centerIn: parent
                                                    text: "P" + dayNum
                                                    color: dayNum === page.today ? Theme.accent2 : Theme.textFaint
                                                    font.family: Theme.monoFamily
                                                    font.pixelSize: Theme.fsTiny
                                                    font.weight: Font.Bold }
                                            }
                                        }
                                    }
                                }
                            }
                        }


                        // Body rows
                        Column {
                            id: gridBody
                            y: vertFlick.headerRowHeight

                            Repeater {
                                model: page.rows.length
                                delegate: Row {
                                    id: rowDelegate
                                    property int rowIdx: index
                                    property var rowData: page.rows[index]
                                    property bool belowMin: {
                                        var qty = Number(rowData[page.qtyKey(page.today)]) || 0
                                        var min = Number(rowData.min_stock) || 0
                                        return min > 0 && qty <= min
                                    }
                                    height: vertFlick.rowH

                                    // Frozen left: Name
                                    Rectangle {
                                        width: 160; height: vertFlick.rowH
                                        color: rowDelegate.belowMin ? Theme.alpha(Theme.danger, 0.08) : (rowIdx % 2 === 0 ? Theme.alpha(Theme.glass, 0.3) : "transparent")
                                        border.width: 1; border.color: Theme.alpha(Theme.glassBorder, 0.5)
                                        TextField {
                                            anchors.fill: parent
                                            anchors.margins: 2
                                            text: rowData.name || ""
                                            color: rowDelegate.belowMin ? Theme.danger : Theme.text
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fsTiny
                                            background: Item {}
                                            verticalAlignment: Text.AlignVCenter
                                            activeFocusOnTab: true
                                            Keys.onDownPressed: page.moveFocusDown()
                                            Keys.onUpPressed: page.moveFocusUp()
                                            Keys.onReturnPressed: page.moveFocusDown()
                                            Keys.onEnterPressed: page.moveFocusDown()
                                            onTextEdited: {
                                                var tmp = page.rows.slice()
                                                tmp[rowIdx].name = text
                                                page.rows = tmp
                                            }
                                        }
                                    }


                                    // Frozen left: Cost
                                    Rectangle {
                                        width: 80; height: vertFlick.rowH
                                        color: rowIdx % 2 === 0 ? Theme.alpha(Theme.glass, 0.3) : "transparent"
                                        border.width: 1; border.color: Theme.alpha(Theme.glassBorder, 0.5)
                                        TextField {
                                            anchors.fill: parent
                                            anchors.margins: 2
                                            text: String(rowData.cost || 0)
                                            color: Theme.text
                                            font.family: Theme.monoFamily
                                            font.pixelSize: Theme.fsTiny
                                            background: Item {}
                                            horizontalAlignment: Text.AlignRight
                                            verticalAlignment: Text.AlignVCenter
                                            activeFocusOnTab: true
                                            validator: DoubleValidator { bottom: 0 }
                                            Keys.onDownPressed: page.moveFocusDown()
                                            Keys.onUpPressed: page.moveFocusUp()
                                            Keys.onReturnPressed: page.moveFocusDown()
                                            Keys.onEnterPressed: page.moveFocusDown()
                                            onTextEdited: {
                                                var tmp = page.rows.slice()
                                                tmp[rowIdx].cost = parseFloat(text) || 0
                                                page.rows = tmp
                                            }
                                        }
                                    }
                                    // Frozen left: Min Stock
                                    Rectangle {
                                        width: 80; height: vertFlick.rowH
                                        color: rowIdx % 2 === 0 ? Theme.alpha(Theme.glass, 0.3) : "transparent"
                                        border.width: 1; border.color: Theme.alpha(Theme.glassBorder, 0.5)
                                        TextField {
                                            anchors.fill: parent
                                            anchors.margins: 2
                                            text: String(rowData.min_stock || 0)
                                            color: Theme.textDim
                                            font.family: Theme.monoFamily
                                            font.pixelSize: Theme.fsTiny
                                            background: Item {}
                                            horizontalAlignment: Text.AlignRight
                                            verticalAlignment: Text.AlignVCenter
                                            activeFocusOnTab: true
                                            validator: IntValidator { bottom: 0 }
                                            Keys.onDownPressed: page.moveFocusDown()
                                            Keys.onUpPressed: page.moveFocusUp()
                                            Keys.onReturnPressed: page.moveFocusDown()
                                            Keys.onEnterPressed: page.moveFocusDown()
                                            onTextEdited: {
                                                var tmp = page.rows.slice()
                                                tmp[rowIdx].min_stock = parseInt(text) || 0
                                                page.rows = tmp
                                            }
                                        }
                                    }


                                    // Scrollable day columns
                                    Item {
                                        width: vertFlick.colsVisible * vertFlick.cellW
                                        height: vertFlick.rowH
                                        clip: true

                                        Row {
                                            x: -horzFlick.contentX
                                            height: parent.height

                                            Repeater {
                                                model: page.daysInMonth()
                                                delegate: Row {
                                                    property int dayNum: index + 1
                                                    height: vertFlick.rowH

                                                    // Qty cell
                                                    Rectangle {
                                                        visible: page.showQty()
                                                        width: vertFlick.cellW; height: vertFlick.rowH
                                                        color: {
                                                            if (dayNum === page.today) return Theme.alpha(Theme.accent, 0.08)
                                                            return rowDelegate.rowIdx % 2 === 0 ? Theme.alpha(Theme.glass, 0.2) : "transparent"
                                                        }
                                                        border.width: 1
                                                        border.color: Theme.alpha(Theme.glassBorder, 0.4)
                                                        TextField {
                                                            anchors.fill: parent
                                                            anchors.margins: 1
                                                            text: String(rowDelegate.rowData[page.qtyKey(dayNum)] || 0)
                                                            color: {
                                                                var v = parseInt(text) || 0
                                                                var min = Number(rowDelegate.rowData.min_stock) || 0
                                                                if (min > 0 && v <= min) return Theme.danger
                                                                if (v > 0) return Theme.text
                                                                return Theme.textFaint
                                                            }
                                                            font.family: Theme.monoFamily
                                                            font.pixelSize: Theme.fsTiny
                                                            background: Item {}
                                                            horizontalAlignment: Text.AlignHCenter
                                                            verticalAlignment: Text.AlignVCenter
                                                            activeFocusOnTab: true
                                                            validator: IntValidator { bottom: 0 }
                                                            Keys.onDownPressed: page.moveFocusDown()
                                                            Keys.onUpPressed: page.moveFocusUp()
                                                            Keys.onReturnPressed: page.moveFocusDown()
                                                            Keys.onEnterPressed: page.moveFocusDown()
                                                            onTextEdited: {
                                                                var tmp = page.rows.slice()
                                                                tmp[rowDelegate.rowIdx][page.qtyKey(dayNum)] = parseInt(text) || 0
                                                                page.rows = tmp
                                                            }
                                                        }
                                                    }


                                                    // Purchase cell
                                                    Rectangle {
                                                        visible: page.showPurchase()
                                                        width: vertFlick.cellW; height: vertFlick.rowH
                                                        color: {
                                                            if (dayNum === page.today) return Theme.alpha(Theme.accent2, 0.08)
                                                            return rowDelegate.rowIdx % 2 === 0 ? Theme.alpha(Theme.glass, 0.15) : "transparent"
                                                        }
                                                        border.width: 1
                                                        border.color: Theme.alpha(Theme.glassBorder, 0.4)
                                                        TextField {
                                                            anchors.fill: parent
                                                            anchors.margins: 1
                                                            text: String(rowDelegate.rowData[page.purKey(dayNum)] || 0)
                                                            color: {
                                                                var v = parseInt(text) || 0
                                                                if (v > 0) return Theme.accent2
                                                                return Theme.textFaint
                                                            }
                                                            font.family: Theme.monoFamily
                                                            font.pixelSize: Theme.fsTiny
                                                            background: Item {}
                                                            horizontalAlignment: Text.AlignHCenter
                                                            verticalAlignment: Text.AlignVCenter
                                                            activeFocusOnTab: true
                                                            validator: IntValidator { bottom: 0 }
                                                            Keys.onDownPressed: page.moveFocusDown()
                                                            Keys.onUpPressed: page.moveFocusUp()
                                                            Keys.onReturnPressed: page.moveFocusDown()
                                                            Keys.onEnterPressed: page.moveFocusDown()
                                                            onTextEdited: {
                                                                var tmp = page.rows.slice()
                                                                tmp[rowDelegate.rowIdx][page.purKey(dayNum)] = parseInt(text) || 0
                                                                page.rows = tmp
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


                        // Horizontal scrollbar (invisible Flickable to drive x offset)
                        Flickable {
                            id: horzFlick
                            anchors.left: parent.left
                            anchors.leftMargin: vertFlick.frozenW
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 14
                            contentWidth: vertFlick.colsVisible * vertFlick.cellW
                            contentHeight: 14
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            flickableDirection: Flickable.HorizontalFlick

                            Rectangle {
                                width: parent.contentWidth
                                height: 14
                                color: "transparent"
                            }

                            ScrollBar.horizontal: ScrollBar {
                                policy: ScrollBar.AlwaysOn
                                height: 8
                                contentItem: Rectangle {
                                    implicitHeight: 8
                                    radius: 4
                                    color: Theme.alpha(Theme.accent, 0.4)
                                }
                                background: Rectangle {
                                    radius: 4
                                    color: Theme.alpha(Theme.glass, 0.3)
                                }
                            }
                        }

                        // Mouse wheel horizontal scroll
                        MouseArea {
                            anchors.fill: parent
                            anchors.leftMargin: vertFlick.frozenW
                            z: -1
                            acceptedButtons: Qt.NoButton
                            onWheel: function(wheel) {
                                if (wheel.angleDelta.x !== 0) {
                                    horzFlick.contentX = Math.max(0, Math.min(
                                        horzFlick.contentWidth - horzFlick.width,
                                        horzFlick.contentX - wheel.angleDelta.x))
                                }
                            }
                        }
                    }
                }
            }
        }


        // ── Stock Value Section ──────────────────────────────────────────
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
                    Text {
                        text: "Stock Value Summary"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSection
                        font.weight: Font.Bold
                    }
                    Item { Layout.fillWidth: true }
                    StatusPill {
                        text: page.stockRows.length + " entries"
                        tint: Theme.success
                    }
                }

                // Stock value table header
                Rectangle {
                    Layout.fillWidth: true
                    height: 30
                    color: Theme.glassStrong
                    radius: Theme.rSm
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.s4
                        anchors.rightMargin: Theme.s4
                        spacing: 0
                        Text {
                            Layout.preferredWidth: 80
                            text: "Day"
                            color: Theme.textDim
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsTiny
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Text {
                            Layout.fillWidth: true
                            text: "Total Qty"
                            color: Theme.textDim
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsTiny
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: "Stock Value"
                            color: Theme.textDim
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsTiny
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }


                // Stock value rows
                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentHeight: svCol.height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    Column {
                        id: svCol
                        width: parent.width

                        Repeater {
                            model: page.stockRows.length
                            delegate: Rectangle {
                                width: svCol.width
                                height: 28
                                color: index % 2 === 0 ? Theme.alpha(Theme.glass, 0.3) : "transparent"
                                radius: 4

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.s4
                                    anchors.rightMargin: Theme.s4
                                    spacing: 0
                                    Text {
                                        Layout.preferredWidth: 80
                                        text: String(page.stockRows[index].day || "")
                                        color: Theme.text
                                        font.family: Theme.monoFamily
                                        font.pixelSize: Theme.fsTiny
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: String(page.stockRows[index].quantity || 0)
                                        color: Theme.text
                                        font.family: Theme.monoFamily
                                        font.pixelSize: Theme.fsTiny
                                        horizontalAlignment: Text.AlignRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: "\u20B9 " + backend.formatMoney(page.stockRows[index].stock_value || 0)
                                        color: Theme.success
                                        font.family: Theme.monoFamily
                                        font.pixelSize: Theme.fsTiny
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: page.stockRows.length === 0
                        text: "No stock value data available"
                        color: Theme.textFaint
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSmall
                    }
                }
            }
        }
    }
}
