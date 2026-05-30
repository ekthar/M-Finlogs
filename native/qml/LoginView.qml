import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: root

    property bool setupMode: backend.setupRequired
    property bool busy: false
    property bool showDbConfig: false

    // Center the glass auth card
    GlassPanel {
        id: card
        width: 420
        anchors.centerIn: parent
        visible: !root.showDbConfig
        height: cardCol.implicitHeight + Theme.s10
        fillColor: Theme.glassStrong
        radius: Theme.rXl

        opacity: 0
        scale: 0.96
        Component.onCompleted: { opacity = 1; scale = 1 }
        Behavior on opacity { NumberAnimation { duration: Theme.durSlow; easing.type: Theme.easeOut } }
        Behavior on scale { NumberAnimation { duration: Theme.durSlow; easing.type: Theme.easeSpring } }

        ColumnLayout {
            id: cardCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.s8
            spacing: Theme.s4

            // Brand mark
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 60; height: 60; radius: 18
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.grad0 }
                    GradientStop { position: 1.0; color: Theme.grad1 }
                }
                Text { anchors.centerIn: parent; text: "M"; color: "#fff"; font.pixelSize: 28; font.weight: Font.Bold; font.family: Theme.fontFamily }
                SequentialAnimation on scale {
                    loops: Animation.Infinite
                    NumberAnimation { to: 1.05; duration: 1600; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0; duration: 1600; easing.type: Easing.InOutSine }
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: root.setupMode ? "Create your admin account" : "Welcome back"
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsTitle
                font.weight: Font.Bold
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: Theme.s2
                text: root.setupMode ? "Set a password to secure M-Finlogs" : "Sign in to your financial workspace"
                color: Theme.textDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsSmall
            }

            FieldInput {
                id: userField
                Layout.fillWidth: true
                label: "Username"
                placeholder: "admin"
                showCompletions: false
                text: root.setupMode ? "admin" : ""
                onAccepted: passField.inputField.forceActiveFocus()
            }
            FieldInput {
                id: passField
                Layout.fillWidth: true
                label: "Password"
                placeholder: "Enter your password"
                echoMode: TextInput.Password
                showCompletions: false
                onAccepted: root.submit()
            }

            Text {
                id: errorText
                Layout.fillWidth: true
                visible: text.length > 0
                text: ""
                color: Theme.danger
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsTiny
                wrapMode: Text.WordWrap
            }

            PrimaryButton {
                Layout.fillWidth: true
                Layout.topMargin: Theme.s1
                text: root.busy ? "Please wait..." : (root.setupMode ? "Create account & sign in" : "Sign in")
                enabled: !root.busy
                onClicked: root.submit()
            }

            // DB Config link
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Theme.s2
                text: "\u2699 Configure Database"
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsTiny
                font.weight: Font.DemiBold
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.showDbConfig = true
                }
            }

            SequentialAnimation {
                id: shake
                loops: 3
                NumberAnimation { target: card; property: "anchors.horizontalCenterOffset"; to: -6; duration: 50 }
                NumberAnimation { target: card; property: "anchors.horizontalCenterOffset"; to: 6; duration: 50 }
                NumberAnimation { target: card; property: "anchors.horizontalCenterOffset"; to: 0; duration: 50 }
            }
        }
    }

    // ===================== DATABASE CONFIG DIALOG =========================
    GlassPanel {
        id: dbCard
        width: 500
        anchors.centerIn: parent
        visible: root.showDbConfig
        height: dbCol.implicitHeight + Theme.s10
        fillColor: Theme.glassStrong
        radius: Theme.rXl

        ColumnLayout {
            id: dbCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.s8
            spacing: Theme.s3

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Database Configuration"
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsTitle
                font.weight: Font.Bold
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: Theme.s2
                text: "Configure SQL Server connection"
                color: Theme.textDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsSmall
            }

            FieldInput {
                id: dbServer
                Layout.fillWidth: true
                label: "SQL Server Instance"
                placeholder: "localhost or SERVERNAME\\SQLEXPRESS"
                showCompletions: false
            }
            FieldInput {
                id: dbName
                Layout.fillWidth: true
                label: "Database Name"
                placeholder: "M_Finlogs_Accounts"
                showCompletions: false
            }
            FieldCombo {
                id: dbAuthType
                Layout.fillWidth: true
                label: "Authentication"
                options: ["Windows Authentication", "SQL Server Authentication"]
            }
            FieldInput {
                id: dbUsername
                Layout.fillWidth: true
                label: "SQL Username"
                placeholder: "sa"
                visible: dbAuthType.currentIndex === 1
                showCompletions: false
            }
            FieldInput {
                id: dbPassword
                Layout.fillWidth: true
                label: "SQL Password"
                placeholder: "Password"
                echoMode: TextInput.Password
                visible: dbAuthType.currentIndex === 1
                showCompletions: false
            }
            FieldInput {
                id: dbBackupDir
                Layout.fillWidth: true
                label: "Backup Folder (UNC path)"
                placeholder: "\\\\SERVER\\Backups"
                showCompletions: false
            }

            Text {
                id: dbStatus
                Layout.fillWidth: true
                visible: text.length > 0
                text: ""
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsTiny
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.s3
                PrimaryButton {
                    Layout.fillWidth: true
                    text: "Test Connection"
                    from: Theme.success
                    to: "#059669"
                    onClicked: {
                        dbStatus.text = "Testing..."
                        dbStatus.color = Theme.warning
                        var cfg = root.buildDbConfig()
                        var res = backend.testDatabaseConfig(cfg)
                        if (res && res.success === true) {
                            dbStatus.text = "\u2713 Connection successful!"
                            dbStatus.color = Theme.success
                        } else {
                            dbStatus.text = "\u2717 " + (res.error || "Connection failed")
                            dbStatus.color = Theme.danger
                        }
                    }
                }
                PrimaryButton {
                    Layout.fillWidth: true
                    text: "Save & Apply"
                    onClicked: {
                        var cfg = root.buildDbConfig()
                        var res = backend.saveDatabaseConfig(cfg)
                        if (res && res.ok === true) {
                            dbStatus.text = "\u2713 Saved! Restart app to apply."
                            dbStatus.color = Theme.success
                        } else {
                            dbStatus.text = "\u2717 " + (res.error || "Save failed")
                            dbStatus.color = Theme.danger
                        }
                    }
                }
            }

            // ─── Mode Selection ───────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.glassBorder
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Theme.s2
                text: "Connection Mode"
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsSection
                font.weight: Font.Bold
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Current: " + backend.currentMode().toUpperCase()
                color: Theme.textDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsTiny
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.s3

                PrimaryButton {
                    Layout.fillWidth: true
                    text: "\uD83C\uDF10 Server Mode"
                    from: Theme.accent
                    to: Theme.accent2
                    onClicked: {
                        var res = backend.selectMode("server")
                        if (res && res.ok) {
                            dbStatus.text = "\u2713 Set to Server mode. Restart to apply."
                            dbStatus.color = Theme.success
                        }
                    }
                }
                PrimaryButton {
                    Layout.fillWidth: true
                    text: "\uD83D\uDCBB Local Mode"
                    from: Theme.accent3
                    to: Theme.success
                    onClicked: {
                        var res = backend.selectMode("local")
                        if (res && res.ok) {
                            dbStatus.text = "\u2713 Set to Local (SQLite) mode. Restart to apply."
                            dbStatus.color = Theme.success
                        }
                    }
                }
            }

            PrimaryButton {
                Layout.fillWidth: true
                text: "\uD83D\uDCE5 Migrate: Server \u2192 Local"
                danger: false
                from: Theme.warning
                to: "#d97706"
                onClicked: {
                    dbStatus.text = "Migrating all data from SQL Server to local SQLite..."
                    dbStatus.color = Theme.warning
                    var res = backend.migrateServerToLocal()
                    if (res && res.ok === true) {
                        dbStatus.text = "\u2713 Migration complete! All data copied. Restart to use local mode."
                        dbStatus.color = Theme.success
                    } else {
                        dbStatus.text = "\u2717 Migration failed: " + (res.error || "Unknown error")
                        dbStatus.color = Theme.danger
                    }
                }
            }

            GhostButton {
                Layout.fillWidth: true
                text: "\u2190 Back to Login"
                onClicked: root.showDbConfig = false
            }
        }

        Component.onCompleted: {
            var cfg = backend.readDatabaseConfig()
            if (cfg && !cfg.error) {
                dbServer.text = cfg.server || ""
                dbName.text = cfg.database || ""
                dbAuthType.currentIndex = cfg.useWindowsAuth === false ? 1 : 0
                dbUsername.text = cfg.username || ""
                dbBackupDir.text = cfg.backupDir || ""
            }
        }
    }

    function buildDbConfig() {
        return {
            server: dbServer.text,
            database: dbName.text,
            useWindowsAuth: dbAuthType.currentIndex === 0,
            username: dbUsername.text,
            password: dbPassword.text,
            backupDir: dbBackupDir.text,
            driver: ""
        }
    }

    function submit() {
        if (busy) return
        errorText.text = ""
        busy = true
        var res
        if (setupMode) {
            res = backend.setupAdmin(userField.text, passField.text)
        } else {
            res = backend.login(userField.text, passField.text)
        }
        busy = false
        if (!res || res.ok !== true) {
            errorText.text = res && res.error ? res.error : "Sign in failed"
            shake.restart()
        }
    }
}
