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
                        onAccepted: typeField.forceActiveFocus()
                        // Lookup balance when user finishes typing
                        inputField.onEditingFinished: page.lookupPartyBalance(text)
                    }
                    FieldCombo {
                        id: typeField
                        Layout.preferredWidth: 150
                        label: "Type"
                        options: ["Sale", "Sale Return", "Expense", "Receipt", "Purchase"]
                        // Enter on combo moves to next field
                        Keys.onReturnPressed: modeField.forceActiveFocus()
                        Keys.onEnterPressed: modeField.forceActiveFocus()
                    }
                    FieldCombo {
                        id: modeField
                        Layout.preferredWidth: 140
                        label: "Mode"
                        options: ["Credit", "Cash", "UPI", "Bank"]
                        Keys.onReturnPressed: amountField.inputField.forceActiveFocus()
                        Keys.onEnterPressed: amountField.inputField.forceActiveFocus()
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
                        text: "Export 7-Day PDF"
                        tint: Theme.accent2
                        implicitWidth: 140
                        onClicked: backend.exportRecentPdf(7)
                    }
                    StatusPill { text: page.rows.length + " entries"; tint: Theme.accent }
                }

                DataTable {
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
            // Auto-increment bill, clear inputs, cycle focus back to Bill
            billField.text = backend.nextBillNumber(billField.text)
            partyField.text = ""
            amountField.text = ""
            refreshParties()
            billField.inputField.forceActiveFocus()
        }
    }
}
