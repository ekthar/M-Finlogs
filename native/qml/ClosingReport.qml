import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: root
    anchors.fill: parent
    z: 10001

    property bool open: false
    property var report: ({})

    function show() {
        console.log("[CLOSING] show() called")
        report = backend.closingReport()
        console.log("[CLOSING] backend.closingReport returned:", JSON.stringify(report))
        if (report && report.ok === true) {
            open = true
            console.log("[CLOSING] opened, forcing focus")
            forceActiveFocus()
        } else {
            console.log("[CLOSING] report generation failed")
            backend.toast("Could not generate closing report", "error")
        }
    }

    function close() {
        console.log("[CLOSING] close() called")
        open = false
    }

    opacity: open ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeOut } }

    Rectangle {
        anchors.fill: parent
        color: Theme.alpha(Theme.bg0, 0.6)
        TapHandler { onTapped: root.close() }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(640, root.width * 0.88)
        height: Math.min(600, root.height * 0.85)
        radius: Theme.rLg
        color: Theme.bg2
        border.width: 1
        border.color: Theme.glassBorder

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.s5
            spacing: Theme.s4

            // Title
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Today's Closing Report"
                    color: Theme.text; font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsSection; font.weight: Font.Bold
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: report.date || ""
                    color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall
                }
                Rectangle {
                    width: 28; height: 28; radius: 7; color: closeHover.hovered ? Theme.glassStrong : "transparent"
                    Text { anchors.centerIn: parent; text: "\u2715"; color: Theme.textDim; font.pixelSize: 13 }
                    HoverHandler { id: closeHover }
                    TapHandler { onTapped: root.close() }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.glassBorder }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: contentCol.implicitHeight
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
                    id: contentCol
                    width: parent.width
                    spacing: Theme.s4

                    // A. Cash Flow
                    Text { text: "A. Cash Flow"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Bold }
                    RowLayout { Layout.fillWidth: true; spacing: Theme.s2
                        Text { Layout.fillWidth: true; text: "Opening Cash (seed)"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                        Text { text: "\u20B9 " + backend.formatMoney(report.openingCash || 0); color: Theme.text; font.family: Theme.monoFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.DemiBold }
                    }
                    RowLayout { Layout.fillWidth: true; spacing: Theme.s2
                        Text { Layout.fillWidth: true; text: "Collections from Debtors (COL Cash)"; color: Theme.accent3; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                        Text { text: "+ \u20B9 " + backend.formatMoney(report.cashReceipts || 0); color: Theme.success; font.family: Theme.monoFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.DemiBold }
                    }
                    RowLayout { Layout.fillWidth: true; spacing: Theme.s2
                        Text { Layout.fillWidth: true; text: "Cash Sales Today"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                        Text { text: "+ \u20B9 " + backend.formatMoney(report.cashSales || 0); color: Theme.success; font.family: Theme.monoFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.DemiBold }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.glassBorderSoft }
                    RowLayout { Layout.fillWidth: true; spacing: Theme.s2
                        Text { Layout.fillWidth: true; text: "Closing Cash in Hand"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Bold }
                        Text { text: "\u20B9 " + backend.formatMoney(report.closingCash || 0); color: Theme.accent3; font.family: Theme.monoFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Bold }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.glassBorder; Layout.topMargin: Theme.s2 }

                    // B. UPI / Card
                    Text { text: "B. UPI / Card"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Bold }
                    RowLayout { Layout.fillWidth: true; spacing: Theme.s2
                        Text { Layout.fillWidth: true; text: "COL UPI Collections"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                        Text { text: "+ \u20B9 " + backend.formatMoney(report.upiReceipts || 0); color: Theme.success; font.family: Theme.monoFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.DemiBold }
                    }
                    RowLayout { Layout.fillWidth: true; spacing: Theme.s2
                        Text { Layout.fillWidth: true; text: "UPI Sales Today"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                        Text { text: "+ \u20B9 " + backend.formatMoney(report.upiSales || 0); color: Theme.success; font.family: Theme.monoFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.DemiBold }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.glassBorderSoft }
                    RowLayout { Layout.fillWidth: true; spacing: Theme.s2
                        Text { Layout.fillWidth: true; text: "Total UPI / Card Received"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.Bold }
                        Text { text: "\u20B9 " + backend.formatMoney(report.upiTotal || 0); color: Theme.accent2; font.family: Theme.monoFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.Bold }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.glassBorder; Layout.topMargin: Theme.s2 }

                    // C. Credit Sales
                    Text { text: "C. Credit Sales"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Bold }
                    RowLayout { Layout.fillWidth: true; spacing: Theme.s2
                        Text { Layout.fillWidth: true; text: "Credit Sales Today"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                        Text { text: "\u20B9 " + backend.formatMoney(report.creditSales || 0); color: Theme.warning; font.family: Theme.monoFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.DemiBold }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.glassBorder; Layout.topMargin: Theme.s2 }

                    // D. Grand Totals
                    Text { text: "D. Grand Total"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Bold }
                    RowLayout { Layout.fillWidth: true; spacing: Theme.s2
                        Text { Layout.fillWidth: true; text: "Total Collected Today"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.DemiBold }
                        Text { text: "\u20B9 " + backend.formatMoney((report.totalCollected || 0) + (report.openingCash || 0)); color: Theme.accent; font.family: Theme.monoFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Bold }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.glassBorder; Layout.topMargin: Theme.s2 }

                    // E. Recent Transactions
                    Text { text: "E. Today's Entries (" + ((report.receiptCount || 0) + (report.cashSaleCount || 0) + (report.upiSaleCount || 0) + (report.creditSaleCount || 0)) + ")"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.Bold }

                    Repeater {
                        model: report.recentTxns || []
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            height: 28
                            radius: Theme.rSm
                            color: index % 2 === 0 ? "transparent" : Theme.rowAlt
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: Theme.s3; anchors.rightMargin: Theme.s3
                                Text { Layout.preferredWidth: 60; text: "#" + (modelData.billNo || ""); color: Theme.textFaint; font.family: Theme.monoFamily; font.pixelSize: Theme.fsTiny; elide: Text.ElideRight }
                                Text { Layout.fillWidth: true; text: modelData.party || ""; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                Rectangle { Layout.preferredWidth: 60; height: 20; radius: Theme.rPill; color: Theme.alpha(Theme.typeColor(modelData.type || ""), 0.12); Text { anchors.centerIn: parent; text: modelData.type || ""; color: Theme.typeColor(modelData.type || ""); font.pixelSize: Theme.fsTiny; font.weight: Font.DemiBold } }
                                Rectangle { Layout.preferredWidth: 50; height: 20; radius: Theme.rPill; color: Theme.alpha(Theme.accent, 0.08); Text { anchors.centerIn: parent; text: modelData.mode || ""; color: Theme.accent; font.pixelSize: Theme.fsTiny } }
                                Text { Layout.preferredWidth: 80; text: "\u20B9 " + backend.formatMoney(Number(modelData.amount || 0)); color: modelData.mode === "Credit" ? Theme.warning : Theme.text; font.family: Theme.monoFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignRight }
                            }
                        }
                    }
                }
            }

            // Suggested bank deposit
            Rectangle {
                Layout.fillWidth: true
                height: 42
                radius: Theme.rMd
                color: Theme.alpha(Theme.accent3, 0.08)
                border.width: 1
                border.color: Theme.alpha(Theme.accent3, 0.2)
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: Theme.s4; anchors.rightMargin: Theme.s4
                    Text { text: "\u2139"; color: Theme.accent3; font.pixelSize: 14 }
                    Text { text: "Suggested bank deposit: \u20B9 " + backend.formatMoney(Math.max(0, (report.closingCash || 0) - 5000)); color: Theme.text; font.family: Theme.monoFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.DemiBold }
                    Text { text: "(keep \u20B9 5,000 in hand)"; color: Theme.textFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                    Item { Layout.fillWidth: true }
                }
            }
        }
    }

    Keys.onEscapePressed: root.close()
}
