import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Rectangle {
    id: root

    property string message: ""
    property bool showRetry: true
    signal retry()

    visible: message.length > 0
    implicitHeight: bannerRow.implicitHeight + Theme.s4
    radius: Theme.rMd
    color: Theme.alpha(Theme.palette.danger, 0.08)
    border.width: 1
    border.color: Theme.alpha(Theme.palette.danger, 0.25)

    Accessible.role: Accessible.AlertMessage
    Accessible.name: "Error: " + root.message

    RowLayout {
        id: bannerRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.s4
        anchors.rightMargin: Theme.s4
        spacing: Theme.s3

        Text {
            text: "\u26A0"
            color: Theme.palette.danger
            font.pixelSize: Theme.fsBody
        }

        Text {
            Layout.fillWidth: true
            text: root.message
            color: Theme.palette.danger
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsSmall
            font.weight: Font.Medium
            wrapMode: Text.WordWrap
            elide: Text.ElideRight
            maximumLineCount: 2
        }

        Rectangle {
            visible: root.showRetry
            width: retryText.implicitWidth + Theme.s4
            height: 28
            radius: Theme.rSm
            color: retryHover.hovered ? Theme.alpha(Theme.palette.danger, 0.15) : "transparent"
            border.width: 1
            border.color: Theme.alpha(Theme.palette.danger, 0.4)
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: "Retry"
            Keys.onReturnPressed: root.retry()
            Keys.onSpacePressed: root.retry()

            Text {
                id: retryText
                anchors.centerIn: parent
                text: "Retry"
                color: Theme.palette.danger
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsTiny
                font.weight: Font.DemiBold
            }

            FocusRing { visible: parent.activeFocus }
            HoverHandler { id: retryHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.retry() }
        }

        Rectangle {
            width: 24
            height: 24
            radius: Theme.rSm
            color: dismissHover.hovered ? Theme.alpha(Theme.palette.danger, 0.15) : "transparent"
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: "Dismiss error"
            Keys.onReturnPressed: root.message = ""
            Keys.onSpacePressed: root.message = ""

            Text {
                anchors.centerIn: parent
                text: "\u2715"
                color: Theme.palette.danger
                font.pixelSize: Theme.fsTiny
            }

            FocusRing { visible: parent.activeFocus }
            HoverHandler { id: dismissHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.message = "" }
        }
    }
}
