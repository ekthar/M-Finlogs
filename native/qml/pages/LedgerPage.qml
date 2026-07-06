import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property var rawRows: []
    property var rows: []
    property real openingBalance: 0
    property var partyList: []
    property var totals: ({})

    Component.onCompleted: partyList = backend.partyNames()

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
                date: r.date || "",
                bill_no: r.bill_no || "",
                type: r.type || "",
                mode: r.mode || "",
                debit: dr > 0 ? dr : null,
                credit: cr > 0 ? cr : null,
                balance_label: balLabel
            })
        }
        totals = { debit: totalDr, credit: totalCr }
        return result
    }

    function load() {
        if (partyField.text.trim().length === 0) {
            backend.toast("Choose a party first", "error")
            return
        }
        var res = backend.ledger(partyField.text, dateRange.fromIso, dateRange.toIso)
        if (res && res.ok === false) return
        openingBalance = Number(res.opening_balance || 0)
        rawRows = res.data || []
        rows = transformRows(rawRows)
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
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSection
                        font.weight: Font.Bold
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Opening Balance: " + backend.formatMoney(page.openingBalance) + (page.openingBalance >= 0 ? " Dr" : " Cr")
                        color: Theme.textDim
                        font.family: Theme.monoFamily
                        font.pixelSize: Theme.fsSmall
                    }
                }
                DataTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    emptyText: "Select a party and press Show"
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
                }
            }
        }
    }
}
