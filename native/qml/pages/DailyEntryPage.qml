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
        rows = backend.transactions(1, 100, 0)
    }
    function refreshParties() {
        partyList = backend.partyNames()
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

    Component.onCompleted: {
        refresh()
        refreshParties()
        billField.inputField.forceActiveFocus()
    }
    Connections {
        target: backend
        function onDataChanged() { page.refresh() }
    }

    // Edit Transaction Dialog
    Dialog {
        id: editDialog
        modal: true
        closePolicy: Dialog.CloseOnEscape
        title: "Edit Transaction"
        width: Math.min(520, page.width * 0.9)
        height: Math.min(600, page.height * 0.95)
        padding: 0

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

    // Delete Confirmation Dialog
    Dialog {
        id: deleteDialog
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        title: "Delete Transaction"
        closePolicy: Dialog.CloseOnEscape
        width: Math.min(440, page.width * 0.85)
        height: Math.min(220, page.height * 0.5)

        property string infoText: "Transaction details will be permanently removed."

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.s4
            spacing: Theme.s3

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.s3

                Text {
                    text: "\u26A0"
                    color: "#e74c3c"
                    font.pixelSize: 32
                    Layout.alignment: Qt.AlignTop
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s2

                    Text {
                        Layout.fillWidth: true
                        text: "Are you sure you want to delete this transaction? This action cannot be undone."
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
        }

        onAccepted: {
            if (page.editingRow) {
                var res = backend.deleteTransaction(page.editingRow.id)
                if (res && res.ok === true) {
                    page.refresh()
                }
            }
            page.editingRow = null
        }
        onRejected: {
            page.editingRow = null
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
            title: "Daily Entry"
            subtitle: "Press Enter to navigate: Bill \u2192 Party \u2192 Type \u2192 Mode \u2192 Amount \u2192 Save \u2192 Bill"
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
                spacing: Theme.s5
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

        // Entry form panel
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

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s3

                    DatePickerField {
                        id: dateField
                        Layout.preferredWidth: 170
                        label: "Date"
                    }
                    FieldInput {
                        id: billField
                        Layout.preferredWidth: 150
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
                        // Lookup balance when user finishes typing
                        inputField.onEditingFinished: page.lookupPartyBalance(text)
                    }
                    FieldCombo {
                        id: typeField
                        Layout.preferredWidth: 150
                        label: "Type"
                        options: ["Sale", "Sale Return", "Expense", "Receipt", "Purchase"]
                        onNextField: modeField.focusCombo()
                    }
                    FieldCombo {
                        id: modeField
                        Layout.preferredWidth: 140
                        label: "Mode"
                        options: ["Credit", "Cash", "UPI", "Bank"]
                        onNextField: amountField.inputField.forceActiveFocus()
                    }
                    FieldInput {
                        id: amountField
                        Layout.preferredWidth: 150
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

        // Recent transactions table
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
                            text: "Export 7-Day PDF"
                            tint: Theme.accent2
                            implicitWidth: 140
                            onClicked: backend.exportRecentPdf(7)
                        }
                        GhostButton {
                            text: "Export Excel"
                            tint: Theme.accent3
                            implicitWidth: 120
                            onClicked: backend.exportRecentExcel(30)
                        }
                        StatusPill { text: page.rows.length + " entries"; tint: Theme.accent }
                    }

                DataTable {
                    id: transactionTable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    emptyText: "No transactions yet \u2014 add your first entry above"
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
                        contextMenu.popup(row)
                    }
                }
            }
        }
    }

    // Context menu for edit/delete actions
    Rectangle {
        id: contextMenu
        visible: false
        width: 160
        height: col.implicitHeight + 8
        radius: Theme.rMd
        color: Theme.glassStrong
        border.width: 1
        border.color: Theme.glassBorder
        z: 999

        property var targetRow: null

        function popup(row) {
            targetRow = row
            var pos = transactionTable.mapToItem(page, transactionTable.width - width, 0)
            x = Math.max(0, page.width - width - Theme.s4)
            y = Math.min(page.height - height - Theme.s4, Theme.s5)
            visible = true
            forceActiveFocus()
        }

        ColumnLayout {
            id: col
            anchors.fill: parent
            anchors.margins: 4
            spacing: 2

            // Edit action
            Rectangle {
                Layout.fillWidth: true
                height: 36
                radius: Theme.rSm
                color: mEdit.containsMouse ? Theme.rowHover : "transparent"

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
                color: mDel.containsMouse ? Theme.rowHover : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.s3
                    spacing: Theme.s2
                    Text { text: "\u2716"; color: "#e74c3c"; font.pixelSize: 14 }
                    Text { text: "Delete"; color: "#e74c3c"; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
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

    // Click outside to close context menu
    TapHandler {
        enabled: contextMenu.visible
        onTapped: contextMenu.visible = false
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
            refreshParties()
            billField.inputField.forceActiveFocus()
        }
    }
}
