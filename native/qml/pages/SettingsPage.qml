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
                color: Theme.alpha(Theme.palette.fg, 0.04)
            }
        }

        ColumnLayout {
            id: col
            width: parent.width
            spacing: Theme.s5

            SectionHeader {
                title: "Settings"
                subtitle: "Appearance, database, backup, users, and runtime controls"
            }

            // ========== Appearance ==========
            GlassPanel {
                Layout.fillWidth: true
                implicitHeight: appCol.implicitHeight + Theme.s8
                radius: Theme.rLg
                ColumnLayout {
                    id: appCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.s5
                    spacing: Theme.s4
                    Text { text: "Appearance"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSection; font.weight: Font.Bold }

                    // Theme toggle row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s3
                        ColumnLayout {
                            spacing: 2
                            Text { text: "Theme"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.DemiBold }
                            Text { text: "Light is easier on the eyes in bright rooms"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                        }
                        Item { Layout.fillWidth: true }
                        // segmented Light / Dark control (pill shape via per-corner radius, Qt 6.7+)
                        Row {
                            spacing: 0
                            Rectangle {
                                width: 90; height: 36
                                topLeftRadius: Theme.rMd
                                bottomLeftRadius: Theme.rMd
                                topRightRadius: 0
                                bottomRightRadius: 0
                                color: !Theme.dark ? Theme.palette.primary : Theme.alpha(Theme.palette.fg, 0.04)
                                border.width: 1; border.color: Theme.palette.border
                                activeFocusOnTab: true
                                Accessible.role: Accessible.Button
                                Accessible.name: "Light theme"
                                Keys.onReturnPressed: Theme.dark = false
                                Keys.onSpacePressed: Theme.dark = false
                                FocusRing { visible: parent.activeFocus }
                                Text { anchors.centerIn: parent; text: "\u2600 Light"; color: !Theme.dark ? Theme.palette.primaryFg : Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.DemiBold }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Theme.dark = false }
                            }
                            Rectangle {
                                width: 90; height: 36
                                topLeftRadius: 0
                                bottomLeftRadius: 0
                                topRightRadius: Theme.rMd
                                bottomRightRadius: Theme.rMd
                                color: Theme.dark ? Theme.palette.primary : Theme.alpha(Theme.palette.fg, 0.04)
                                border.width: 1; border.color: Theme.palette.border
                                activeFocusOnTab: true
                                Accessible.role: Accessible.Button
                                Accessible.name: "Dark theme"
                                Keys.onReturnPressed: Theme.dark = true
                                Keys.onSpacePressed: Theme.dark = true
                                FocusRing { visible: parent.activeFocus }
                                Text { anchors.centerIn: parent; text: "\u263D Dark"; color: Theme.dark ? Theme.palette.primaryFg : Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall; font.weight: Font.DemiBold }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Theme.dark = true }
                            }
                        }
                    }

                    // Follow system theme toggle
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s3
                        ColumnLayout {
                            spacing: 2
                            Text { text: "Follow System Theme"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.DemiBold }
                            Text { text: "Automatically match your OS dark/light preference"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            width: 52; height: 30; radius: 15
                            color: Theme.followSystemTheme ? Theme.palette.primary : Theme.alpha(Theme.palette.fg, 0.04)
                            border.width: 1; border.color: Theme.followSystemTheme ? Theme.palette.primary : Theme.palette.border
                            Behavior on color { ColorAnimation { duration: Theme.durFast } }
                            activeFocusOnTab: true
                            Accessible.role: Accessible.CheckBox
                            Accessible.name: "Follow system theme"
                            Keys.onReturnPressed: Theme.followSystemTheme = !Theme.followSystemTheme
                            Keys.onSpacePressed: Theme.followSystemTheme = !Theme.followSystemTheme
                            Rectangle {
                                width: 24; height: 24; radius: 12
                                color: "#ffffff"
                                y: 3
                                x: Theme.followSystemTheme ? parent.width - width - 3 : 3
                                Behavior on x { NumberAnimation { duration: Theme.durFast; easing.type: Theme.easeOut } }
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Theme.followSystemTheme = !Theme.followSystemTheme }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.palette.borderSubtle }

                    // Animations toggle row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s3
                        ColumnLayout {
                            spacing: 2
                            Text { text: "Animations"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsBody; font.weight: Font.DemiBold }
                            Text { text: "Turn off for snappier performance on low-end systems"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                        }
                        Item { Layout.fillWidth: true }
                        // toggle switch
                        Rectangle {
                            width: 52; height: 30; radius: 15
                            color: Theme.animationsEnabled ? Theme.palette.primary : Theme.alpha(Theme.palette.fg, 0.04)
                            border.width: 1; border.color: Theme.animationsEnabled ? Theme.palette.primary : Theme.palette.border
                            Behavior on color { ColorAnimation { duration: Theme.durFast } }
                            activeFocusOnTab: true
                            Accessible.role: Accessible.CheckBox
                            Accessible.name: "Animations"
                            Keys.onReturnPressed: Theme.animationsEnabled = !Theme.animationsEnabled
                            Keys.onSpacePressed: Theme.animationsEnabled = !Theme.animationsEnabled
                            Rectangle {
                                    width: 24; height: 24; radius: 12
                                    color: "#ffffff"
                                    y: 3
                                    x: Theme.animationsEnabled ? parent.width - width - 3 : 3
                                    Behavior on x { NumberAnimation { duration: Theme.durFast; easing.type: Theme.easeOut } }
                                }

                                FocusRing { visible: parent.activeFocus }

                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Theme.animationsEnabled = !Theme.animationsEnabled }
                        }
                    }
                }
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

                    Text { text: "Database Configuration"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSection; font.weight: Font.Bold }

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

                    Text { id: dbStatus; visible: text.length > 0; text: ""; color: Theme.palette.primary; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; wrapMode: Text.WordWrap }

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
                        GhostButton { text: "Backup Now"; onClicked: {
                            var r = backend.backupDatabase(dbBackupDir.text)
                            if (r && r.ok) { dbStatus.text = "\u2713 Backup saved: " + (r.path || ""); dbStatus.color = Theme.success }
                            else { dbStatus.text = "\u2717 " + (r.error || "Backup failed"); dbStatus.color = Theme.danger }
                        } }
                        GhostButton { text: "Auto Backup"; onClicked: {
                            var r = backend.autoBackup()
                            if (r && r.ok) { dbStatus.text = "\u2713 Auto backup saved: " + (r.path || ""); dbStatus.color = Theme.success }
                            else { dbStatus.text = "\u2717 " + (r.error || "Backup failed"); dbStatus.color = Theme.danger }
                        } }
                        GhostButton { text: "Restore Backup"; tint: Theme.warning; onClicked: {
                            dbStatus.text = "Choose a backup file to restore..."
                            dbStatus.color = Theme.warning
                            var r = backend.restoreDatabase("")
                            if (r && r.ok === true && r.path) {
                                dbStatus.text = "\u2713 Restored from: " + r.path + " — restart the app."
                                dbStatus.color = Theme.success
                            } else if (r && r.ok === true) {
                                dbStatus.text = "Restore cancelled."
                                dbStatus.color = Theme.palette.fgMuted
                            } else {
                                dbStatus.text = "\u2717 " + (r.error || "Restore failed")
                                dbStatus.color = Theme.danger
                            }
                        } }
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
                    Text { text: "Native Server Controls"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSection; font.weight: Font.Bold }
                    Text { text: "Start/stop the native HTTP server for client mode"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                    RowLayout {
                        spacing: Theme.s3
                        PrimaryButton { text: "Start Server"; onClicked: backend.startNativeServer() }
                        GhostButton { text: "Stop Server"; onClicked: backend.stopNativeServer() }
                    }
                }
            }

            // ========== Data Import ==========
            GlassPanel {
                Layout.fillWidth: true
                implicitHeight: impCol.implicitHeight + Theme.s8
                radius: Theme.rLg
                ColumnLayout {
                    id: impCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.s5
                    spacing: Theme.s3
                    Text { text: "Data Import"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSection; font.weight: Font.Bold }
                    Text { text: "Import transactions from CSV or Excel (.xlsx) files"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
                    RowLayout {
                        spacing: Theme.s3
                        PrimaryButton {
                            text: "Import Transactions"
                            onClicked: backend.importTransactions()
                        }
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
                    Text { text: "Opening Cash (first day)"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSection; font.weight: Font.Bold }
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
                    Text { text: "User Management"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSection; font.weight: Font.Bold }

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
                    Text { text: "About M-Finlogs"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSection; font.weight: Font.Bold }
                    Text { text: "Native financial workspace with the Aurora design system."; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsSmall }
                    RowLayout {
                        spacing: 8
                        Text { text: "\u2728"; color: Theme.palette.primary; font.pixelSize: 14 }
                        Text { text: "Crafted by EKTHAR"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny }
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
