import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: page

    property var metrics: ({})
    property var trend: []

    Connections {
        target: backend
        function onDashboardLoaded(res) {
            if (res && res.error) {
                backend.toast(res.error, "error")
                return
            }
            metrics = res
            trend = backend.salesTrend(30)
        }
        function onDataChanged() {
            page.refresh()
        }
    }

    function refresh() {
        backend.fetchDashboard()
    }

    Component.onCompleted: refresh()

    Flickable {
        anchors.fill: parent
        anchors.leftMargin: Theme.s8
        anchors.rightMargin: Theme.s8
        anchors.topMargin: Theme.s6
        contentWidth: width
        contentHeight: col.implicitHeight + Theme.s10
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            width: 8
            contentItem: Rectangle {
                implicitWidth: 8
                radius: 4
                color: Theme.alpha(Theme.palette.primary, 0.45)
            }
            background: Rectangle {
                implicitWidth: 8
                radius: 4
                color: Theme.alpha(Theme.palette.surface, 0.2)
            }
        }

        ColumnLayout {
            id: col
            width: parent.width
            spacing: Theme.s7

            // ── Header row ────────────────────────────────────────────
            StaggerEntrance {
                index: 0
                baseDelay: Theme.staggerCard
                anchors.left: parent.left
                anchors.right: parent.right
                RowLayout {
                    width: parent.width
                    SectionHeader {
                        title: "Dashboard"
                        subtitle: "Your business at a glance"
                    }
                    Item { Layout.fillWidth: true }
                    PrimaryButton {
                        text: "Closing Report"
                        implicitWidth: 140
                        onClicked: closingReport.show()
                    }
                }
            }

            // ── Metric cards (inline stagger preserves GridLayout sizing) ──
            GridLayout {
                Layout.fillWidth: true
                columns: width > 1200 ? 4 : (width > 700 ? 3 : 2)
                rowSpacing: Theme.s5
                columnSpacing: Theme.s5

                MetricCard {
                    Layout.fillWidth: true
                    label: "Sales Today"
                    glyph: "\u25C8"
                    accent: Theme.palette.success
                    value: Number(page.metrics.sales_today || 0)
                }
                MetricCard {
                    Layout.fillWidth: true
                    label: "Sales This Month"
                    glyph: "\u25A4"
                    accent: Theme.palette.primary
                    value: Number(page.metrics.sales_month || 0)
                }
                MetricCard {
                    Layout.fillWidth: true
                    label: "Cash Balance"
                    glyph: "\u25C9"
                    accent: Theme.palette.info
                    value: Number(page.metrics.cash_balance || 0)
                }
                MetricCard {
                    Layout.fillWidth: true
                    label: "Bank Balance"
                    glyph: "\u2637"
                    accent: Theme.palette.info
                    value: Number(page.metrics.bank_balance || 0)
                }
                MetricCard {
                    Layout.fillWidth: true
                    label: "Receivables"
                    glyph: "\u26A0"
                    accent: Theme.palette.danger
                    value: Number(page.metrics.receivables || 0)
                }
            }

            // ── Charts row ────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                spacing: Theme.s5

                GlassPanel {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 2
                    Layout.fillHeight: true
                    radius: Theme.rLg

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.s6
                        spacing: Theme.s4
                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                spacing: 2
                                Text {
                                    text: "Sales Trend"
                                    color: Theme.palette.fg
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fsSection
                                    font.weight: Theme.wSemibold
                                }
                                Text {
                                    text: "Last 30 days"
                                    color: Theme.palette.fgMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fsCaption
                                }
                            }
                            Item { Layout.fillWidth: true }
                            StatusPill { text: "Live"; tint: Theme.palette.success }
                        }
                        Sparkline {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            points: page.trend
                            line: Theme.palette.primary
                            fill: Theme.alpha(Theme.palette.primary, 0.14)
                        }
                    }
                }

                GlassPanel {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    radius: Theme.rLg

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.s6
                        spacing: Theme.s4
                        Text {
                            text: "Fund Availability"
                            color: Theme.palette.fg
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsSection
                            font.weight: Theme.wSemibold
                        }

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
                                    ctx.beginPath()
                                    ctx.strokeStyle = Theme.palette.primary
                                    ctx.arc(cx, cy, r, -Math.PI/2 + cashFrac*2*Math.PI, -Math.PI/2 + 2*Math.PI)
                                    ctx.stroke()
                                    ctx.beginPath()
                                    ctx.strokeStyle = Theme.palette.info
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
                                    color: Theme.palette.fgMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fsCaption
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: backend.formatMoney(donutWrap.total === 1 ? 0 : donutWrap.total)
                                    color: Theme.palette.fg
                                    font.family: Theme.monoFamily
                                    font.pixelSize: Theme.fsHero
                                    font.weight: Theme.wBold
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.s4
                            RowLayout {
                                spacing: 6
                                Rectangle { width: 10; height: 10; radius: 5; color: Theme.palette.info }
                                Text { text: "Cash"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsCaption }
                            }
                            RowLayout {
                                spacing: 6
                                Rectangle { width: 10; height: 10; radius: 5; color: Theme.palette.primary }
                                Text { text: "Bank"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsCaption }
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
        }
    }

    ClosingReport {
        id: closingReport
    }
}
