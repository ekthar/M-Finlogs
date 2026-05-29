import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property var rows: []

    function load() { rows = backend.trialBalance() }
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
                columns: [
                    { title: "Account / Party", key: "account", weight: 2 },
                    { title: "Debit", key: "debit", money: true, align: "right", weight: 1.2 },
                    { title: "Credit", key: "credit", money: true, align: "right", weight: 1.2 }
                ]
            }
        }
    }
}
