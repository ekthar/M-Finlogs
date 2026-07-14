import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property var rows: []
    property bool isLoading: false
    property string errorMessage: ""

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

    function load() { page.isLoading = true; backend.fetchAuditLogs() }
    Component.onCompleted: { load() }
    Connections {
        target: backend
        function onDataChanged() { page.load() }
        function onAuditLogsLoaded(result) {
            page.isLoading = false
            if (result && result.error) {
                page.errorMessage = result.error
                return
            }
            page.errorMessage = ""
            page.rows = result || []
        }
    }

    function doExportPdf() {
        var cols = ["Time", "User", "Action", "Details"]
        var data = []
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i]
            data.push([String(r.timestamp || ""), String(r.username || ""), String(r.action || ""), String(r.details || "")])
        }
        backend.exportTableToPdf("Audit Logs", cols, data)
    }
    function doExportCsv() {
        var cols = ["Time", "User", "Action", "Details"]
        var data = []
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i]
            data.push([String(r.timestamp || ""), String(r.username || ""), String(r.action || ""), String(r.details || "")])
        }
        backend.exportTableToExcel("Audit Logs", cols, data)
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
                title: "Audit Logs"
                subtitle: "Every change, who made it and when"
            }
            Item { Layout.fillWidth: true }
            GhostButton { text: "PDF"; tint: Theme.danger; implicitWidth: 80; onClicked: page.doExportPdf() }
            GhostButton { text: "CSV"; tint: Theme.success; implicitWidth: 80; onClicked: page.doExportCsv() }
            GhostButton { text: "Refresh"; onClicked: page.load() }
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
                emptyText: "No audit entries"
                loading: page.isLoading
                rows: page.rows
                columns: [
                    { title: "Time", key: "timestamp", weight: 1.6 },
                    { title: "User", key: "username", weight: 1.2 },
                    { title: "Action", key: "action", weight: 1.4 },
                    { title: "Details", key: "details", weight: 2.6 }
                ]
            }
        }
    }
}
