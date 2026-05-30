import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: page
    property var userList: []
    property var dbConfig: ({})

    Component.onCompleted: {
        openingField.text = backend.formatMoney(backend.openingCashSeed())
        if (backend.isAdmin) loadUsers()
        dbConfig = backend.readDatabaseConfig()
        if (dbConfig && !dbConfig.error) {
            dbServer.text = dbConfig.server || ""
            dbDatabase.text = dbConfig.database || ""
            dbAuthType.currentIndex = dbConfig.useWindowsAuth === false ? 1 : 0
            dbUsername.text = dbConfig.username || ""
            dbBackupDir.text = dbConfig.backupDir || ""
            dbApiBase.text = dbConfig.apiBaseUrl || ""
        }
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
                subtitle: "Database, backup, users, and runtime controls"
            }

            // ========== Database Configuration ==========
            GlassPanel {
                Layout.fillWidth: true
                implicitHeight: dbCol.implicitHeight + Theme.s8
                radius: Theme.rLg
                ColumnLayout {
                    id: dbCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.s5
                    spacing: Theme.s3

                    Text { text: "Database Configuration"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSection; font.weight: Font.Bold }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: Theme.s3
                        rowSpacing: Theme.s2
                        FieldInput { id: dbServer; Layout.fillWidth: true; label: "SQL Server Instance"; placeholder: "localhost"; showCompletions: false }
                        FieldInput { id: dbDatabase; Layout.fillWidth: true; label: "Database Name"; placeholder: "Finlogs"; showCompletions: false }
                        FieldCombo { id: dbAuthType; Layout.fillWidth: true; label: "Authentication"; options: ["Windows Authentication", "SQL Server Authentication"] }
                        FieldInput { id: dbUsername; Layout.fillWidth: true; label: "SQL Username"; placeholder: "sa"; showCompletions: false; visible: dbAuthType.currentIndex === 1 }
                        FieldInput { id: dbPassword; Layout.fillWidth: true; label: "SQL Password"; placeholder: "Password"; echoMode: TextInput.Password; showCompletions: false; visible: dbAuthType.currentIndex === 1 }
                        FieldInput { id: dbApiBase; Layout.fillWidth: true; label: "API Base URL"; placeholder: "http://127.0.0.1:8000"; showCompletions: false }
                        FieldInput { id: dbBackupDir; Layout.fillWidth: true; Layout.columnSpan: 2; label: "Backup Directory"; placeholder: "D:/finlogs"; showCompletions: false }
                    }

                    Text { id: dbStatus; visible: text.length > 0; text: ""; color: Theme.accent; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; wrapMode: Text.WordWrap }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s3
                        PrimaryButton {
                            text: "Test Connection"
                            from: Theme.success; to: "#059669"
                            onClicked: {
                                dbStatus.text = "Testing..."
                                dbStatus.color = Theme.warning
                                var cfg = page.buildDbConfig()
                                var res = backend.testDatabaseConfig(cfg)
                                if (res && res.ok === true) {
                                    dbStatus.text = "\u2713 Connection successful!"
                                    dbStatus.color = Theme.success
                                } else {
                                    dbStatus.text = "\u2717 " + (res.error || "Connection failed")
                                    dbStatus.color = Theme.danger
                                }
                            }
                        }
                        PrimaryButton {
                            text: "Save Config"
                            onClicked: {
                                var cfg = page.buildDbConfig()
                                var res = backend.saveDatabaseConfig(cfg)
                                if (res && res.ok === true) {
                                    dbStatus.text = "\u2713 Saved! Restart to apply."
                                    dbStatus.color = Theme.success
                                } else {
                                    dbStatus.text = "\u2717 " + (res.error || "Save failed")
                                    dbStatus.color = Theme.danger
                                }
                            }
                        }
                        GhostButton { text: "Backup Now"; onClicked: backend.backupDatabase(dbBackupDir.text) }
                        GhostButton { text: "Auto Backup"; onClicked: backend.autoBackup() }
                        GhostButton { text: "Restore Backup"; onClicked: backend.restoreDatabase("") }
                    }
                }
            }

            // ========== Server Controls ==========
            GlassPanel {
                Layout.fillWidth: true
                implicitHeight: serverCol.implicitHeight + Theme.s8
                radius: Theme.rLg
                ColumnLayout {
                    id: serverCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.s5
                    spacing: Theme.s3
                    Text { text: "Native Server Controls"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSection; font.weight: Font.Bold }
                    Text { text: "Start/stop the native HTTP server for client mode"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                    RowLayout {
                        spacing: Theme.s3
                        PrimaryButton { text: "Start Server"; onClicked: backend.startNativeServer() }
                        GhostButton { text: "Stop Server"; onClicked: backend.stopNativeServer() }
                    }
                }
            }

            // ========== Opening Cash ==========
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
                    Text { text: "Opening Cash (first day)"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSection; font.weight: Font.Bold }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s3
                        FieldInput { id: openingField; Layout.preferredWidth: 240; numeric: true; showCompletions: false; placeholder: "0.00" }
                        PrimaryButton { text: "Save"; onClicked: backend.saveOpeningCashSeed(Number(String(openingField.text).replace(/,/g, ""))) }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // ========== User Management (admin) ==========
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
                    Text { text: "User Management"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSection; font.weight: Font.Bold }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s3
                        FieldInput { id: newUser; Layout.fillWidth: true; label: "Username"; placeholder: "New username"; showCompletions: false }
                        FieldInput { id: newPass; Layout.fillWidth: true; label: "Password"; placeholder: "Min 8 chars"; echoMode: TextInput.Password; showCompletions: false }
                        FieldCombo { id: newRole; Layout.preferredWidth: 160; label: "Role"; options: ["accounts", "admin"] }
                        PrimaryButton {
                            Layout.alignment: Qt.AlignBottom
                            text: "Create User"
                            onClicked: {
                                var res = backend.createUser(newUser.text, newPass.text, newRole.currentText)
                                if (res && res.ok === true) { newUser.text = ""; newPass.text = ""; page.loadUsers() }
                            }
                        }
                    }

                    // Change password
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s3
                        FieldInput { id: pwUser; Layout.fillWidth: true; label: "Change Password - Username"; placeholder: "Username"; showCompletions: false }
                        FieldInput { id: pwNew; Layout.fillWidth: true; label: "New Password"; placeholder: "Min 8 chars"; echoMode: TextInput.Password; showCompletions: false }
                        PrimaryButton {
                            Layout.alignment: Qt.AlignBottom
                            text: "Update Password"
                            onClicked: {
                                var res = backend.changePassword(pwUser.text, pwNew.text)
                                if (res && res.ok === true) { pwUser.text = ""; pwNew.text = "" }
                            }
                        }
                    }

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
                    GhostButton { text: "Refresh Users"; onClicked: page.loadUsers() }
                }
            }

            // ========== About ==========
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
                    Text { text: "About M-Finlogs"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSection; font.weight: Font.Bold }
                    Text { text: "Native financial workspace with the Aurora design system."; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                    RowLayout {
                        spacing: 8
                        Text { text: "\u2728"; color: Theme.accent; font.pixelSize: 14 }
                        Text { text: "Crafted by EKTHAR"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                    }
                }
            }
        }
    }

    function buildDbConfig() {
        return {
            server: dbServer.text,
            database: dbDatabase.text,
            useWindowsAuth: dbAuthType.currentIndex === 0,
            username: dbUsername.text,
            password: dbPassword.text,
            backupDir: dbBackupDir.text,
            apiBaseUrl: dbApiBase.text,
            driver: ""
        }
    }
}
