import QtQuick
import QtQuick.Window
import QtQuick.Controls.Basic
import MFinlogs

ApplicationWindow {
    id: window
    width: 1360
    height: 860
    minimumWidth: 1080
    minimumHeight: 680
    visible: true
    title: "M-Finlogs"
    color: Theme.bg0

    // Living aurora backdrop behind everything
    AuroraBackground {
        anchors.fill: parent
    }

    // Root content switcher: login <-> app shell with a cross-fade
    Item {
        id: rootStack
        anchors.fill: parent

        LoginView {
            id: loginView
            anchors.fill: parent
            visible: opacity > 0
            opacity: backend.authenticated ? 0 : 1
            scale: backend.authenticated ? 1.04 : 1.0
            enabled: !backend.authenticated
            Behavior on opacity { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeOut } }
            Behavior on scale { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeOut } }
        }

        Loader {
            id: shellLoader
            anchors.fill: parent
            active: backend.authenticated
            visible: opacity > 0
            opacity: backend.authenticated ? 1 : 0
            scale: backend.authenticated ? 1.0 : 0.98
            Behavior on opacity { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeOut } }
            Behavior on scale { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeOut } }
            sourceComponent: AppShell {}
        }
    }

    // Global toast host
    ToastHost {
        id: toasts
        anchors.fill: parent
    }

    Connections {
        target: backend
        function onToast(message, kind) {
            toasts.show(message, kind)
        }
    }
}
