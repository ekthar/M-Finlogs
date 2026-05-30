import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property var rows: []

    function load() { rows = backend.auditLogs() }
    Component.onCompleted: load()
    Connections { target: backend; function onDataChanged() { page.load() } }

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
            GhostButton { text: "Refresh"; onClicked: page.load() }
        }

        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rLg
            DataTable {
                anchors.fill: parent
                anchors.margins: Theme.s5
                emptyText: "No audit entries"
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
