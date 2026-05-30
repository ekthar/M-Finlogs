import QtQuick
import QtQuick.Effects
import MFinlogs

// Animated "aurora" backdrop: a base gradient with three slowly drifting,
// heavily-blurred light blobs. The drift animation and the live blur are
// gated on Theme.animationsEnabled so low-end systems / "animations off"
// get a cheap static gradient instead of continuous GPU work.
Item {
    id: root
    clip: true

    readonly property bool fancy: Theme.animationsEnabled

    // Base vertical gradient (always cheap)
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.bg0 }
            GradientStop { position: 0.5; color: Theme.bg1 }
            GradientStop { position: 1.0; color: Theme.bg2 }
        }
    }

    // Blob layer — only built when fancy mode is on.
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
                    color: Theme.alpha(Theme.grad0, Theme.dark ? 0.55 : 0.35)
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
                    color: Theme.alpha(Theme.grad1, Theme.dark ? 0.5 : 0.3)
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
                    color: Theme.alpha(Theme.grad2, Theme.dark ? 0.4 : 0.25)
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

    // Subtle vignette to settle the edges (cheap, static)
    Rectangle {
        anchors.fill: parent
        visible: Theme.dark
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: Theme.alpha(Theme.bg0, 0.55) }
        }
    }
}
