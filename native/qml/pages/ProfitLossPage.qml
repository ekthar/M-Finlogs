import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property var data: ({})

    function load() { data = backend.profitAndLoss() }
    Component.onCompleted: load()
    Connections { target: backend; function onDataChanged() { page.load() } }

    property real sales: Number(data.sales || 0)
    property real expenses: Number(data.expenses || 0)
    property real net: Number(data.net_profit || 0)

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s8
        anchors.rightMargin: Theme.s8
        anchors.topMargin: Theme.s5
        anchors.bottomMargin: Theme.s6
        spacing: Theme.s5

        SectionHeader {
            title: "Profit & Loss"
            subtitle: "Income statement for the financial year"
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width > 760 ? 3 : 1
            rowSpacing: Theme.s4
            columnSpacing: Theme.s4
            MetricCard { Layout.fillWidth: true; label: "Total Sales"; glyph: "\u25C8"; accent: Theme.success; value: page.sales }
            MetricCard { Layout.fillWidth: true; label: "Total Expenses"; glyph: "\u25BC"; accent: Theme.danger; value: page.expenses }
            MetricCard {
                Layout.fillWidth: true
                label: "Net Profit"; glyph: "\u2197"
                accent: page.net >= 0 ? Theme.success : Theme.danger
                value: page.net
            }
        }

        GlassPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: 160
            radius: Theme.rLg
            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.s6
                spacing: Theme.s6

                // Simple proportion bar (sales vs expenses)
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.s3
                    Text {
                        text: "Sales vs Expenses"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSection
                        font.weight: Font.Bold
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 18
                        radius: 9
                        color: Theme.glass
                        Rectangle {
                            height: parent.height
                            radius: 9
                            width: parent.width * (page.sales + page.expenses > 0
                                ? page.sales / (page.sales + page.expenses) : 0)
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: Theme.grad0 }
                                GradientStop { position: 1.0; color: Theme.success }
                            }
                            Behavior on width { NumberAnimation { duration: Theme.durSlow; easing.type: Theme.easeOut } }
                        }
                    }
                    Text {
                        text: "Margin: " + (page.sales > 0 ? (page.net / page.sales * 100).toFixed(1) : "0.0") + "%"
                        color: Theme.textDim
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSmall
                    }
                }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
