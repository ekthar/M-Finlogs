import QtQuick
import QtQuick.Window
import QtQuick.Controls.Basic
import MFinlogs

ApplicationWindow {
    id: window
    width: 1360
    height: 860
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "M-Finlogs"
    color: Theme.palette.bg
    flags: Qt.Window | Qt.FramelessWindowHint

    AuroraBackground {
        anchors.fill: parent
    }

            Rectangle {
                id: titleBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 38
                z: 10000
                color: Theme.alpha(Theme.palette.bg, 0.75)
                Accessible.role: Accessible.TitleBar
                Accessible.name: "Window title bar"

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
                color: Theme.palette.primary
                Text { anchors.centerIn: parent; text: "M"; color: Theme.palette.primaryFg; font.pixelSize: 11; font.weight: Font.Bold; font.family: Theme.fontFamily }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "M-Finlogs"
                color: Theme.palette.fgMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsSmall
                font.weight: Theme.wSemibold
            }
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            Rectangle {
                width: 46; height: 38
                color: minHover.hovered ? Theme.alpha(Theme.palette.fg, 0.06) : "transparent"
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: "Minimize"
                Keys.onReturnPressed: window.showMinimized()
                Keys.onSpacePressed: window.showMinimized()
                Text { anchors.centerIn: parent; text: "\u2013"; color: Theme.palette.fgMuted; font.pixelSize: 14 }
                HoverHandler { id: minHover }
                TapHandler { onTapped: window.showMinimized() }
            }
            Rectangle {
                width: 46; height: 38
                color: maxHover.hovered ? Theme.alpha(Theme.palette.fg, 0.06) : "transparent"
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: "Maximize"
                Keys.onReturnPressed: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
                Keys.onSpacePressed: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
                Text { anchors.centerIn: parent; text: "\u25A1"; color: Theme.palette.fgMuted; font.pixelSize: 12 }
                HoverHandler { id: maxHover }
                TapHandler {
                    onTapped: window.visibility === Window.Maximized
                        ? window.showNormal() : window.showMaximized()
                }
            }
            Rectangle {
                width: 46; height: 38
                color: closeHover.hovered ? Theme.palette.danger : "transparent"
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: "Close"
                Keys.onReturnPressed: Qt.quit()
                Keys.onSpacePressed: Qt.quit()
                Text { anchors.centerIn: parent; text: "\u2715"; color: closeHover.hovered ? "#fff" : Theme.palette.fgMuted; font.pixelSize: 13 }
                HoverHandler { id: closeHover }
                TapHandler { onTapped: Qt.quit() }
            }
        }
    }

    Item {
        id: rootStack
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        Loader {
            id: loginLoader
            anchors.fill: parent
            active: !backend.authenticated
            visible: opacity > 0
            opacity: backend.authenticated ? 0 : 1
            scale: backend.authenticated ? 1.04 : 1.0
            Behavior on opacity { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeOut } }
            Behavior on scale { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeOut } }
            sourceComponent: LoginView {}
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

    Rectangle {
        id: fadeOverlay
        anchors.fill: parent
        color: Theme.dark ? "#000000" : "#ffffff"
        opacity: 0
        visible: opacity > 0.01
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    }

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

    Connections {
        target: Theme
        function onDarkChanged() {
            fadeOverlay.opacity = 0.3
            crossfadeTimer.restart()
        }
    }

    Connections {
        target: backend
        function onAuthChanged() {}
        function onSetupChanged() {}
        function onDataChanged() {}
    }

    Timer {
        id: crossfadeTimer
        interval: 80
        onTriggered: fadeOverlay.opacity = 0
    }
}
