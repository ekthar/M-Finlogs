import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property var rows: []
    property real openingBalance: 0
    property var partyList: []

    Component.onCompleted: partyList = backend.partyNames()

    function load() {
        if (partyField.text.trim().length === 0) {
            backend.toast("Choose a party first", "error")
            return
        }
        var res = backend.ledger(partyField.text, fromDate.isoText, toDate.isoText)
        if (res && res.ok === false) return
        openingBalance = Number(res.opening_balance || 0)
        rows = res.data || []
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s8
        anchors.rightMargin: Theme.s8
        anchors.topMargin: Theme.s5
        anchors.bottomMargin: Theme.s6
        spacing: Theme.s5

        SectionHeader {
            title: "Party Ledger"
            subtitle: "Statement of account with running balance"
        }

        GlassPanel {
            Layout.fillWidth: true
            implicitHeight: filterRow.implicitHeight + Theme.s8
            radius: Theme.rLg
            RowLayout {
                id: filterRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.s5
                spacing: Theme.s3
                FieldInput {
                    id: partyField
                    Layout.fillWidth: true
                    label: "Party"
                    placeholder: "Search party"
                    completions: page.partyList
                    onAccepted: page.load()
                }
                DatePickerField { id: fromDate; Layout.preferredWidth: 160; label: "From" }
                DatePickerField { id: toDate; Layout.preferredWidth: 160; label: "To" }
                PrimaryButton { Layout.alignment: Qt.AlignBottom; Layout.preferredWidth: 120; text: "Show"; onClicked: page.load() }
            }
        }

        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rLg
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.s5
                spacing: Theme.s3
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Statement"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSection
                        font.weight: Font.Bold
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Opening: " + backend.formatMoney(page.openingBalance)
                        color: Theme.textDim
                        font.family: Theme.monoFamily
                        font.pixelSize: Theme.fsSmall
                    }
                }
                DataTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    emptyText: "Select a party and press Show"
                    rows: page.rows
                    columns: [
                        { title: "Date", key: "date", date: true, weight: 1.1 },
                        { title: "Bill", key: "bill_no", weight: 1 },
                        { title: "Type", key: "type", chip: true, weight: 1.3 },
                        { title: "Mode", key: "mode", weight: 1 },
                        { title: "Amount", key: "amount", money: true, align: "right", weight: 1.2 },
                        { title: "Balance", key: "balance", money: true, align: "right", weight: 1.3 }
                    ]
                }
            }
        }
    }
}
