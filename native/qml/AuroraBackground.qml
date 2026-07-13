import QtQuick
import QtQuick.Effects
import MFinlogs

Item {
    id: root
    clip: true

    readonly property bool fancy: Theme.animationsEnabled

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.palette.bg }
            GradientStop { position: 0.5; color: Theme.palette.bgSubtle }
            GradientStop { position: 1.0; color: Theme.palette.bgMuted }
        }
    }

    Loader {
        anchors.fill: parent
        active: root.fancy
        sourceComponent: blobComponent
    }

    Component {
        id: blobComponent
        Item {
            anchors.fill: parent

            Item {
                id: blobLayer
                anchors.fill: parent
                visible: false

                Rectangle {
                    id: blobA
                    width: Math.max(420, root.width * 0.5)
                    height: width
                    radius: width / 2
                    color: Theme.alpha(Theme.palette.primary, Theme.dark ? 0.35 : 0.08)
                    x: -width * 0.2
                    y: -height * 0.25
                    SequentialAnimation on x {
                        loops: Animation.Infinite
                        NumberAnimation { to: root.width * 0.18; duration: 12000; easing.type: Easing.InOutSine }
                        NumberAnimation { to: -blobA.width * 0.2; duration: 12000; easing.type: Easing.InOutSine }
                    }
                    SequentialAnimation on y {
                        loops: Animation.Infinite
                        NumberAnimation { to: root.height * 0.1; duration: 14000; easing.type: Easing.InOutSine }
                        NumberAnimation { to: -blobA.height * 0.25; duration: 14000; easing.type: Easing.InOutSine }
                    }
                }

                Rectangle {
                    id: blobB
                    width: Math.max(380, root.width * 0.45)
                    height: width
                    radius: width / 2
                    color: Theme.alpha(Theme.palette.info, Theme.dark ? 0.25 : 0.06)
                    x: root.width * 0.55
                    y: -height * 0.15
                    SequentialAnimation on x {
                        loops: Animation.Infinite
                        NumberAnimation { to: root.width * 0.4; duration: 16000; easing.type: Easing.InOutSine }
                        NumberAnimation { to: root.width * 0.6; duration: 16000; easing.type: Easing.InOutSine }
                    }
                    SequentialAnimation on y {
                        loops: Animation.Infinite
                        NumberAnimation { to: root.height * 0.2; duration: 13000; easing.type: Easing.InOutSine }
                        NumberAnimation { to: -blobB.height * 0.15; duration: 13000; easing.type: Easing.InOutSine }
                    }
                }

                Rectangle {
                    id: blobC
                    width: Math.max(400, root.width * 0.42)
                    height: width
                    radius: width / 2
                    color: Theme.alpha(Theme.palette.primary, Theme.dark ? 0.20 : 0.05)
                    x: root.width * 0.2
                    y: root.height * 0.55
                    SequentialAnimation on x {
                        loops: Animation.Infinite
                        NumberAnimation { to: root.width * 0.45; duration: 18000; easing.type: Easing.InOutSine }
                        NumberAnimation { to: root.width * 0.1; duration: 18000; easing.type: Easing.InOutSine }
                    }
                    SequentialAnimation on y {
                        loops: Animation.Infinite
                        NumberAnimation { to: root.height * 0.4; duration: 15000; easing.type: Easing.InOutSine }
                        NumberAnimation { to: root.height * 0.62; duration: 15000; easing.type: Easing.InOutSine }
                    }
                }
            }

            MultiEffect {
                source: blobLayer
                anchors.fill: blobLayer
                blurEnabled: true
                blur: 1.0
                blurMax: 64
                opacity: Theme.dark ? 0.75 : 0.55
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: Theme.dark
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: Theme.alpha(Theme.palette.bg, 0.55) }
        }
    }
}
