import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page

    property var rows: []
    property var partyList: []

    function refresh() {
        rows = backend.transactions(1, 100, 0)
    }
    function refreshParties() {
        partyList = backend.partyNames()
    }

    Component.onCompleted: {
        refresh()
        refreshParties()
        billField.text = ""
    }
    Connections {
        target: backend
        function onDataChanged() { page.refresh() }
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
            subtitle: "Record a transaction — press Enter to move between fields"
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
                    }
                    FieldInput {
                        id: partyField
                        Layout.fillWidth: true
                        label: "Party"
                        placeholder: "Search or type party"
                        completions: page.partyList
                    }
                    FieldCombo {
                        id: typeField
                        Layout.preferredWidth: 150
                        label: "Type"
                        options: ["Sale", "Sale Return", "Expense", "Receipt", "Purchase"]
                    }
                    FieldCombo {
                        id: modeField
                        Layout.preferredWidth: 140
                        label: "Mode"
                        options: ["Credit", "Cash", "UPI", "Bank"]
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
                    StatusPill { text: page.rows.length + " entries"; tint: Theme.accent }
                }

                DataTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    emptyText: "No transactions yet — add your first entry above"
                    rows: page.rows
                    columns: [
                        { title: "Date", key: "date", date: true, weight: 1.1 },
                        { title: "Bill No", key: "bill_no", weight: 1 },
                        { title: "Party", key: "party", weight: 2 },
                        { title: "Type", key: "type", chip: true, weight: 1.2 },
                        { title: "Mode", key: "mode", weight: 1 },
                        { title: "Amount", key: "amount", money: true, align: "right", weight: 1.2 }
                    ]
                }
            }
        }
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
            // Auto-increment bill, clear inputs, keep focus flow snappy
            billField.text = backend.nextBillNumber(billField.text)
            partyField.text = ""
            amountField.text = ""
            refreshParties()
            partyField.inputField.forceActiveFocus()
        }
        // Validation/errors are surfaced via the global toast from the backend.
    }
}
