import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: page
    property var rawRows: []
    property var rows: []
    property real openingBalance: 0
    property var partyList: []
    property var totals: ({})
    property var editingRow: null
    property bool isLoading: false
    property string errorMessage: ""

    Component.onCompleted: {
        partyList = backend.partyNames()
    }

    // Debit types: Sale, Expense, Purchase
    // Credit types: Receipt, Sale Return
    function isDebit(type) {
        var t = String(type).toLowerCase()
        return t === "sale" || t === "expense" || t === "purchase"
    }

    function transformRows(data) {
        var result = []
        var totalDr = 0, totalCr = 0
        for (var i = 0; i < data.length; i++) {
            var r = data[i]
            var amt = Number(r.amount || 0)
            var dr = isDebit(r.type) ? amt : 0
            var cr = isDebit(r.type) ? 0 : amt
            totalDr += dr
            totalCr += cr
            var bal = Number(r.balance || 0)
            var balLabel = bal >= 0
                ? backend.formatMoney(bal) + " Dr"
                : backend.formatMoney(Math.abs(bal)) + " Cr"
            result.push({
                id: r.id,
                date: r.date || "",
                bill_no: r.bill_no || "",
                type: r.type || "",
                mode: r.mode || "",
                debit: dr > 0 ? dr : null,
                credit: cr > 0 ? cr : null,
                balance_label: balLabel,
                amount: amt,
                party: partyField.text
            })
        }
        totals = { debit: totalDr, credit: totalCr }
        return result
    }

    Connections {
        target: backend
        function onLedgerLoaded(res) {
            page.isLoading = false
            if (res && res.error) {
                page.errorMessage = res.error
                return
            }
            page.errorMessage = ""
            openingBalance = Number(res.opening_balance || 0)
            rawRows = res.data || []
            rows = transformRows(rawRows)
        }
    }

    function load() {
        if (partyField.text.trim().length === 0) {
            backend.toast("Choose a party first", "error")
            return
        }
        page.isLoading = true
        backend.fetchLedger(partyField.text, dateRange.fromIso, dateRange.toIso)
    }

    // ── Edit Transaction Dialog ────────────────────────────────────────────
    Dialog {
        id: editDialog
        modal: true
        closePolicy: Dialog.CloseOnEscape
        title: "Edit Transaction"
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

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    width: 8
                    contentItem: Rectangle {
                        implicitWidth: 8
                        radius: 4
                        color: Theme.alpha(Theme.palette.primary, 0.45)
                    }
                    background: Rectangle {
                        implicitWidth: 8
                        radius: 4
                        color: Theme.alpha(Theme.alpha(Theme.palette.fg, 0.04), 0.2)
                    }
                }

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

                        // Validate inputs
                        if (editPartyField.text.trim().length === 0) {
                            backend.toast("Party name cannot be empty", "error")
                            return
                        }
                        var amt = parseFloat(editAmountField.text)
                        if (isNaN(amt) || amt <= 0) {
                            backend.toast("Amount must be greater than 0", "error")
                            return
                        }

                        var res = backend.editTransaction(
                            row.id,
                            editDateField.isoText,
                            editBillField.text,
                            editPartyField.text,
                            editTypeField.currentText,
                            editModeField.currentText,
                            amt
                        )
                        if (res && res.ok === true) {
                            page.load()
                            backend.toast("Transaction updated", "success")
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
        width: Math.min(440, page.width > 0 ? page.width * 0.85 : 420)
        height: Math.min(260, page.height > 0 ? page.height * 0.5 : 240)
        padding: 0

        property string infoText: "Transaction details will be permanently removed."

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
                        text: "Are you sure you want to delete this transaction?"
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
                        if (page.editingRow) {
                            var res = backend.deleteTransaction(page.editingRow.id)
                            if (res && res.ok === true) {
                                page.load()
                                backend.toast("Transaction deleted", "success")
                            }
                        }
                        page.editingRow = null
                        deleteDialog.close()
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s8
        anchors.rightMargin: Theme.s8
        anchors.topMargin: Theme.s5
        anchors.bottomMargin: Theme.s6
        spacing: Theme.s5

        SectionHeader {
            title: "Party Ledger"
            subtitle: "Statement of account \u2014 Debit (Dr) / Credit (Cr) with running balance"
        }

        GlassPanel {
            Layout.fillWidth: true
            implicitHeight: filterRow.implicitHeight + Theme.s8
            radius: Theme.rLg
            RowLayout {
                id: filterRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.s5
                spacing: Theme.s3
                FieldInput {
                    id: partyField
                    Layout.fillWidth: true
                    label: "Party"
                    placeholder: "Search party"
                    completions: page.partyList
                    onAccepted: page.load()
                }
                DateRangePicker {
                    id: dateRange
                    Layout.fillWidth: true
                    onRangeChanged: Qt.callLater(page.load)
                }
                PrimaryButton { Layout.alignment: Qt.AlignBottom; Layout.preferredWidth: 120; text: "Show"; onClicked: page.load() }
                GhostButton { Layout.alignment: Qt.AlignBottom; text: "Export"; onClicked: {} }
            }
        }

        ErrorBanner {
            Layout.fillWidth: true
            message: page.errorMessage
            onRetry: page.load()
        }

        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rLg
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.s5
                spacing: Theme.s3
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Statement"
                        color: Theme.palette.fg
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSection
                        font.weight: Font.Bold
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Opening Balance: " + backend.formatMoney(page.openingBalance) + (page.openingBalance >= 0 ? " Dr" : " Cr")
                        color: Theme.palette.fgMuted
                        font.family: Theme.monoFamily
                        font.pixelSize: Theme.fsSmall
                    }
                }
                DataTable {
                    id: transactionTable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    emptyText: "Select a party and press Show"
                    loading: page.isLoading
                    rows: page.rows
                    totals: page.totals
                    totalsLabel: "Total"
                    columns: [
                        { title: "Date", key: "date", date: true, weight: 1.1 },
                        { title: "Bill", key: "bill_no", weight: 0.9 },
                        { title: "Particulars", key: "type", chip: true, weight: 1.2 },
                        { title: "Mode", key: "mode", weight: 0.9 },
                        { title: "Debit (Dr)", key: "debit", money: true, align: "right", weight: 1.2 },
                        { title: "Credit (Cr)", key: "credit", money: true, align: "right", weight: 1.2 },
                        { title: "Balance", key: "balance_label", align: "right", weight: 1.4 }
                    ]
                    onRowActivated: function(row) {
                        var globalPt = transactionTable.mapToGlobal(
                            transactionTable.width - contextMenu.width - Theme.s3,
                            transactionTable.height / 2
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

    function openEditDialog(row) {
        if (!row) return
        editingRow = row
        editDialog.loadRow(row)
        editDialog.open()
    }

    function openDeleteDialog(row) {
        if (!row) return
        editingRow = row
        deleteDialog.infoText = "Transaction #" + row.id + " (" + row.party + ", \u20B9" + Number(row.amount).toFixed(2) + ")"
        deleteDialog.open()
    }
}
