import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: page
    property var rows: []

    function load() { rows = backend.parties() }
    Component.onCompleted: load()
    Connections { target: backend; function onDataChanged() { page.load() } }

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
                title: "Parties"
                subtitle: "Manage customers, suppliers, banks and expense accounts"
            }

            // -------- Create party --------
            GlassPanel {
                Layout.fillWidth: true
                implicitHeight: createCol.implicitHeight + Theme.s8
                radius: Theme.rLg
                ColumnLayout {
                    id: createCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.s5
                    spacing: Theme.s3

                    Text {
                        text: "Add New Party"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSection
                        font.weight: Font.Bold
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s3
                        FieldInput {
                            id: nameField
                            Layout.fillWidth: true
                            label: "Party Name"
                            placeholder: "e.g. Acme Traders"
                            showCompletions: false
                            onAccepted: createBtn.clicked()
                        }
                        FieldCombo {
                            id: typeField
                            Layout.preferredWidth: 200
                            label: "Type"
                            options: ["Customer", "Credit Customer", "Supplier", "Expense Account", "Bank"]
                        }
                        FieldCombo {
                            id: creditField
                            Layout.preferredWidth: 140
                            label: "Allow Credit"
                            options: ["No", "Yes"]
                        }
                        PrimaryButton {
                            id: createBtn
                            Layout.alignment: Qt.AlignBottom
                            Layout.preferredWidth: 140
                            text: "Create"
                            onClicked: {
                                var res = backend.createParty(nameField.text, typeField.currentText, creditField.currentIndex === 1)
                                if (res && res.ok === true) {
                                    nameField.text = ""
                                    creditField.currentIndex = 0
                                }
                            }
                        }
                    }
                }
            }

            // -------- Rename party (admin only) --------
            GlassPanel {
                Layout.fillWidth: true
                visible: backend.isAdmin
                implicitHeight: renameCol.implicitHeight + Theme.s8
                radius: Theme.rLg
                ColumnLayout {
                    id: renameCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.s5
                    spacing: Theme.s3

                    Text {
                        text: "Rename Party (Admin)"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSection
                        font.weight: Font.Bold
                    }
                    Text {
                        text: "This will rename the party across all historical transactions."
                        color: Theme.textDim
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsTiny
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s3
                        FieldInput {
                            id: oldNameField
                            Layout.fillWidth: true
                            label: "Current Name"
                            placeholder: "Search party to rename"
                            completions: backend.partyNames()
                        }
                        FieldInput {
                            id: newNameField
                            Layout.fillWidth: true
                            label: "New Name"
                            placeholder: "Enter new name"
                            showCompletions: false
                            onAccepted: renameBtn.clicked()
                        }
                        PrimaryButton {
                            id: renameBtn
                            Layout.alignment: Qt.AlignBottom
                            Layout.preferredWidth: 140
                            danger: true
                            text: "Rename"
                            onClicked: {
                                var res = backend.renameParty(oldNameField.text, newNameField.text)
                                if (res && res.ok === true) {
                                    oldNameField.text = ""
                                    newNameField.text = ""
                                }
                            }
                        }
                    }
                }
            }

            // -------- Party list --------
            GlassPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 400
                radius: Theme.rLg
                DataTable {
                    anchors.fill: parent
                    anchors.margins: Theme.s5
                    emptyText: "No parties yet \u2014 create one above"
                    rows: page.rows
                    columns: [
                        { title: "Name", key: "name", weight: 2.4 },
                        { title: "Type", key: "type", chip: true, weight: 1.4 }
                    ]
                }
            }
        }
    }
}
