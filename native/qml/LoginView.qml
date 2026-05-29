import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: root

    property bool setupMode: backend.setupRequired
    property bool busy: false

    // Center the glass auth card
    GlassPanel {
        id: card
        width: 420
        anchors.centerIn: parent
        height: cardCol.implicitHeight + Theme.s10
        fillColor: Theme.glassStrong
        radius: Theme.rXl

        // Entrance (opacity + scale; anchors keep it centered)
        opacity: 0
        scale: 0.96
        Component.onCompleted: {
            opacity = 1
            scale = 1
        }
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

                // Soft pulsing glow
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

            // Shake animation on error
            SequentialAnimation {
                id: shake
                loops: 3
                NumberAnimation { target: card; property: "anchors.horizontalCenterOffset"; to: -6; duration: 50 }
                NumberAnimation { target: card; property: "anchors.horizontalCenterOffset"; to: 6; duration: 50 }
                NumberAnimation { target: card; property: "anchors.horizontalCenterOffset"; to: 0; duration: 50 }
            }
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
