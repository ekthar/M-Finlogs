import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: page
    property var rows: []
    property var stockRows: []
    property var fyList: []
    property string selectedFy: ""
    property int selectedMonth: new Date().getMonth() + 1

    Component.onCompleted: {
        fyList = backend.financialYears()
        if (fyList.length > 0) selectedFy = String(fyList[0])
        load()
    }
    Connections { target: backend; function onDataChanged() { page.load() } }

    function load() {
        if (!selectedFy) return
        rows = backend.inventorySnapshot(selectedFy, selectedMonth)
        stockRows = backend.stockValue(selectedFy, selectedMonth)
    }

    function saveAll() {
        // Collect rows from current model back into a flat list for backend
        var data = []
        for (var i = 0; i < rows.length; i++) {
            data.push(rows[i])
        }
        backend.saveInventory(selectedFy, selectedMonth, data)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s8
        anchors.rightMargin: Theme.s8
        anchors.topMargin: Theme.s5
        anchors.bottomMargin: Theme.s6
        spacing: Theme.s5

        SectionHeader {
            title: "Inventory"
            subtitle: "Daily stock quantities and purchase tracking"
        }

        // Filters
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

                FieldCombo {
                    id: fyCombo
                    Layout.preferredWidth: 180
                    label: "Financial Year"
                    options: page.fyList
                    onCurrentTextChanged: { page.selectedFy = currentText; page.load() }
                }
                FieldCombo {
                    id: monthCombo
                    Layout.preferredWidth: 140
                    label: "Month"
                    options: ["1","2","3","4","5","6","7","8","9","10","11","12"]
                    currentIndex: page.selectedMonth - 1
                    onCurrentIndexChanged: { page.selectedMonth = currentIndex + 1; page.load() }
                }
                Item { Layout.fillWidth: true }
                PrimaryButton { text: "Refresh"; onClicked: page.load() }
                PrimaryButton { text: "Save"; onClicked: page.saveAll() }
                GhostButton { text: "Export"; onClicked: backend.exportTableToExcel("Inventory", [], []) }
            }
        }

        // Inventory data table
        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rLg
            DataTable {
                anchors.fill: parent
                anchors.margins: Theme.s5
                emptyText: "No inventory items \u2014 add products via the old desktop app or import"
                rows: page.rows
                columns: [
                    { title: "Name", key: "name", weight: 2.4 },
                    { title: "Cost", key: "cost", money: true, align: "right", weight: 1 },
                    { title: "Min Stock", key: "min_stock", align: "right", weight: 0.8 },
                    { title: "Qty (Day 1)", key: "qty_1", align: "right", weight: 0.8 },
                    { title: "Qty (Day 15)", key: "qty_15", align: "right", weight: 0.8 },
                    { title: "Qty (Day 30)", key: "qty_30", align: "right", weight: 0.8 }
                ]
            }
        }

        // Stock value summary
        GlassPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            radius: Theme.rLg
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.s5
                spacing: Theme.s3
                Text {
                    text: "Stock Value"
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsSection
                    font.weight: Font.Bold
                }
                DataTable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    emptyText: "No stock data"
                    rows: page.stockRows
                    columns: [
                        { title: "Day", key: "day", align: "center", weight: 0.6 },
                        { title: "Total Qty", key: "quantity", align: "right", weight: 1 },
                        { title: "Stock Value", key: "stock_value", money: true, align: "right", weight: 1.2 }
                    ]
                }
            }
        }
    }
}
