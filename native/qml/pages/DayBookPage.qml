import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: page
    property var rows: []
    property var totals: ({})
    property var partyList: []
    property var editingRow: null

    function load() {
        console.log("[DAYBOOK] load() called date:", dateField ? dateField.isoText : "no dateField")
        var result = backend.dayBook(dateField.isoText) || []
        
        // Natural Sorting: sort date-wise, then bill-wise naturally
        result.sort(function(a, b) {
            if (a.date !== b.date) {
                return a.date.localeCompare(b.date);
            }
            var billA = String(a.bill_no || "");
            var billB = String(b.bill_no || "");
            return billA.localeCompare(billB, undefined, { numeric: true, sensitivity: 'base' });
        });
        
        rows = result
        console.log("[DAYBOOK] rows returned:", rows ? rows.length : "null")
        var sum = 0
        for (var i = 0; i < rows.length; i++) sum += Number(rows[i].amount || 0)
        totals = { amount: sum }
    }

    Component.onCompleted: {
        console.log("[DAYBOOK] Component.onCompleted")
        partyList = backend.partyNames()
        load()
    }

    function doExportPdf() {
        var cols = ["Bill", "Party", "Type", "Mode", "Amount"]
        var data = []
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i]
            data.push([String(r.bill_no || ""), String(r.party || ""), String(r.type || ""), String(r.mode || ""), String(r.amount || "")])
        }
        backend.exportTableToPdf("Day Book", cols, data)
    }
    
    function doExportCsv() {
        var cols = ["Bill", "Party", "Type", "Mode", "Amount"]
        var data = []
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i]
            data.push([String(r.bill_no || ""), String(r.party || ""), String(r.type || ""), String(r.mode || ""), String(r.amount || "")])
        }
        backend.exportTableToExcel("Day Book", cols, data)
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
        onOpened: console.log("[DAYBOOK] editDialog opened, targetRow:", targetRow ? targetRow.id : "null")
        onClosed: console.log("[DAYBOOK] editDialog closed")

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
        onOpened: console.log("[DAYBOOK] deleteDialog opened")
        onClosed: console.log("[DAYBOOK] deleteDialog closed")

        property string infoText: "Transaction details will be permanently removed."

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
                    color: "#e74c3c"
                    font.pixelSize: 32
                    Layout.alignment: Qt.AlignTop
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s2

                    Text {
                        Layout.fillWidth: true
                        text: "Are you sure you want to delete this transaction?"
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

        RowLayout {
            Layout.fillWidth: true
            SectionHeader {
                title: "Day Book"
                subtitle: "All transactions on a chosen day"
            }
            Item { Layout.fillWidth: true }
            GhostButton { text: "PDF"; tint: Theme.danger; implicitWidth: 80; onClicked: page.doExportPdf() }
            GhostButton { text: "CSV"; tint: Theme.success; implicitWidth: 80; onClicked: page.doExportCsv() }
        }

        GlassPanel {
            Layout.fillWidth: true
            implicitHeight: row.implicitHeight + Theme.s8
            radius: Theme.rLg
            RowLayout {
                id: row
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: Theme.s5
                spacing: Theme.s3
                DatePickerField {
                    id: dateField
                    Layout.preferredWidth: 180
                    label: "Date"
                    onAccepted: page.load()
                }
                PrimaryButton { Layout.alignment: Qt.AlignBottom; Layout.preferredWidth: 120; text: "Show"; onClicked: page.load() }
            }
        }

        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rLg
            DataTable {
                id: transactionTable
                anchors.fill: parent
                anchors.margins: Theme.s5
                emptyText: "No transactions for this date"
                rows: page.rows
                totals: page.totals
                totalsLabel: "Day Total"
                columns: [
                    { title: "Bill", key: "bill_no", weight: 1 },
                    { title: "Party", key: "party", weight: 2 },
                    { title: "Type", key: "type", chip: true, weight: 1.2 },
                    { title: "Mode", key: "mode", weight: 1 },
                    { title: "Amount", key: "amount", money: true, align: "right", weight: 1.2 }
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

            Rectangle {
                Layout.fillWidth: true
                height: 36
                radius: Theme.rSm
                color: mDel.hovered ? Theme.rowHover : "transparent"

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
}
