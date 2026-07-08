import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: page

    property var metrics: ({})
    property var trend: []

    function refresh() {
        metrics = backend.dashboard()
        trend = backend.salesTrend(30)
    }

    Component.onCompleted: {
        console.log("[DASHBOARD] Component.onCompleted - page.width:", page.width, "page.height:", page.height)
        refresh()
        console.log("[DASHBOARD] Component.onCompleted done")
    }
    Connections {
        target: backend
        function onDataChanged() {
            console.log("[DASHBOARD] backend.dataChanged received")
            page.refresh()
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.leftMargin: Theme.s8
        anchors.rightMargin: Theme.s8
        anchors.topMargin: Theme.s5
        contentWidth: width
        contentHeight: col.implicitHeight + Theme.s8
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            width: 8
            contentItem: Rectangle {
                implicitWidth: 8
                radius: 4
                color: Theme.alpha(Theme.accent, 0.45)
            }
            background: Rectangle {
                implicitWidth: 8
                radius: 4
                color: Theme.alpha(Theme.glass, 0.2)
            }
        }

        ColumnLayout {
            id: col
            width: parent.width
            spacing: Theme.s5

            RowLayout {
                Layout.fillWidth: true
                SectionHeader {
                    title: "Overview"
                    subtitle: "Your business at a glance"
                }
                Item { Layout.fillWidth: true }
                PrimaryButton {
                    text: "Closing Report"
                    implicitWidth: 140
                    onClicked: closingReport.show()
                }
            }

            // Metric cards
            GridLayout {
                Layout.fillWidth: true
                columns: width > 1100 ? 5 : (width > 720 ? 3 : 2)
                rowSpacing: Theme.s4
                columnSpacing: Theme.s4

                MetricCard {
                    Layout.fillWidth: true
                    label: "Sales Today"; glyph: "\u25C8"; accent: Theme.success
                    value: Number(page.metrics.sales_today || 0)
                }
                MetricCard {
                    Layout.fillWidth: true
                    label: "Sales This Month"; glyph: "\u25A4"; accent: Theme.accent
                    value: Number(page.metrics.sales_month || 0)
                }
                MetricCard {
                    Layout.fillWidth: true
                    label: "Cash Balance"; glyph: "\u25C9"; accent: Theme.accent3
                    value: Number(page.metrics.cash_balance || 0)
                }
                MetricCard {
                    Layout.fillWidth: true
                    label: "Bank Balance"; glyph: "\u2637"; accent: Theme.accent2
                    value: Number(page.metrics.bank_balance || 0)
                }
                MetricCard {
                    Layout.fillWidth: true
                    label: "Receivables"; glyph: "\u26A0"; accent: Theme.danger
                    value: Number(page.metrics.receivables || 0)
                }
            }

            // Charts row
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                spacing: Theme.s4

                // Sales trend
                GlassPanel {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 2
                    Layout.fillHeight: true
                    radius: Theme.rLg

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.s5
                        spacing: Theme.s3
                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                spacing: 2
                                Text {
                                    text: "Sales Trend"
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fsSection
                                    font.weight: Font.Bold
                                }
                                Text {
                                    text: "Last 30 days"
                                    color: Theme.textDim
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fsTiny
                                }
                            }
                            Item { Layout.fillWidth: true }
                            StatusPill { text: "Live"; tint: Theme.success }
                        }
                        Sparkline {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            points: page.trend
                            line: Theme.accent
                            fill: Theme.alpha(Theme.accent, 0.18)
                        }
                    }
                }

                // Fund split
                GlassPanel {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    radius: Theme.rLg

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.s5
                        spacing: Theme.s4
                        Text {
                            text: "Fund Availability"
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsSection
                            font.weight: Font.Bold
                        }

                        // Donut-ish ring drawn via Canvas
                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillHeight: true
                            Layout.preferredWidth: 160
                            property real cash: Number(page.metrics.cash_balance || 0)
                            property real bank: Number(page.metrics.bank_balance || 0)
                            property real total: Math.max(1, cash + bank)
                            id: donutWrap

                            Canvas {
                                id: donut
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    var cx = width / 2, cy = height / 2
                                    var r = Math.min(width, height) / 2 - 8
                                    var cashFrac = donutWrap.cash / donutWrap.total
                                    ctx.lineWidth = 16
                                    ctx.lineCap = "round"
                                    // Bank (background ring)
                                    ctx.beginPath()
                                    ctx.strokeStyle = Theme.accent2
                                    ctx.arc(cx, cy, r, -Math.PI/2 + cashFrac*2*Math.PI, -Math.PI/2 + 2*Math.PI)
                                    ctx.stroke()
                                    // Cash
                                    ctx.beginPath()
                                    ctx.strokeStyle = Theme.accent3
                                    ctx.arc(cx, cy, r, -Math.PI/2, -Math.PI/2 + cashFrac*2*Math.PI)
                                    ctx.stroke()
                                }
                                Connections {
                                    target: page
                                    function onMetricsChanged() { donut.requestPaint() }
                                }
                            }
                            Column {
                                anchors.centerIn: parent
                                spacing: 0
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Total"
                                    color: Theme.textDim
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fsTiny
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: backend.formatMoney(donutWrap.total === 1 ? 0 : donutWrap.total)
                                    color: Theme.text
                                    font.family: Theme.monoFamily
                                    font.pixelSize: Theme.fsBody
                                    font.weight: Font.Bold
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.s4
                            RowLayout {
                                spacing: 6
                                Rectangle { width: 10; height: 10; radius: 5; color: Theme.accent3 }
                                Text { text: "Cash"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                            }
                            RowLayout {
                                spacing: 6
                                Rectangle { width: 10; height: 10; radius: 5; color: Theme.accent2 }
                                Text { text: "Bank"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
        }
    }

    // Closing Report Overlay
    ClosingReport {
        id: closingReport
    }
}
