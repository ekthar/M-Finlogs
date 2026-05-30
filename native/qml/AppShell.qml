import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: shell
    anchors.fill: parent

    property bool collapsed: false
    property int currentIndex: 0

    // Navigation model: groups (header) + items. pageId maps to StackView pages.
    readonly property var navModel: [
        { kind: "item", label: "Dashboard", glyph: "\u25C9", page: "DashboardPage" },
        { kind: "item", label: "Daily Entry", glyph: "\u270E", page: "DailyEntryPage" },
        { kind: "header", label: "Reports" },
        { kind: "item", label: "Party Ledger", glyph: "\u2630", page: "LedgerPage" },
        { kind: "item", label: "Day Book", glyph: "\u2637", page: "DayBookPage" },
        { kind: "item", label: "Daily Summary", glyph: "\u25A4", page: "DailySummaryPage" },
        { kind: "item", label: "Outstanding", glyph: "\u26A0", page: "OutstandingPage" },
        { kind: "item", label: "Trial Balance", glyph: "\u2696", page: "TrialBalancePage" },
        { kind: "item", label: "Profit & Loss", glyph: "\u2197", page: "ProfitLossPage" },
        { kind: "header", label: "Manage" },
        { kind: "item", label: "Parties", glyph: "\u263A", page: "PartiesPage" },
        { kind: "item", label: "Audit Logs", glyph: "\u2261", page: "AuditPage" },
        { kind: "item", label: "Settings", glyph: "\u2699", page: "SettingsPage" }
    ]

    function pageTitle() {
        for (var i = 0; i < navModel.length; i++) {
            if (navModel[i].kind === "item" && navModel[i].page === currentPage)
                return navModel[i].label
        }
        return "Dashboard"
    }
    property string currentPage: "DashboardPage"

    function navigate(page) {
        if (page === currentPage) return
        currentPage = page
        stack.replace(Qt.resolvedUrl("pages/" + page + ".qml"),
                      {}, StackView.PushTransition)
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---------------- Sidebar ----------------
        Item {
            id: sidebar
            Layout.fillHeight: true
            Layout.preferredWidth: shell.collapsed ? 76 : 244
            Behavior on Layout.preferredWidth { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeInOut } }

            // Glass sidebar surface
            Rectangle {
                anchors.fill: parent
                color: Theme.alpha(Theme.bg0, 0.55)
                Rectangle {
                    anchors.right: parent.right
                    width: 1
                    height: parent.height
                    color: Theme.glassBorderSoft
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: Theme.s5
                anchors.bottomMargin: Theme.s4
                spacing: Theme.s2

                // Brand
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 12
                    spacing: 10
                    Rectangle {
                        width: 34; height: 34; radius: 10
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.grad0 }
                            GradientStop { position: 1.0; color: Theme.grad1 }
                        }
                        Text { anchors.centerIn: parent; text: "M"; color: "#fff"; font.pixelSize: 18; font.weight: Font.Bold; font.family: Theme.fontFamily }
                    }
                    Text {
                        visible: !shell.collapsed
                        Layout.fillWidth: true
                        text: "M-Finlogs"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: 17
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        width: 26; height: 26; radius: 7
                        color: collapseHover.hovered ? Theme.glassStrong : "transparent"
                        Text { anchors.centerIn: parent; text: shell.collapsed ? "\u00BB" : "\u00AB"; color: Theme.textDim; font.pixelSize: 14 }
                        HoverHandler { id: collapseHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: shell.collapsed = !shell.collapsed }
                    }
                }

                Item { height: Theme.s3 }

                // Nav list
                ListView {
                    id: navList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    interactive: false
                    spacing: 2
                    model: shell.navModel
                    delegate: Loader {
                        width: navList.width
                        sourceComponent: modelData.kind === "header" ? headerComp : itemComp

                        Component {
                            id: headerComp
                            Item {
                                width: navList.width
                                height: shell.collapsed ? 14 : 30
                                Text {
                                    visible: !shell.collapsed
                                    anchors.left: parent.left
                                    anchors.leftMargin: 24
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 6
                                    text: modelData.label
                                    color: Theme.textFaint
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fsTiny
                                    font.weight: Font.Bold
                                    font.capitalization: Font.AllUppercase
                                    font.letterSpacing: 1
                                }
                            }
                        }
                        Component {
                            id: itemComp
                            NavButton {
                                width: navList.width
                                label: modelData.label
                                glyph: modelData.glyph
                                collapsed: shell.collapsed
                                active: shell.currentPage === modelData.page
                                onClicked: shell.navigate(modelData.page)
                            }
                        }
                    }
                }

                // Footer credit
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    visible: !shell.collapsed
                    Text {
                        anchors.centerIn: parent
                        text: "Crafted by EKTHAR"
                        color: Theme.textFaint
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsTiny
                        font.letterSpacing: 0.5
                    }
                }
            }
        }

        // ---------------- Main column ----------------
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Top bar
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 66

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.s8
                    anchors.rightMargin: Theme.s8
                    spacing: Theme.s4

                    ColumnLayout {
                        spacing: 0
                        Text {
                            text: shell.pageTitle()
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: 20
                            font.weight: Font.Bold
                        }
                        Text {
                            text: backend.companyName.length > 0 ? backend.companyName : "Financial workspace"
                            color: Theme.textDim
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsTiny
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // User chip
                    Rectangle {
                        Layout.preferredHeight: 38
                        Layout.preferredWidth: userRow.implicitWidth + 28
                        radius: Theme.rPill
                        color: Theme.glass
                        border.width: 1
                        border.color: Theme.glassBorder
                        RowLayout {
                            id: userRow
                            anchors.centerIn: parent
                            spacing: 8
                            Rectangle {
                                width: 24; height: 24; radius: 12
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: Theme.grad0 }
                                    GradientStop { position: 1.0; color: Theme.grad2 }
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: backend.currentUser.length > 0 ? backend.currentUser.charAt(0).toUpperCase() : "?"
                                    color: "#fff"; font.pixelSize: 12; font.weight: Font.Bold
                                }
                            }
                            Text {
                                text: backend.currentUser
                                color: Theme.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fsSmall
                                font.weight: Font.DemiBold
                            }
                            Rectangle {
                                width: 1; height: 18; color: Theme.glassBorder
                            }
                            Text {
                                text: backend.isAdmin ? "Admin" : "Accounts"
                                color: Theme.textDim
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fsTiny
                            }
                        }
                    }

                    GhostButton {
                        text: "Logout"
                        implicitWidth: 96
                        onClicked: backend.logout()
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.glassBorderSoft
                }
            }

            // Page stack
            StackView {
                id: stack
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                initialItem: Qt.resolvedUrl("pages/DashboardPage.qml")

                pushEnter: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durBase; easing.type: Theme.easeOut }
                        NumberAnimation { property: "y"; from: 16; to: 0; duration: Theme.durBase; easing.type: Theme.easeOut }
                    }
                }
                pushExit: Transition {
                    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durFast }
                }
                replaceEnter: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durBase; easing.type: Theme.easeOut }
                        NumberAnimation { property: "y"; from: 16; to: 0; duration: Theme.durBase; easing.type: Theme.easeOut }
                    }
                }
                replaceExit: Transition {
                    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durFast }
                }
            }
        }
    }
}
