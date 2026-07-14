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
    property bool isLoading: false
    property string errorMessage: ""

    property var editingRow: null

    function refresh() {
        page.isLoading = true
        backend.fetchTransactions(1, 100, 0)
    }
    function refreshParties() {
        var result = backend.partyNames()
        partyList = result || []
    }
    function lookupPartyBalance(name) {
        if (!name || name.trim().length === 0) {
            partyBalanceVisible = false
            return
        }
        var info = backend.partyBalance(name.trim())
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
        refresh()
        refreshParties()
        Qt.callLater(function() {
            if (billField && billField.inputField) {
                billField.inputField.forceActiveFocus()
            }
        })
    }

    Connections {
        target: backend
        function onDataChanged() {
            page.refresh()
        }
        function onTransactionsLoaded(result) {
            page.isLoading = false
            if (result && result.error) {
                page.errorMessage = result.error
                return
            }
            page.errorMessage = ""
            page.rows = result || []
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
        onOpened: editDateField.forceActiveFocus()
        onClosed: {
            page.editingRow = null
            if (transactionTable) transactionTable.forceActiveFocus()
        }

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
            fillColor: Theme.palette.bgMuted
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
                color: Theme.palette.fg
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
                color: Theme.palette.border
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
                color: Theme.palette.border
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

        property string infoText: "Transaction details will be permanently removed."
        property bool batchMode: false
        // FIX: capture the count at open-time to avoid stale binding after deselection
        property int batchCount: 0

        background: GlassPanel {
            fillColor: Theme.palette.bgMuted
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
                    font.pixelSize: Theme.fsHero
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
                        color: Theme.palette.fg
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsBody
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: deleteDialog.infoText
                        color: Theme.palette.fgMuted
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
                color: Theme.palette.border
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
            fillColor: Theme.alpha(Theme.palette.primary, 0.06)
            borderColor: Theme.alpha(Theme.palette.primary, 0.15)
            elevated: false
            sheen: false
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.s4
                anchors.rightMargin: Theme.s4
                spacing: Theme.s4
                Text { text: "\u2328"; color: Theme.palette.primary; font.pixelSize: Theme.fsBody }
                Text { text: "Enter = Next field"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                Text { text: "\u2022"; color: Theme.palette.fgSubtle; font.pixelSize: Theme.s2 }
                Text { text: "Amount + Enter = Save"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                Text { text: "\u2022"; color: Theme.palette.fgSubtle; font.pixelSize: Theme.s2 }
                Text { text: "Empty party = 'customer'"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                Text { text: "\u2022"; color: Theme.palette.fgSubtle; font.pixelSize: Theme.s2 }
                Text { text: "Alt+D = Dashboard"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                Item { Layout.fillWidth: true }
            }
        }

        ErrorBanner {
            Layout.fillWidth: true
            message: page.errorMessage
            onRetry: page.refresh()
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
                    color: Theme.alpha(Theme.palette.info, 0.08)
                    border.width: 1
                    border.color: Theme.alpha(Theme.palette.info, 0.2)
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.s3
                        anchors.rightMargin: Theme.s3
                        spacing: Theme.s2
                        Text { text: "\u2139"; color: Theme.palette.info; font.pixelSize: 13 }
                        Text {
                            text: page.partyBalanceText
                            color: Theme.palette.fg
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
                            color: Theme.palette.fg
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsSection
                            font.weight: Font.Bold
                        }
                        Item { Layout.fillWidth: true }
                        GhostButton {
                            text: "Import"
                            tint: Theme.palette.primary
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
                            tint: Theme.palette.primary
                            implicitWidth: 90
                            onClicked: backend.downloadImportTemplate()
                        }
                        Rectangle {
                            id: smartBtn
                            activeFocusOnTab: true
                            Accessible.role: Accessible.Button
                            Accessible.name: "Smart Import"
                            Keys.onReturnPressed: { var res = backend.smartImportExcel(); if (res && res.ok === true) { page.refresh() } }
                            Keys.onSpacePressed: { var res = backend.smartImportExcel(); if (res && res.ok === true) { page.refresh() } }
                            Layout.preferredWidth: 28; Layout.preferredHeight: 28
                            radius: 14
                            color: smartHover.hovered ? Theme.alpha(Theme.palette.primary, 0.2) : "transparent"
                            border.width: 1
                            border.color: smartHover.hovered ? Theme.alpha(Theme.palette.primary, 0.5) : "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "\u2699"
                                color: Theme.palette.primary
                                font.pixelSize: Theme.fsBody
                            }
                            FocusRing { visible: parent.activeFocus }
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
                                color: Theme.palette.bgMuted
                                border.width: 1
                                border.color: Theme.palette.border
                                z: 1000
                                Text {
                                    id: tipText
                                    anchors.centerIn: parent
                                    text: "Smart Import"
                                    color: Theme.palette.fgMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fsTiny
                                }
                            }
                        }
                        GhostButton {
                            text: "Export PDF"
                            tint: Theme.palette.primary
                            implicitWidth: 100
                            onClicked: backend.exportRecentPdf(7)
                        }
                        GhostButton {
                            text: "Export Excel"
                            tint: Theme.palette.info
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
                        StatusPill { text: page.rows.length + " entries"; tint: Theme.palette.primary }
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
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: "Delete selected"
                        Keys.onReturnPressed: { deleteDialog.batchCount = transactionTable.checkedRows.length; deleteDialog.batchMode = true; deleteDialog.infoText = "Batch delete " + deleteDialog.batchCount + " transactions"; deleteDialog.open() }
                        Keys.onSpacePressed: { deleteDialog.batchCount = transactionTable.checkedRows.length; deleteDialog.batchMode = true; deleteDialog.infoText = "Batch delete " + deleteDialog.batchCount + " transactions"; deleteDialog.open() }

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

                        FocusRing { visible: parent.activeFocus }

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
                        border.color: Theme.palette.border
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: "Clear selection"
                        Keys.onReturnPressed: transactionTable.deselectAll()
                        Keys.onSpacePressed: transactionTable.deselectAll()

                        Text {
                            anchors.centerIn: parent
                            text: "Clear selection"
                            color: Theme.palette.fgMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsTiny
                        }

                        FocusRing { visible: parent.activeFocus }

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
                    emptyText: "No transactions yet - add your first entry above"
                    loading: page.isLoading
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
    Popup {
        id: contextMenu
        width: 160
        padding: 4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent | Popup.CloseOnPressOutside
        modal: false
        z: 999

        property var targetRow: null

        // FIX: accept global coordinates and map to page-local, with bounds clamping
        function popupAt(row, globalX, globalY) {
            targetRow = row
            var localPos = page.mapFromGlobal(globalX, globalY)
            x = Math.max(Theme.s4, Math.min(localPos.x, page.width  - width  - Theme.s4))
            y = Math.max(Theme.s4, Math.min(localPos.y, page.height - height - Theme.s4))
            open()
        }

        onOpened: mEditItem.forceActiveFocus()

        background: Rectangle {
            radius: Theme.rMd
            color: Theme.palette.popover
            border.width: 1
            border.color: Theme.palette.border
        }

        contentItem: ColumnLayout {
            id: cmCol
            spacing: 2

            // Edit action
            Rectangle {
                id: mEditItem
                Layout.fillWidth: true
                height: 36
                radius: Theme.rSm
                color: mEdit.hovered ? Theme.alpha(Theme.palette.fg, 0.05) : "transparent"
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: "Edit transaction"
                Keys.onReturnPressed: { contextMenu.close(); page.openEditDialog(contextMenu.targetRow) }
                Keys.onSpacePressed: { contextMenu.close(); page.openEditDialog(contextMenu.targetRow) }
                Keys.onDownPressed: mDelItem.forceActiveFocus()
                Keys.onUpPressed: mDelItem.forceActiveFocus()
                Keys.onEscapePressed: contextMenu.close()

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.s3
                    spacing: Theme.s2
                    Text { text: "\u270E"; color: Theme.palette.primary; font.pixelSize: Theme.fsBody }
                    Text { text: "Edit"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                    Item { Layout.fillWidth: true }
                }

                FocusRing { visible: parent.activeFocus }

                HoverHandler { id: mEdit }

                TapHandler {
                    onTapped: {
                        contextMenu.close()
                        page.openEditDialog(contextMenu.targetRow)
                    }
                }
            }

            // Delete action
            Rectangle {
                id: mDelItem
                Layout.fillWidth: true
                height: 36
                radius: Theme.rSm
                color: mDel.hovered ? Theme.alpha(Theme.palette.fg, 0.05) : "transparent"
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: "Delete transaction"
                Keys.onReturnPressed: { contextMenu.close(); page.openDeleteDialog(contextMenu.targetRow) }
                Keys.onSpacePressed: { contextMenu.close(); page.openDeleteDialog(contextMenu.targetRow) }
                Keys.onDownPressed: mEditItem.forceActiveFocus()
                Keys.onUpPressed: mEditItem.forceActiveFocus()
                Keys.onEscapePressed: contextMenu.close()

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.s3
                    spacing: Theme.s2
                    Text { text: "\u2716"; color: Theme.danger; font.pixelSize: Theme.fsBody }
                    Text { text: "Delete"; color: Theme.danger; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                    Item { Layout.fillWidth: true }
                }

                FocusRing { visible: parent.activeFocus }

                HoverHandler { id: mDel }

                TapHandler {
                    onTapped: {
                        contextMenu.close()
                        page.openDeleteDialog(contextMenu.targetRow)
                    }
                }
            }
        }
    }

    // ── Dialog helper functions ────────────────────────────────────────────
    function openEditDialog(row) {
        if (!row) {
            return
        }
        editingRow = row
        editDialog.loadRow(row)
        editDialog.open()
    }

    function openDeleteDialog(row) {
        if (!row) {
            return
        }
        editingRow = row
        deleteDialog.batchMode = false
        deleteDialog.batchCount = 0
        deleteDialog.infoText = "Transaction #" + row.id + " (" + row.party + ", \u20B9" + Number(row.amount).toFixed(2) + ")"
        deleteDialog.open()
    }

    // ── Save entry ─────────────────────────────────────────────────────────
    function save() {
        var res = backend.addTransaction(
            dateField.isoText,
            billField.text,
            partyField.text,
            typeField.currentText,
            modeField.currentText,
            Number(amountField.text)
        )
        if (res && res.ok === true) {
            // Auto-increment bill, clear inputs, cycle focus back to Bill
            billField.text = backend.nextBillNumber(billField.text)
            partyField.text = ""
            amountField.text = ""
            partyBalanceVisible = false
            refreshParties()
            page.refresh()
            billField.inputField.forceActiveFocus()
        }
    }
}
