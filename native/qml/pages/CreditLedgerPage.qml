import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: page
    property var rows: []
    property real totalDr: 0
    property real totalCr: 0
    property bool isLoading: false
    property string errorMessage: ""

    function refresh() {
        page.isLoading = true
        backend.fetchPartyBalances()
    }
    function applyRows(result) {
        page.isLoading = false
        if (result && result.error) {
            page.errorMessage = result.error
            return
        }
        page.errorMessage = ""
        rows = result || []
        var dr = 0, cr = 0
        for (var i = 0; i < rows.length; i++) {
            var b = Number(rows[i].balance || 0)
            if (b > 0) dr += b; else cr += Math.abs(b)
        }
        totalDr = dr; totalCr = cr
    }

    function daysSince(dateStr) {
        if (!dateStr || dateStr.length === 0) return 999
        var parts = dateStr.split('-')
        var d
        if (parts[0].length === 4)
            d = new Date(parseInt(parts[0]), parseInt(parts[1]) - 1, parseInt(parts[2]))
        else
            d = new Date(parseInt(parts[2]), parseInt(parts[1]) - 1, parseInt(parts[0]))
        return Math.floor((new Date() - d) / (86400000))
    }

    function statusColor(row) {
        var b = Number(row.balance || 0)
        if (b <= 0) return Theme.success
        var days = daysSince(row.lastDate)
        if (days >= 60) return Theme.danger
        if (days >= 30) return Theme.warning
        return Theme.palette.primary
    }

    function statusLabel(row) {
        var b = Number(row.balance || 0)
        if (b <= 0) return "Settled"
        var days = daysSince(row.lastDate)
        if (days >= 60) return "Overdue " + days + "d"
        if (days >= 30) return days + "d due"
        return "Active"
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

    Component.onCompleted: refresh()
    Connections {
        target: backend
        function onDataChanged() { page.refresh() }
        function onPartyBalancesLoaded(result) { page.applyRows(result) }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s8
        anchors.rightMargin: Theme.s8
        anchors.topMargin: Theme.s5
        anchors.bottomMargin: Theme.s6
        spacing: Theme.s5

        SectionHeader {
            title: "Credit Ledger"
            subtitle: "Running balance for every party — colour-coded by age"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s3
            StatusPill { text: "Total Dr: " + backend.formatMoney(page.totalDr); tint: page.totalDr > 0 ? Theme.danger : Theme.success }
            StatusPill { text: "Total Cr: " + backend.formatMoney(page.totalCr); tint: page.totalCr > 0 ? Theme.palette.info : Theme.success }
            Item { Layout.fillWidth: true }
            GhostButton { text: "Refresh"; implicitWidth: 100; onClicked: page.refresh() }
            GhostButton { text: "PDF"; tint: Theme.danger; implicitWidth: 80; onClicked: page.doExportPdf() }
            GhostButton { text: "CSV"; tint: Theme.success; implicitWidth: 80; onClicked: page.doExportCsv() }
        }

        ErrorBanner {
            Layout.fillWidth: true
            message: page.errorMessage
            onRetry: page.refresh()
        }

        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rLg
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Header
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    color: Theme.alpha(Theme.palette.fg, 0.06)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.s4
                        anchors.rightMargin: Theme.s4
                        spacing: 0
                        Repeater {
                            model: [
                                { title: "Party", weight: 2.5 },
                                { title: "Balance", weight: 1.5, align: Text.AlignRight },
                                { title: "Status", weight: 1.2 },
                                { title: "Last Receipt", weight: 1.2 },
                                { title: "Status", weight: 1 },
                                { title: "Ledger Closing", weight: 1.2, align: Text.AlignRight }
                            ]
                            delegate: Text {
                                Layout.fillWidth: true
                                Layout.preferredWidth: modelData.weight
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: modelData.align || Text.AlignLeft
                                text: modelData.title
                                color: Theme.palette.fgMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fsTiny
                                font.weight: Font.Bold
                                font.capitalization: Font.AllUppercase
                                font.letterSpacing: 0.5
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: page.rows
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded; width: 8
                        contentItem: Rectangle { implicitWidth: 8; radius: 4; color: Theme.alpha(Theme.palette.primary, 0.45) }
                        background: Rectangle { implicitWidth: 8; radius: 4; color: Theme.alpha(Theme.alpha(Theme.palette.fg, 0.04), 0.2) }
                    }

                    delegate: Rectangle {
                        width: parent.width
                        height: 50
                        color: index % 2 === 0 ? "transparent" : Theme.alpha(Theme.palette.fg, 0.02)

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.s4
                            anchors.rightMargin: Theme.s4
                            spacing: 0

                            Text {
                                Layout.fillWidth: true; Layout.preferredWidth: 2.5
                                text: modelData.party || ""
                                color: Theme.palette.fg; font.family: Theme.fontFamily
                                font.pixelSize: Theme.fsSmall; font.weight: Font.DemiBold
                                elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
                            }

                            Text {
                                Layout.fillWidth: true; Layout.preferredWidth: 1.5
                                text: modelData.balanceLabel || ""
                                color: Number(modelData.balance || 0) > 0 ? Theme.danger : Theme.palette.info
                                font.family: Theme.monoFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                            }

                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredWidth: 1.2
                                height: 24
                                radius: Theme.rPill
                                color: Theme.alpha(page.statusColor(modelData), 0.16)
                                Text {
                                    anchors.centerIn: parent
                                    text: page.statusLabel(modelData)
                                    color: page.statusColor(modelData)
                                    font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.DemiBold
                                }
                            }

                            Text {
                                Layout.fillWidth: true; Layout.preferredWidth: 1.2
                                text: modelData.lastDate || ""
                                color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
                                verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true; Layout.preferredWidth: 1
                                text: Number(modelData.balance || 0) >= 0 ? "Dr" : "Cr"
                                color: Theme.typeColor(modelData.lastType || "")
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.DemiBold
                                verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true; Layout.preferredWidth: 1.2
                                text: backend.formatMoney(Number(modelData.closing_balance || modelData.balance || 0))
                                color: Theme.palette.fg; font.family: Theme.monoFamily; font.pixelSize: Theme.fsSmall
                                horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom; width: parent.width; height: 1
                            color: Theme.palette.borderSubtle
                        }
                    }
                }
            }
        }
    }

    function doExportPdf() {
        var cols = ["Party", "Balance", "Status", "Last Date", "Type", "Closing Balance"]
        var data = []
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i]
            data.push([String(r.party || ""), String(r.balanceLabel || ""), page.statusLabel(r), String(r.lastDate || ""), Number(r.balance || 0) >= 0 ? "Dr" : "Cr", String(r.closing_balance || r.balance || "0")])
        }
        backend.exportTableToPdf("Credit Ledger", cols, data)
    }

    function doExportCsv() {
        var cols = ["Party", "Balance", "Status", "Last Date", "Type", "Closing Balance"]
        var data = []
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i]
            data.push([String(r.party || ""), String(r.balanceLabel || ""), page.statusLabel(r), String(r.lastDate || ""), Number(r.balance || 0) >= 0 ? "Dr" : "Cr", String(r.closing_balance || r.balance || "0")])
        }
        backend.exportTableToExcel("Credit Ledger", cols, data)
    }
}
