import QtQuick
import MFinlogs

// Lightweight animated area+line sparkline drawn on a Canvas.
// `points` is an array of { date, value }.
Item {
    id: root
    property var points: []
    property color line: Theme.accent
    property color fill: Theme.alpha(Theme.accent, 0.18)

    // Animated reveal progress 0..1
    property real progress: 0
    onPointsChanged: { progress = 0; revealAnim.restart() }
    NumberAnimation {
        id: revealAnim
        target: root
        property: "progress"
        from: 0; to: 1
        duration: Theme.durSlow
        easing.type: Theme.easeOut
    }
    onProgressChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var n = root.points ? root.points.length : 0
            if (n < 2) return

            var maxV = 1
            for (var i = 0; i < n; i++) maxV = Math.max(maxV, Number(root.points[i].value) || 0)

            var pad = 6
            var w = width - pad * 2
            var h = height - pad * 2
            var count = Math.max(2, Math.floor(n * root.progress))

            function px(i) { return pad + (w * i / (n - 1)) }
            function py(v) { return pad + h - (h * (Number(v) || 0) / maxV) }

            // Area fill
            ctx.beginPath()
            ctx.moveTo(px(0), pad + h)
            for (var a = 0; a < count; a++) ctx.lineTo(px(a), py(root.points[a].value))
            ctx.lineTo(px(count - 1), pad + h)
            ctx.closePath()
            var grad = ctx.createLinearGradient(0, pad, 0, pad + h)
            grad.addColorStop(0, root.fill)
            grad.addColorStop(1, "transparent")
            ctx.fillStyle = grad
            ctx.fill()

            // Line
            ctx.beginPath()
            ctx.moveTo(px(0), py(root.points[0].value))
            for (var b = 1; b < count; b++) ctx.lineTo(px(b), py(root.points[b].value))
            ctx.strokeStyle = root.line
            ctx.lineWidth = 2.4
            ctx.lineJoin = "round"
            ctx.stroke()

            // Leading dot
            if (count >= 1) {
                ctx.beginPath()
                ctx.arc(px(count - 1), py(root.points[count - 1].value), 3.2, 0, Math.PI * 2)
                ctx.fillStyle = root.line
                ctx.fill()
            }
        }
    }
}
