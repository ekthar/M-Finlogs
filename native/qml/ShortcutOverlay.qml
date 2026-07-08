import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: root
    anchors.fill: parent
    z: 10001

    property bool open: false

    function toggle() {
        open = !open
        console.log("[SHORTCUT] toggle() open:", open)
        if (open) {
            forceActiveFocus()
        }
    }

    opacity: open ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeOut } }

    Rectangle {
        anchors.fill: parent
        color: Theme.alpha(Theme.bg0, 0.65)
        TapHandler { onTapped: root.open = false }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(600, root.width * 0.85)
        height: Math.min(480, root.height * 0.8)
        radius: Theme.rLg
        color: Theme.bg2
        border.width: 1
        border.color: Theme.glassBorder

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.s5
            spacing: Theme.s4

            Text {
                text: "Keyboard Shortcuts"
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsSection
                font.weight: Font.Bold
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.glassBorder
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: grid.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    width: 8
                    contentItem: Rectangle {
                        implicitWidth: 8
                        radius: 4
                        color: Theme.alpha(Theme.accent, 0.45)
                    }
                    background: Rectangle {
                        implicitWidth: 8
                        radius: 4
                        color: Theme.alpha(Theme.glass, 0.2)
                    }
                }

                ColumnLayout {
                    id: grid
                    width: parent.width
                    spacing: 2

                    Repeater {
                        model: [
                            { keys: "Alt + D", desc: "Dashboard" },
                            { keys: "Alt + N", desc: "Daily Entry" },
                            { keys: "Alt + L", desc: "Party Ledger" },
                            { keys: "Alt + B", desc: "Day Book" },
                            { keys: "Alt + O", desc: "Outstanding" },
                            { keys: "Alt + T", desc: "Trial Balance" },
                            { keys: "Alt + I", desc: "Inventory" },
                            { keys: "Alt + P", desc: "Parties" },
                    { keys: "Alt + A", desc: "Audit Logs" },
                    { keys: "Alt + C", desc: "Credit Ledger" },
                    { keys: "Alt + F", desc: "Credit Followup" },
                    { keys: "Alt + S", desc: "Settings" },
                            { keys: "Ctrl + [", desc: "Toggle sidebar" },
                            { keys: "Ctrl + K", desc: "Global search" },
                            { keys: "Ctrl + /", desc: "This shortcut help" },
                            { keys: "Tab / Shift+Tab", desc: "Navigate fields" },
                            { keys: "Enter", desc: "Next field / Save" }
                        ]
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            height: 34
                            radius: Theme.rSm
                            color: index % 2 === 0 ? "transparent" : Theme.rowAlt

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.s3
                                anchors.rightMargin: Theme.s3
                                spacing: Theme.s4

                                Rectangle {
                                    Layout.preferredWidth: shortcutText.implicitWidth + 20
                                    height: 26
                                    radius: Theme.rSm
                                    color: Theme.alpha(Theme.accent, 0.1)
                                    border.width: 1
                                    border.color: Theme.alpha(Theme.accent, 0.2)
                                    Text {
                                        id: shortcutText
                                        anchors.centerIn: parent
                                        text: modelData.keys
                                        color: Theme.accent
                                        font.family: Theme.monoFamily
                                        font.pixelSize: Theme.fsTiny
                                        font.weight: Font.DemiBold
                                    }
                                }

                                Text {
                                    text: modelData.desc
                                    color: Theme.textDim
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fsSmall
                                }

                                Item { Layout.fillWidth: true }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Press Escape to close"
                    color: Theme.textFaint
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsTiny
                }
                Item { Layout.fillWidth: true }
            }
        }
    }

    Keys.onEscapePressed: root.open = false
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Slash && (event.modifiers & Qt.ControlModifier)) {
            root.open = false
            event.accepted = true
        }
    }
}
