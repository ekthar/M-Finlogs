import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property var rows: []
    property real total: 0
    property var stats: ({})

    function load() {
        var res = backend.outstanding()
        if (res && res.ok === false) return
        rows = res.data || []
        total = Number(res.total || 0)
        stats = res.stats || {}
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
            title: "Credit Outstanding"
            subtitle: "Receivables and aging risk"
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width > 760 ? 3 : 1
            rowSpacing: Theme.s4
            columnSpacing: Theme.s4
            MetricCard {
                Layout.fillWidth: true
                label: "Total Outstanding"; glyph: "\u26A0"; accent: Theme.danger
                value: page.total
            }
            MetricCard {
                Layout.fillWidth: true
                label: "High Risk (15d+)"; glyph: "\u23F3"; accent: Theme.warning
                value: Number(page.stats.high_amount || 0)
            }
            MetricCard {
                Layout.fillWidth: true
                label: "Critical (30d+)"; glyph: "\u2757"; accent: Theme.danger
                value: Number(page.stats.critical_amount || 0)
            }
        }

        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rLg
            DataTable {
                anchors.fill: parent
                anchors.margins: Theme.s5
                emptyText: "No outstanding balances"
                rows: page.rows
                columns: [
                    { title: "Party", key: "party", weight: 2 },
                    { title: "Type", key: "type", weight: 1.2 },
                    { title: "Status", key: "status", chip: true, weight: 1.2 },
                    { title: "Days", key: "days_unpaid", align: "right", weight: 0.8 },
                    { title: "Last Receipt", key: "last_receipt", weight: 1.3 },
                    { title: "Balance", key: "balance", money: true, align: "right", weight: 1.3 }
                ]
            }
        }
    }
}
