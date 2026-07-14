import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property var rows: []
    property var totals: ({})
    property bool isLoading: false
    property string errorMessage: ""

    function load() {
        page.isLoading = true
        backend.fetchTrialBalance()
    }
    function applyRows(result) {
        page.isLoading = false
        if (result && result.error) {
            page.errorMessage = result.error
            return
        }
        page.errorMessage = ""
        rows = result || []
        var d = 0, c = 0
        for (var i = 0; i < rows.length; i++) {
            d += Number(rows[i].debit || 0)
            c += Number(rows[i].credit || 0)
        }
        totals = { debit: d, credit: c }
    }
    Timer {
        id: timeoutTimer
        interval: 15000
        running: page.isLoading
        onTriggered: {
            if (page.isLoading) {
                page.isLoading = false
                page.errorMessage = "Request timed out. Check your database connection."
            }
        }
    }

    Component.onCompleted: { load() }
    Connections {
        target: backend
        function onDataChanged() { page.load() }
        function onTrialBalanceLoaded(result) { page.applyRows(result) }
    }

    function doExportPdf() {
        var cols = ["Account/Party", "Debit", "Credit"]
        var data = []
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i]
            data.push([String(r.account || ""), String(r.debit || ""), String(r.credit || "")])
        }
        backend.exportTableToPdf("Trial Balance", cols, data)
    }
    function doExportCsv() {
        var cols = ["Account/Party", "Debit", "Credit"]
        var data = []
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i]
            data.push([String(r.account || ""), String(r.debit || ""), String(r.credit || "")])
        }
        backend.exportTableToExcel("Trial Balance", cols, data)
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
                title: "Trial Balance"
                subtitle: "Account-wise debit and credit position"
            }
            Item { Layout.fillWidth: true }
            GhostButton { text: "PDF"; tint: Theme.danger; implicitWidth: 80; onClicked: page.doExportPdf() }
            GhostButton { text: "CSV"; tint: Theme.success; implicitWidth: 80; onClicked: page.doExportCsv() }
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
            DataTable {
                anchors.fill: parent
                anchors.margins: Theme.s5
                emptyText: "No account data"
                loading: page.isLoading
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
