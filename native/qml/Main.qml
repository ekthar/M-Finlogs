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
    // Frameless premium window with custom title bar (removes the white OS bar)
    flags: Qt.Window | Qt.FramelessWindowHint

    // Living aurora backdrop behind everything
    AuroraBackground {
        anchors.fill: parent
    }

    // ---- Custom title bar (draggable, with window controls) ----
    Rectangle {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 38
        z: 10000
        color: Theme.alpha(Theme.bg0, 0.6)

        // Drag to move window
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onPressed: window.startSystemMove()
            onDoubleClicked: window.visibility === Window.Maximized
                ? window.showNormal() : window.showMaximized()
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10
            Rectangle {
                width: 20; height: 20; radius: 6
                anchors.verticalCenter: parent.verticalCenter
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.grad0 }
                    GradientStop { position: 1.0; color: Theme.grad1 }
                }
                Text { anchors.centerIn: parent; text: "M"; color: "#fff"; font.pixelSize: 11; font.weight: Font.Bold; font.family: Theme.fontFamily }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "M-Finlogs"
                color: Theme.textDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsSmall
                font.weight: Font.DemiBold
            }
        }

        // Window controls
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            // Minimize
            Rectangle {
                width: 46; height: 38
                color: minHover.hovered ? Theme.glass : "transparent"
                Text { anchors.centerIn: parent; text: "\u2013"; color: Theme.textDim; font.pixelSize: 14 }
                HoverHandler { id: minHover }
                TapHandler { onTapped: window.showMinimized() }
            }
            // Maximize/Restore
            Rectangle {
                width: 46; height: 38
                color: maxHover.hovered ? Theme.glass : "transparent"
                Text { anchors.centerIn: parent; text: "\u25A1"; color: Theme.textDim; font.pixelSize: 12 }
                HoverHandler { id: maxHover }
                TapHandler {
                    onTapped: window.visibility === Window.Maximized
                        ? window.showNormal() : window.showMaximized()
                }
            }
            // Close
            Rectangle {
                width: 46; height: 38
                color: closeHover.hovered ? Theme.danger : "transparent"
                Text { anchors.centerIn: parent; text: "\u2715"; color: closeHover.hovered ? "#fff" : Theme.textDim; font.pixelSize: 13 }
                HoverHandler { id: closeHover }
                TapHandler { onTapped: Qt.quit() }
            }
        }
    }

    // Root content switcher: login <-> app shell with a cross-fade
    Item {
        id: rootStack
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

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
