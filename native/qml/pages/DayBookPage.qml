import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property var rows: []
    property var totals: ({})

    function load() {
        rows = backend.dayBook(dateField.isoText)
        var sum = 0
        for (var i = 0; i < rows.length; i++) sum += Number(rows[i].amount || 0)
        totals = { amount: sum }
    }
    Component.onCompleted: load()

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
                DatePickerField { id: dateField; Layout.preferredWidth: 180; label: "Date" }
                PrimaryButton { Layout.alignment: Qt.AlignBottom; Layout.preferredWidth: 120; text: "Show"; onClicked: page.load() }
            }
        }

        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rLg
            DataTable {
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
            }
        }
    }
}
