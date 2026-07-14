import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: page
    property var rows: []
    property real totalOutstanding: 0

    function refresh() {
        backend.fetchPartyBalances()
    }
    function applyRows(result) {
        var all = result || []
        rows = all.filter(function(row) { return Number(row.balance || 0) > 0 })
        var t = 0
        for (var i = 0; i < rows.length; i++) t += Number(rows[i].balance || 0)
        totalOutstanding = t
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

    function ageLabel(row) {
        var days = daysSince(row.lastDate)
        if (days >= 60) return "60+ days"
        if (days >= 30) return "30-60 days"
        if (days >= 15) return "15-30 days"
        return "< 15 days"
    }

    function ageColor(row) {
        var days = daysSince(row.lastDate)
        if (days >= 60) return Theme.danger
        if (days >= 30) return Theme.warning
        if (days >= 15) return Theme.palette.primary
        return Theme.success
    }

    function doExportCsv() {
        var cols = ["Party", "Ledger Closing Balance", "Last Receipt", "Status", "Age"]
        var data = []
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i]
            data.push([r.party, String(r.closing_balance || r.balance || "0"), r.lastDate || "", "Dr", page.ageLabel(r)])
        }
        backend.exportTableToExcel("Credit Followup", cols, data)
    }

    function doExportPdf() {
        var cols = ["Party", "Ledger Closing Balance", "Last Receipt", "Status", "Age"]
        var data = []
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i]
            data.push([r.party, String(r.closing_balance || r.balance || "0"), r.lastDate || "", "Dr", page.ageLabel(r)])
        }
        backend.exportTableToPdf("Credit Followup", cols, data)
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
            title: "Credit Follow-up"
            subtitle: page.totalOutstanding > 0
                ? "Total outstanding: " + backend.formatMoney(page.totalOutstanding) + " — " + page.rows.length + " debtors"
                : "All accounts settled"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s3

            StatusPill {
                text: "Outstanding: " + backend.formatMoney(page.totalOutstanding)
                tint: page.totalOutstanding > 0 ? Theme.danger : Theme.success
            }
            StatusPill {
                text: page.rows.length + " debtors"
                tint: page.rows.length > 0 ? Theme.warning : Theme.success
            }

            Item { Layout.fillWidth: true }

            GhostButton { text: "Export CSV"; implicitWidth: 100; onClicked: page.doExportCsv() }
            GhostButton { text: "Export PDF"; tint: Theme.danger; implicitWidth: 100; onClicked: page.doExportPdf() }
            GhostButton { text: "Refresh"; implicitWidth: 100; onClicked: page.refresh() }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.palette.border
        }

        // Legend
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s4
            Text { text: "\u25CF"; color: Theme.success; font.pixelSize: 10 }
            Text { text: "< 15 days"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
            Text { text: "\u25CF"; color: Theme.palette.primary; font.pixelSize: 10 }
            Text { text: "15-30 days"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
            Text { text: "\u25CF"; color: Theme.warning; font.pixelSize: 10 }
            Text { text: "30-60 days"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
            Text { text: "\u25CF"; color: Theme.danger; font.pixelSize: 10 }
            Text { text: "60+ days overdue"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
            Item { Layout.fillWidth: true }
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
                        Text { Layout.fillWidth: true; Layout.preferredWidth: 2.5; text: "Party"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold; font.capitalization: Font.AllUppercase; verticalAlignment: Text.AlignVCenter }
                        Text { Layout.fillWidth: true; Layout.preferredWidth: 1.5; text: "Outstanding"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold; font.capitalization: Font.AllUppercase; horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter }
                        Text { Layout.fillWidth: true; Layout.preferredWidth: 1.2; text: "Age"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold; font.capitalization: Font.AllUppercase; verticalAlignment: Text.AlignVCenter }
                        Text { Layout.fillWidth: true; Layout.preferredWidth: 1.2; text: "Last Tx"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold; font.capitalization: Font.AllUppercase; verticalAlignment: Text.AlignVCenter }
                        Text { Layout.fillWidth: true; Layout.preferredWidth: 1; text: "Type"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold; font.capitalization: Font.AllUppercase; verticalAlignment: Text.AlignVCenter }
                        Text { Layout.fillWidth: true; Layout.preferredWidth: 1.2; text: "Amount"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.Bold; font.capitalization: Font.AllUppercase; horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter }
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
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
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
                                text: backend.formatMoney(Number(modelData.balance || 0))
                                color: Theme.danger; font.family: Theme.monoFamily
                                font.pixelSize: Theme.fsSmall; font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                            }

                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredWidth: 1.2
                                height: 24; radius: Theme.rPill
                                color: Theme.alpha(page.ageColor(modelData), 0.16)
                                Text {
                                    anchors.centerIn: parent
                                    text: page.ageLabel(modelData)
                                    color: page.ageColor(modelData)
                                    font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.DemiBold
                                }
                            }

                            Text {
                                Layout.fillWidth: true; Layout.preferredWidth: 1.2
                                text: modelData.lastDate ? backend.formatDate(modelData.lastDate) : ""
                                color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
                                verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true; Layout.preferredWidth: 1
                                text: modelData.lastType || ""
                                color: Theme.typeColor(modelData.lastType || "Normal"); font.family: Theme.fontFamily
                                font.pixelSize: Theme.fsTiny; font.weight: Font.DemiBold
                                verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true; Layout.preferredWidth: 1.2
                                text: modelData.lastAmount ? backend.formatMoney(Number(modelData.lastAmount)) : ""
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

                // Footer total
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    color: Theme.alpha(Theme.palette.fg, 0.06)
                    visible: page.rows.length > 0

                    Rectangle { anchors.top: parent.top; width: parent.width; height: 2; color: Theme.alpha(Theme.danger, 0.5) }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.s4
                        anchors.rightMargin: Theme.s4

                        Text { Layout.fillWidth: true; Layout.preferredWidth: 2.5; text: "Total"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.Bold; verticalAlignment: Text.AlignVCenter }
                        Text { Layout.fillWidth: true; Layout.preferredWidth: 1.5; text: backend.formatMoney(page.totalOutstanding); color: Theme.danger; font.family: Theme.monoFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.Bold; horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter }
                        Text { Layout.fillWidth: true; Layout.preferredWidth: 1.2; text: page.rows.length + " debtors"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; verticalAlignment: Text.AlignVCenter }
                        Item { Layout.fillWidth: true; Layout.preferredWidth: 2.4 }
                    }
                }
            }
        }
    }
}
