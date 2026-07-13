import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: page

    property var rows: []
    property var partyList: []
    property string partyBalanceText: ""
    property bool partyBalanceVisible: false

    property var editingRow: null

    function refresh() {
        console.log("[ENTRY] refresh() called - page.width:", page.width, "page.height:", page.height)
        var result = backend.transactions(1, 100, 0)
        console.log("[ENTRY] backend.transactions returned", result ? result.length : "null", "rows")
        rows = result || []
    }
    function refreshParties() {
        console.log("[ENTRY] refreshParties() called")
        var result = backend.partyNames()
        console.log("[ENTRY] backend.partyNames returned", result ? result.length : "null", "names")
        partyList = result || []
    }
    function lookupPartyBalance(name) {
        console.log("[ENTRY] lookupPartyBalance name:", name)
        if (!name || name.trim().length === 0) {
            partyBalanceVisible = false
            console.log("[ENTRY] lookupPartyBalance: empty name, hiding")
            return
        }
        var info = backend.partyBalance(name.trim())
        console.log("[ENTRY] backend.partyBalance result:", JSON.stringify(info))
        if (info && info.hasData) {
            partyBalanceText = name.trim() + ": " + info.balanceLabel +
                " | Last: " + info.lastType + " \u20B9" + backend.formatMoney(info.lastAmount) +
                " on " + backend.formatDate(info.lastDate)
            partyBalanceVisible = true
        } else {
            partyBalanceVisible = false
        }
    }

    // FIX: defer forceActiveFocus so layout geometry is settled before we call it
    Component.onCompleted: {
        console.log("[ENTRY] Component.onCompleted - page.width:", page.width, "page.height:", page.height,
                    "parent:", parent, "parent.width:", parent ? parent.width : 0,
                    "parent.height:", parent ? parent.height : 0)
        refresh()
        refreshParties()
        Qt.callLater(function() {
            console.log("[ENTRY] deferred focus - billField.inputField:", billField ? billField.inputField : null)
            if (billField && billField.inputField) {
                billField.inputField.forceActiveFocus()
                console.log("[ENTRY] focus set to billField")
            } else {
                console.log("[ENTRY] WARNING: billField.inputField not available even after callLater")
            }
        })
        console.log("[ENTRY] Component.onCompleted done")
    }

    Connections {
        target: backend
        function onDataChanged() {
            console.log("[ENTRY] backend.dataChanged received")
            page.refresh()
        }
    }

    // ── Edit Transaction Dialog ────────────────────────────────────────────
    Dialog {
        id: editDialog
        modal: true
        closePolicy: Dialog.CloseOnEscape
        title: "Edit Transaction"
        // FIX: guard against page.height=0 during early layout phase
        width: Math.min(520, page.width > 0 ? page.width * 0.9 : 480)
        height: Math.min(600, page.height > 0 ? page.height * 0.92 : 560)
        padding: 0
        onOpened: console.log("[ENTRY] editDialog opened, targetRow:", targetRow ? targetRow.id : "null")
        onClosed: console.log("[ENTRY] editDialog closed")

        property var originalValues: ({})
        property var targetRow: null

        function loadRow(row) {
            if (!row) return
            targetRow = row
            var parts = row.date.split('-')
            var d
            if (parts[0].length === 4) {
                d = new Date(parseInt(parts[0]), parseInt(parts[1]) - 1, parseInt(parts[2]))
            } else {
                d = new Date(parseInt(parts[2]), parseInt(parts[1]) - 1, parseInt(parts[0]))
            }
            originalValues = {
                date: Qt.formatDate(d, "yyyy-MM-dd"),
                bill_no: row.bill_no || "",
                party: row.party || "",
                type: row.type || "Sale",
                mode: row.mode || "Credit",
                amount: String(row.amount || "0")
            }
            editDateField.selectedDate = d
            editBillField.text = originalValues.bill_no
            editPartyField.text = originalValues.party
            editTypeField.currentIndex = Math.max(0, editTypeField.options.indexOf(row.type))
            editModeField.currentIndex = Math.max(0, editModeField.options.indexOf(row.mode))
            editAmountField.text = originalValues.amount
            title = "Edit Transaction #" + row.id
        }

        background: GlassPanel {
            fillColor: Theme.bg2
            radius: Theme.rLg
        }

        contentItem: ColumnLayout {
            spacing: 0

            Item { Layout.preferredHeight: Theme.s5 }

            Text {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.s5
                Layout.rightMargin: Theme.s5
                text: editDialog.title
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsSection
                font.weight: Font.Bold
            }

            Item { Layout.preferredHeight: Theme.s3 }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.s5
                Layout.rightMargin: Theme.s5
                height: 1
                color: Theme.glassBorder
            }

            Item { Layout.preferredHeight: Theme.s5 }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: Theme.s5
                Layout.rightMargin: Theme.s5
                clip: true
                contentWidth: width
                contentHeight: fieldCol.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: fieldCol
                    width: parent.width
                    spacing: Theme.s4

                    DatePickerField {
                        id: editDateField
                        Layout.fillWidth: true
                        label: "Date"
                    }

                    FieldInput {
                        id: editBillField
                        Layout.fillWidth: true
                        label: "Bill / Ref"
                        placeholder: "INV-001"
                        showCompletions: false
                    }

                    FieldInput {
                        id: editPartyField
                        Layout.fillWidth: true
                        label: "Party"
                        placeholder: "Search or type party"
                        completions: page.partyList
                    }

                    FieldCombo {
                        id: editTypeField
                        Layout.fillWidth: true
                        label: "Type"
                        options: ["Sale", "Sale Return", "Expense", "Receipt", "Purchase"]
                    }

                    FieldCombo {
                        id: editModeField
                        Layout.fillWidth: true
                        label: "Mode"
                        options: ["Credit", "Cash", "UPI", "Bank"]
                    }

                    FieldInput {
                        id: editAmountField
                        Layout.fillWidth: true
                        label: "Amount"
                        placeholder: "0.00"
                        numeric: true
                        showCompletions: false
                    }
                }
            }

            Item { Layout.preferredHeight: Theme.s4 }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.s5
                Layout.rightMargin: Theme.s5
                height: 1
                color: Theme.glassBorder
            }

            Item { Layout.preferredHeight: Theme.s4 }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.s5
                Layout.rightMargin: Theme.s5
                Layout.bottomMargin: Theme.s5
                spacing: Theme.s3

                Item { Layout.fillWidth: true }

                GhostButton {
                    text: "Cancel"
                    onClicked: {
                        page.editingRow = null
                        editDialog.close()
                    }
                }

                PrimaryButton {
                    text: "Save Changes"
                    onClicked: {
                        if (!editDialog.targetRow) return
                        var row = editDialog.targetRow
                        var res = backend.editTransaction(
                            row.id,
                            editDateField.isoText,
                            editBillField.text,
                            editPartyField.text,
                            editTypeField.currentText,
                            editModeField.currentText,
                            parseFloat(editAmountField.text)
                        )
                        if (res && res.ok === true) {
                            page.refresh()
                        }
                        page.editingRow = null
                        editDialog.close()
                    }
                }
            }
        }
    }

    // ── Delete Confirmation Dialog ─────────────────────────────────────────
    Dialog {
        id: deleteDialog
        modal: true
        closePolicy: Dialog.CloseOnEscape
        title: "Delete Transaction"
        // FIX: guard against page.height=0 during early layout phase
        width: Math.min(440, page.width > 0 ? page.width * 0.85 : 420)
        height: Math.min(260, page.height > 0 ? page.height * 0.5 : 240)
        padding: 0
        onOpened: console.log("[ENTRY] deleteDialog opened, batchMode:", batchMode)
        onClosed: console.log("[ENTRY] deleteDialog closed")

        property string infoText: "Transaction details will be permanently removed."
        property bool batchMode: false
        // FIX: capture the count at open-time to avoid stale binding after deselection
        property int batchCount: 0

        background: GlassPanel {
            fillColor: Theme.bg2
            radius: Theme.rLg
        }

        contentItem: ColumnLayout {
            spacing: 0

            Item { Layout.preferredHeight: Theme.s5 }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.s5
                Layout.rightMargin: Theme.s5
                spacing: Theme.s3

                Text {
                    text: "\u26A0"
                    color: Theme.danger
                    font.pixelSize: 32
                    Layout.alignment: Qt.AlignTop
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s2

                    Text {
                        Layout.fillWidth: true
                        text: deleteDialog.batchMode
                            ? "Are you sure you want to delete " + deleteDialog.batchCount + " selected transactions?"
                            : "Are you sure you want to delete this transaction?"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsBody
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: deleteDialog.infoText
                        color: Theme.textDim
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSmall
                        wrapMode: Text.WordWrap
                        visible: text.length > 0
                    }
                }
            }

            Item { Layout.fillHeight: true }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.glassBorder
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.s5
                Layout.rightMargin: Theme.s5
                Layout.topMargin: Theme.s4
                Layout.bottomMargin: Theme.s5
                spacing: Theme.s3

                Item { Layout.fillWidth: true }

                GhostButton {
                    text: "Cancel"
                    onClicked: {
                        page.editingRow = null
                        deleteDialog.close()
                    }
                }

                PrimaryButton {
                    text: "Delete"
                    danger: true
                    onClicked: {
                        if (deleteDialog.batchMode) {
                            var ids = transactionTable.checkedRows.slice()
                            var res = backend.batchDeleteTransactions(ids)
                            if (res && res.ok === true) {
                                transactionTable.deselectAll()
                                page.refresh()
                                undoBtn.visible = true
                            }
                        } else if (page.editingRow) {
                            var res2 = backend.deleteTransaction(page.editingRow.id)
                            if (res2 && res2.ok === true) {
                                page.refresh()
                                undoBtn.visible = true
                            }
                        }
                        page.editingRow = null
                        deleteDialog.close()
                    }
                }
            }
        }
    }

    // ── Main layout ────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s8
        anchors.rightMargin: Theme.s8
        anchors.topMargin: Theme.s5
        anchors.bottomMargin: Theme.s6
        spacing: Theme.s5

        SectionHeader {
            title: "Daily Entry"
            subtitle: "Date \u2192 Bill \u2192 Party \u2192 Type \u2192 Mode \u2192 Amount \u2192 Save (Enter to advance)"
        }

        // Keyboard hints bar
        GlassPanel {
            Layout.fillWidth: true
            implicitHeight: 36
            radius: Theme.rSm
            fillColor: Theme.alpha(Theme.accent, 0.06)
            borderColor: Theme.alpha(Theme.accent, 0.15)
            elevated: false
            sheen: false
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.s4
                anchors.rightMargin: Theme.s4
                spacing: Theme.s4
                Text { text: "\u2328"; color: Theme.accent; font.pixelSize: 14 }
                Text { text: "Enter = Next field"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                Text { text: "\u2022"; color: Theme.textFaint; font.pixelSize: 8 }
                Text { text: "Amount + Enter = Save"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                Text { text: "\u2022"; color: Theme.textFaint; font.pixelSize: 8 }
                Text { text: "Empty party = 'customer'"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                Text { text: "\u2022"; color: Theme.textFaint; font.pixelSize: 8 }
                Text { text: "Alt+D = Dashboard"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                Item { Layout.fillWidth: true }
            }
        }

        // ── Entry form panel ───────────────────────────────────────────────
        GlassPanel {
            Layout.fillWidth: true
            implicitHeight: formCol.implicitHeight + Theme.s8
            radius: Theme.rLg

            ColumnLayout {
                id: formCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.s5
                spacing: Theme.s4

                // Party balance info (visible when a known party is typed)
                Rectangle {
                    Layout.fillWidth: true
                    height: 32
                    radius: Theme.rSm
                    visible: page.partyBalanceVisible
                    color: Theme.alpha(Theme.accent3, 0.08)
                    border.width: 1
                    border.color: Theme.alpha(Theme.accent3, 0.2)
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.s3
                        anchors.rightMargin: Theme.s3
                        spacing: Theme.s2
                        Text { text: "\u2139"; color: Theme.accent3; font.pixelSize: 13 }
                        Text {
                            text: page.partyBalanceText
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsTiny
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                // FIX: Row 1 — Date | Bill | Party  (no overflow; Type/Mode/Amount on row 2)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s3

                    DatePickerField {
                        id: dateField
                        Layout.preferredWidth: 170
                        label: "Date"
                        // FIX: dateField now participates in Enter-chain via accepted signal
                        onAccepted: billField.inputField.forceActiveFocus()
                    }
                    FieldInput {
                        id: billField
                        Layout.preferredWidth: 170
                        label: "Ref / Bill"
                        placeholder: "INV-001"
                        showCompletions: false
                        onAccepted: partyField.inputField.forceActiveFocus()
                    }
                    FieldInput {
                        id: partyField
                        Layout.fillWidth: true
                        label: "Party"
                        placeholder: "Search or type party"
                        completions: page.partyList
                        onAccepted: typeField.focusCombo()
                        inputField.onEditingFinished: page.lookupPartyBalance(text)
                    }
                }

                // FIX: Row 2 — Type | Mode | Amount | Save  (all fit comfortably)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s3

                    FieldCombo {
                        id: typeField
                        Layout.fillWidth: true
                        label: "Type"
                        options: ["Sale", "Sale Return", "Expense", "Receipt", "Purchase"]
                        onNextField: modeField.focusCombo()
                    }
                    FieldCombo {
                        id: modeField
                        Layout.fillWidth: true
                        label: "Mode"
                        options: ["Credit", "Cash", "UPI", "Bank"]
                        onNextField: amountField.inputField.forceActiveFocus()
                    }
                    FieldInput {
                        id: amountField
                        Layout.preferredWidth: 160
                        label: "Amount"
                        placeholder: "0.00"
                        numeric: true
                        showCompletions: false
                        onAccepted: page.save()
                    }
                    PrimaryButton {
                        Layout.alignment: Qt.AlignBottom
                        Layout.preferredWidth: 120
                        text: "Save"
                        onClicked: page.save()
                    }
                }
            }
        }

        // ── Recent transactions table ──────────────────────────────────────
        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rLg

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.s5
                spacing: Theme.s3

                Item {
                    Layout.fillWidth: true
                    implicitHeight: toolRow.implicitHeight

                    RowLayout {
                        id: toolRow
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: Theme.s3

                        Text {
                            text: "Recent Transactions"
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsSection
                            font.weight: Font.Bold
                        }
                        Item { Layout.fillWidth: true }
                        GhostButton {
                            text: "Import"
                            tint: Theme.accent
                            implicitWidth: 80
                            onClicked: {
                                var res = backend.importTransactions()
                                if (res && res.ok === true) {
                                    page.refresh()
                                }
                            }
                        }
                        GhostButton {
                            text: "Template"
                            tint: Theme.accent
                            implicitWidth: 90
                            onClicked: backend.downloadImportTemplate()
                        }
                        Rectangle {
                            id: smartBtn
                            Layout.preferredWidth: 28; Layout.preferredHeight: 28
                            radius: 14
                            color: smartHover.hovered ? Theme.alpha(Theme.accent2, 0.2) : "transparent"
                            border.width: 1
                            border.color: smartHover.hovered ? Theme.alpha(Theme.accent2, 0.5) : "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "\u2699"
                                color: Theme.accent2
                                font.pixelSize: 14
                            }
                            HoverHandler { id: smartHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                onTapped: {
                                    var res = backend.smartImportExcel()
                                    if (res && res.ok === true) {
                                        page.refresh()
                                    }
                                }
                            }
                            Rectangle {
                                visible: smartHover.hovered
                                x: parent.width + 8; y: -4
                                width: tipText.implicitWidth + 16
                                height: tipText.implicitHeight + 8
                                radius: Theme.rSm
                                color: Theme.bg2
                                border.width: 1
                                border.color: Theme.glassBorder
                                z: 1000
                                Text {
                                    id: tipText
                                    anchors.centerIn: parent
                                    text: "Smart Import"
                                    color: Theme.textDim
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fsTiny
                                }
                            }
                        }
                        GhostButton {
                            text: "Export PDF"
                            tint: Theme.accent2
                            implicitWidth: 100
                            onClicked: backend.exportRecentPdf(7)
                        }
                        GhostButton {
                            text: "Export Excel"
                            tint: Theme.accent3
                            implicitWidth: 100
                            onClicked: backend.exportRecentExcel(30)
                        }
                        GhostButton {
                            id: undoBtn
                            text: "\u21A9 Undo"
                            tint: Theme.warning
                            implicitWidth: 90
                            visible: false
                            onClicked: {
                                var res = backend.undoDeleteTransaction()
                                if (res && res.ok === true) {
                                    page.refresh()
                                    undoBtn.visible = false
                                }
                            }
                        }
                        StatusPill { text: page.rows.length + " entries"; tint: Theme.accent }
                    }
                    HoverHandler { id: toolHover; cursorShape: Qt.ArrowCursor }
                }

                // Batch selection action bar (appears when rows are checked)
                RowLayout {
                    Layout.fillWidth: true
                    visible: transactionTable.checkedRows.length > 0

                    Rectangle {
                        Layout.preferredHeight: 32
                        Layout.preferredWidth: batchDeleteText.implicitWidth + 32
                        radius: Theme.rPill
                        color: Theme.alpha(Theme.danger, 0.12)
                        border.width: 1
                        border.color: Theme.alpha(Theme.danger, 0.3)

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: Theme.s1
                            Text { text: "\u2716"; color: Theme.danger; font.pixelSize: 12 }
                            Text {
                                id: batchDeleteText
                                text: "Delete " + transactionTable.checkedRows.length + " selected"
                                color: Theme.danger
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fsTiny
                                font.weight: Font.DemiBold
                            }
                        }
                        TapHandler {
                            onTapped: {
                                // FIX: capture count at open-time; checkedRows.length binding
                                // was unreliable due to earlier array reactivity bug (now fixed)
                                deleteDialog.batchCount = transactionTable.checkedRows.length
                                deleteDialog.batchMode = true
                                deleteDialog.infoText = "Batch delete " + deleteDialog.batchCount + " transactions"
                                deleteDialog.open()
                            }
                        }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }

                    Rectangle {
                        Layout.preferredHeight: 32
                        Layout.preferredWidth: 110
                        radius: Theme.rPill
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.glassBorder

                        Text {
                            anchors.centerIn: parent
                            text: "Clear selection"
                            color: Theme.textDim
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsTiny
                        }
                        TapHandler {
                            onTapped: transactionTable.deselectAll()
                        }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }

                    Item { Layout.fillWidth: true }
                }

                DataTable {
                    id: transactionTable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    checkable: true
                    emptyText: "No transactions yet \u2014 add your first entry above"
                    loading: false
                    rows: page.rows
                    columns: [
                        { title: "Date", key: "date", date: true, weight: 1.1 },
                        { title: "Bill No", key: "bill_no", weight: 1 },
                        { title: "Party", key: "party", weight: 2 },
                        { title: "Type", key: "type", chip: true, weight: 1.2 },
                        { title: "Mode", key: "mode", weight: 1 },
                        { title: "Amount", key: "amount", money: true, align: "right", weight: 1.2 }
                    ]
                    onRowActivated: function(row) {
                        // FIX: position context menu near the row's right edge (not always top-right)
                        var globalPt = transactionTable.mapToGlobal(
                            transactionTable.width - contextMenu.width - Theme.s3,
                            transactionTable.height / 2   // reasonable vertical fallback
                        )
                        contextMenu.popupAt(row, globalPt.x, globalPt.y)
                    }
                }
            }
        }
    }

    // ── Context menu for edit / delete actions ────────────────────────────
    Rectangle {
        id: contextMenu
        visible: false
        width: 160
        height: cmCol.implicitHeight + 8
        radius: Theme.rMd
        color: Theme.glassStrong
        border.width: 1
        border.color: Theme.glassBorder
        z: 999

        property var targetRow: null

        // FIX: accept global coordinates and map to page-local, with bounds clamping
        function popupAt(row, globalX, globalY) {
            targetRow = row
            var localPos = page.mapFromGlobal(globalX, globalY)
            x = Math.max(Theme.s4, Math.min(localPos.x, page.width  - width  - Theme.s4))
            y = Math.max(Theme.s4, Math.min(localPos.y, page.height - height - Theme.s4))
            visible = true
            forceActiveFocus()
        }

        ColumnLayout {
            id: cmCol
            anchors.fill: parent
            anchors.margins: 4
            spacing: 2

            // Edit action
            Rectangle {
                Layout.fillWidth: true
                height: 36
                radius: Theme.rSm
                color: mEdit.hovered ? Theme.rowHover : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.s3
                    spacing: Theme.s2
                    Text { text: "\u270E"; color: Theme.accent; font.pixelSize: 14 }
                    Text { text: "Edit"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                    Item { Layout.fillWidth: true }
                }

                HoverHandler { id: mEdit }

                TapHandler {
                    onTapped: {
                        contextMenu.visible = false
                        page.openEditDialog(contextMenu.targetRow)
                    }
                }
            }

            // Delete action
            Rectangle {
                Layout.fillWidth: true
                height: 36
                radius: Theme.rSm
                color: mDel.hovered ? Theme.rowHover : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.s3
                    spacing: Theme.s2
                    Text { text: "\u2716"; color: Theme.danger; font.pixelSize: 14 }
                    Text { text: "Delete"; color: Theme.danger; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                    Item { Layout.fillWidth: true }
                }

                HoverHandler { id: mDel }

                TapHandler {
                    onTapped: {
                        contextMenu.visible = false
                        page.openDeleteDialog(contextMenu.targetRow)
                    }
                }
            }
        }

        TapHandler {
            onTapped: contextMenu.visible = false
        }

        Keys.onEscapePressed: contextMenu.visible = false
        focus: true
    }

    // Click outside context menu to close it
    TapHandler {
        enabled: contextMenu.visible
        onTapped: contextMenu.visible = false
    }

    // ── Dialog helper functions ────────────────────────────────────────────
    function openEditDialog(row) {
        if (!row) {
            console.log("[ENTRY] openEditDialog called with null row")
            return
        }
        console.log("[ENTRY] openEditDialog for row id:", row.id, "party:", row.party)
        editingRow = row
        editDialog.loadRow(row)
        editDialog.open()
    }

    function openDeleteDialog(row) {
        if (!row) {
            console.log("[ENTRY] openDeleteDialog called with null row")
            return
        }
        console.log("[ENTRY] openDeleteDialog for row id:", row.id, "party:", row.party)
        editingRow = row
        deleteDialog.batchMode = false
        deleteDialog.batchCount = 0
        deleteDialog.infoText = "Transaction #" + row.id + " (" + row.party + ", \u20B9" + Number(row.amount).toFixed(2) + ")"
        deleteDialog.open()
    }

    // ── Save entry ─────────────────────────────────────────────────────────
    function save() {
        console.log("[ENTRY] save() called date:", dateField.isoText,
                    "bill:", billField.text, "party:", partyField.text,
                    "type:", typeField.currentText, "mode:", modeField.currentText,
                    "amount:", Number(amountField.text))
        var res = backend.addTransaction(
            dateField.isoText,
            billField.text,
            partyField.text,
            typeField.currentText,
            modeField.currentText,
            Number(amountField.text)
        )
        console.log("[ENTRY] backend.addTransaction result:", JSON.stringify(res))
        if (res && res.ok === true) {
            // Auto-increment bill, clear inputs, cycle focus back to Bill
            billField.text = backend.nextBillNumber(billField.text)
            partyField.text = ""
            amountField.text = ""
            partyBalanceVisible = false
            refreshParties()
            page.refresh()
            billField.inputField.forceActiveFocus()
        } else {
            console.log("[ENTRY] save failed:", res ? res.error : "no response")
        }
    }
}
