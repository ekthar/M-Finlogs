import QtQuick
import QtQuick.Effects
import MFinlogs

// Animated "aurora" backdrop: a deep indigo base with three slowly drifting,
// heavily-blurred light blobs that create the premium living-gradient feel.
Item {
    id: root
    clip: true

    // Base vertical gradient
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.bg0 }
            GradientStop { position: 0.5; color: Theme.bg1 }
            GradientStop { position: 1.0; color: Theme.bg2 }
        }
    }

    // Blob layer (blurred via MultiEffect). Kept in a sized Item so the blur
    // has well-defined bounds.
    Item {
        id: blobLayer
        anchors.fill: parent
        visible: false

        Rectangle {
            id: blobA
            width: Math.max(420, root.width * 0.5)
            height: width
            radius: width / 2
            color: Theme.alpha(Theme.grad0, 0.55)
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
            color: Theme.alpha(Theme.grad1, 0.5)
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
            color: Theme.alpha(Theme.grad2, 0.4)
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
        opacity: 0.75
    }

    // Subtle vignette to settle the edges
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: Theme.alpha(Theme.bg0, 0.55) }
        }
    }

    // Fine grain texture using a faint grid to add a "designed" feel
    Canvas {
        anchors.fill: parent
        opacity: 0.04
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = "#ffffff"
            ctx.lineWidth = 1
            var step = 32
            for (var x = 0; x < width; x += step) {
                ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke()
            }
            for (var y = 0; y < height; y += step) {
                ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
            }
        }
    }
}
