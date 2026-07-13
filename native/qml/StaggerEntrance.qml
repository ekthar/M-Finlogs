import QtQuick
import MFinlogs

// Wraps a visual child and staggers its entrance by `index * baseDelay` ms.
// Use for dashboard cards, section headers, overlay elements — anything
// that is NOT inside a recycled ListView/TableView delegate.
//
// NOTE: This wrapper is an Item with no implicit size of its own.
// Use anchors.left/right or explicit width/height on the instance
// to control sizing. For GridLayout children, prefer the inline
// stagger pattern directly on the child (see DataTable row pattern).
//
// Usage:
//   StaggerEntrance {
//       index: 0
//       baseDelay: Theme.staggerCard
//       anchors.left: parent.left
//       anchors.right: parent.right
//       RowLayout { ... }
//   }

Item {
    id: root

    property int index: 0
    property int baseDelay: Theme.staggerCard

    default property alias content: container.children

    opacity: 0
    y: 16

    Component.onCompleted: staggerTimer.start()

    Timer {
        id: staggerTimer
        interval: root.index * root.baseDelay
        onTriggered: {
            root.opacity = 1
            root.y = 0
        }
    }

    Behavior on opacity { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeOutExpo } }
    Behavior on y      { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeOutExpo } }

    Item {
        id: container
        anchors.fill: parent
    }
}
