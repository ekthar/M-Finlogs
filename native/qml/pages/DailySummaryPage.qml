import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property var rows: []

    function load() { rows = backend.dailySummary(fromDate.isoText, toDate.isoText) }
    Component.onCompleted: {
        // Default to current month-to-date
        var now = new Date()
        fromDate.selectedDate = new Date(now.getFullYear(), now.getMonth(), 1)
        toDate.selectedDate = now
        load()
    }

    function doExportPdf() {
        var cols = ["Date", "Opening", "Cash In", "Cash Exp", "Needed", "Short/Excess", "Bank", "Total Sales"]
        var data = []
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i]
            data.push([String(r.date || ""), String(r.opening_cash || ""), String(r.cash_in || ""), String(r.cash_expense || ""), String(r.cash_needed || ""), String(r.cash_short_excess || ""), String(r.bank || ""), String(r.total_sales || "")])
        }
        backend.exportTableToPdf("Daily Summary", cols, data)
    }
    function doExportCsv() {
        var cols = ["Date", "Opening", "Cash In", "Cash Exp", "Needed", "Short/Excess", "Bank", "Total Sales"]
        var data = []
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i]
            data.push([String(r.date || ""), String(r.opening_cash || ""), String(r.cash_in || ""), String(r.cash_expense || ""), String(r.cash_needed || ""), String(r.cash_short_excess || ""), String(r.bank || ""), String(r.total_sales || "")])
        }
        backend.exportTableToExcel("Daily Summary", cols, data)
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
                title: "Daily Summary"
                subtitle: "Cash flow and sales by day"
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
                DatePickerField { id: fromDate; Layout.preferredWidth: 160; label: "From" }
                DatePickerField { id: toDate; Layout.preferredWidth: 160; label: "To" }
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
                emptyText: "No data for the selected range"
                rows: page.rows
                columns: [
                    { title: "Date", key: "date", date: true, weight: 1.2 },
                    { title: "Opening", key: "opening_cash", money: true, align: "right" },
                    { title: "Cash In", key: "cash_in", money: true, align: "right" },
                    { title: "Cash Exp", key: "cash_expense", money: true, align: "right" },
                    { title: "Needed", key: "cash_needed", money: true, align: "right" },
                    { title: "Short/Excess", key: "cash_short_excess", money: true, align: "right" },
                    { title: "Bank", key: "bank", money: true, align: "right" },
                    { title: "Total Sales", key: "total_sales", money: true, align: "right" }
                ]
            }
        }
    }
}
