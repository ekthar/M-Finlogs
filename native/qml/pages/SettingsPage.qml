import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page

    Component.onCompleted: openingField.text = backend.formatMoney(backend.openingCashSeed())

    Flickable {
        anchors.fill: parent
        anchors.leftMargin: Theme.s8
        anchors.rightMargin: Theme.s8
        anchors.topMargin: Theme.s5
        contentWidth: width
        contentHeight: col.implicitHeight + Theme.s8
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: col
            width: parent.width
            spacing: Theme.s5

            SectionHeader {
                title: "Settings"
                subtitle: "Preferences and admin configuration"
            }

            // Opening cash (admin only)
            GlassPanel {
                Layout.fillWidth: true
                visible: backend.isAdmin
                implicitHeight: ocCol.implicitHeight + Theme.s8
                radius: Theme.rLg
                ColumnLayout {
                    id: ocCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.s5
                    spacing: Theme.s3
                    Text {
                        text: "Opening Cash (first day)"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSection
                        font.weight: Font.Bold
                    }
                    Text {
                        text: "Used as the opening cash balance for the first day of the ledger."
                        color: Theme.textDim
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsTiny
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s3
                        FieldInput {
                            id: openingField
                            Layout.preferredWidth: 240
                            numeric: true
                            showCompletions: false
                            placeholder: "0.00"
                        }
                        PrimaryButton {
                            text: "Save"
                            onClicked: backend.saveOpeningCashSeed(Number(String(openingField.text).replace(/,/g, "")))
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // About
            GlassPanel {
                Layout.fillWidth: true
                implicitHeight: aboutCol.implicitHeight + Theme.s8
                radius: Theme.rLg
                ColumnLayout {
                    id: aboutCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.s5
                    spacing: Theme.s2
                    Text {
                        text: "About M-Finlogs"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSection
                        font.weight: Font.Bold
                    }
                    Text {
                        text: "Native financial workspace with the Aurora design system."
                        color: Theme.textDim
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSmall
                    }
                    RowLayout {
                        spacing: 8
                        Text { text: "\u2728"; color: Theme.accent; font.pixelSize: 14 }
                        Text {
                            text: "Crafted by EKTHAR"
                            color: Theme.textDim
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsTiny
                        }
                    }
                }
            }
        }
    }
}
