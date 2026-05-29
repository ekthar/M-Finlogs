import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property var rows: []

    function load() { rows = backend.dayBook(dateField.isoText) }
    Component.onCompleted: load()

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s8
        anchors.rightMargin: Theme.s8
        anchors.topMargin: Theme.s5
        anchors.bottomMargin: Theme.s6
        spacing: Theme.s5

        SectionHeader {
            title: "Day Book"
            subtitle: "All transactions on a chosen day"
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
