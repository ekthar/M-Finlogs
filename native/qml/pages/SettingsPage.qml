import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: page
    property var userList: []

    Component.onCompleted: {
        openingField.text = backend.formatMoney(backend.openingCashSeed())
        if (backend.isAdmin) loadUsers()
    }
    function loadUsers() { userList = backend.users() }

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

            // -------- Opening cash (admin) --------
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

            // -------- User Management (admin) --------
            GlassPanel {
                Layout.fillWidth: true
                visible: backend.isAdmin
                implicitHeight: userCol.implicitHeight + Theme.s8
                radius: Theme.rLg
                ColumnLayout {
                    id: userCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.s5
                    spacing: Theme.s3
                    Text {
                        text: "User Management"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSection
                        font.weight: Font.Bold
                    }

                    // Create user form
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s3
                        FieldInput {
                            id: newUser
                            Layout.fillWidth: true
                            label: "Username"
                            placeholder: "New username"
                            showCompletions: false
                        }
                        FieldInput {
                            id: newPass
                            Layout.fillWidth: true
                            label: "Password"
                            placeholder: "Min 8 chars"
                            echoMode: TextInput.Password
                            showCompletions: false
                        }
                        FieldCombo {
                            id: newRole
                            Layout.preferredWidth: 160
                            label: "Role"
                            options: ["accounts", "admin"]
                        }
                        PrimaryButton {
                            Layout.alignment: Qt.AlignBottom
                            text: "Add User"
                            onClicked: {
                                var res = backend.createUser(newUser.text, newPass.text, newRole.currentText)
                                if (res && res.ok === true) {
                                    newUser.text = ""
                                    newPass.text = ""
                                    page.loadUsers()
                                }
                            }
                        }
                    }

                    // User table
                    DataTable {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(200, page.userList.length * 46 + 50)
                        emptyText: "No users"
                        rows: page.userList
                        columns: [
                            { title: "Username", key: "username", weight: 2 },
                            { title: "Role", key: "role", chip: true, weight: 1.2 }
                        ]
                    }
                }
            }

            // -------- Change Password (admin) --------
            GlassPanel {
                Layout.fillWidth: true
                visible: backend.isAdmin
                implicitHeight: pwCol.implicitHeight + Theme.s8
                radius: Theme.rLg
                ColumnLayout {
                    id: pwCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.s5
                    spacing: Theme.s3
                    Text {
                        text: "Change Password"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSection
                        font.weight: Font.Bold
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s3
                        FieldInput {
                            id: pwUser
                            Layout.fillWidth: true
                            label: "Username"
                            placeholder: "Username"
                            completions: backend.partyNames() // reuse for autocomplete
                            showCompletions: false
                        }
                        FieldInput {
                            id: pwNew
                            Layout.fillWidth: true
                            label: "New Password"
                            placeholder: "Min 8 chars"
                            echoMode: TextInput.Password
                            showCompletions: false
                        }
                        PrimaryButton {
                            Layout.alignment: Qt.AlignBottom
                            text: "Update"
                            onClicked: {
                                var res = backend.changePassword(pwUser.text, pwNew.text)
                                if (res && res.ok === true) {
                                    pwUser.text = ""
                                    pwNew.text = ""
                                }
                            }
                        }
                    }
                }
            }

            // -------- About --------
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
