import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property var rows: []

    property var totals: ({})

    function load() {
        rows = backend.trialBalance()
        var d = 0, c = 0
        for (var i = 0; i < rows.length; i++) {
            d += Number(rows[i].debit || 0)
            c += Number(rows[i].credit || 0)
        }
        totals = { debit: d, credit: c }
    }
    Component.onCompleted: load()
    Connections { target: backend; function onDataChanged() { page.load() } }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s8
        anchors.rightMargin: Theme.s8
        anchors.topMargin: Theme.s5
        anchors.bottomMargin: Theme.s6
        spacing: Theme.s5

        SectionHeader {
            title: "Trial Balance"
            subtitle: "Account-wise debit and credit position"
        }

        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rLg
            DataTable {
                anchors.fill: parent
                anchors.margins: Theme.s5
                emptyText: "No account data"
                rows: page.rows
                totals: page.totals
                totalsLabel: "Total"
                columns: [
                    { title: "Account / Party", key: "account", weight: 2 },
                    { title: "Debit", key: "debit", money: true, align: "right", weight: 1.2 },
                    { title: "Credit", key: "credit", money: true, align: "right", weight: 1.2 }
                ]
            }
        }
    }
}
