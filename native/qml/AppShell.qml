import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: shell
    anchors.fill: parent

    property bool collapsed: false
    property int currentIndex: 0

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
        { kind: "item", label: "Inventory", glyph: "\u2692", page: "InventoryPage" },
        { kind: "item", label: "Credit Ledger", glyph: "\u25C8", page: "CreditLedgerPage" },
        { kind: "item", label: "Credit Followup", glyph: "\u26A0", page: "CreditFollowupPage" },
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
        if (page === currentPage && stack.currentItem) {
            if (typeof stack.currentItem.refresh === "function") {
                stack.currentItem.refresh()
            }
            return
        }

        if (stack.currentItem) {
            var pageItem = stack.currentItem
            for (var i = 0; i < pageItem.children.length; i++) {
                var child = pageItem.children[i]
                if (child && child.hasOwnProperty("close") &&
                        child.hasOwnProperty("visible") && child.visible) {
                    child.close()
                }
            }
        }

        var prev = currentPage
        currentPage = page
        var url = Qt.resolvedUrl("pages/" + page + ".qml")
        var item = stack.replace(url, {}, StackView.ReplaceTransition)
        if (!item) {
            currentPage = prev
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            id: sidebar
            Layout.fillHeight: true
            Layout.preferredWidth: shell.collapsed ? 76 : 244
            Behavior on Layout.preferredWidth { NumberAnimation { duration: Theme.durBase; easing.type: Theme.easeInOut } }

            Rectangle {
                anchors.fill: parent
                color: Theme.palette.bg
                Rectangle {
                    anchors.right: parent.right
                    width: 1
                    height: parent.height
                    color: Theme.palette.border
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: Theme.s5
                anchors.bottomMargin: Theme.s4
                spacing: Theme.s2

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 12
                    spacing: 10
                    Rectangle {
                        width: 34; height: 34; radius: 10
                        color: Theme.palette.primary
                        Text { anchors.centerIn: parent; text: "M"; color: Theme.palette.primaryFg; font.pixelSize: 18; font.weight: Font.Bold; font.family: Theme.fontFamily }
                    }
                    Text {
                        visible: !shell.collapsed
                        Layout.fillWidth: true
                        text: "M-Finlogs"
                        color: Theme.palette.fg
                        font.family: Theme.fontFamily
                        font.pixelSize: 17
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        width: 26; height: 26; radius: 7
                        color: collapseHover.hovered ? Theme.alpha(Theme.palette.fg, 0.08) : "transparent"
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: shell.collapsed ? "Expand sidebar" : "Collapse sidebar"
                        Keys.onReturnPressed: shell.collapsed = !shell.collapsed
                        Keys.onSpacePressed: shell.collapsed = !shell.collapsed
                        Text { anchors.centerIn: parent; text: shell.collapsed ? "\u00BB" : "\u00AB"; color: Theme.palette.fgMuted; font.pixelSize: 14 }
                        HoverHandler { id: collapseHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: shell.collapsed = !shell.collapsed }
                    }
                }

                Item { height: Theme.s3 }

                ListView {
                    id: navList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    focus: true
                    keyNavigationEnabled: true
                    interactive: true
                    clip: true
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
                                    color: Theme.palette.fgSubtle
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

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    visible: !shell.collapsed
                    Text {
                        anchors.centerIn: parent
                        text: "Crafted by EKTHAR"
                        color: Theme.palette.fgSubtle
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsTiny
                        font.letterSpacing: 0.5
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

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
                            color: Theme.palette.fg
                            font.family: Theme.fontFamily
                            font.pixelSize: 20
                            font.weight: Font.Bold
                        }
                        Text {
                            text: backend.companyName.length > 0 ? backend.companyName : "Financial workspace"
                            color: Theme.palette.fgMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fsTiny
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        Layout.preferredHeight: 38
                        Layout.preferredWidth: userRow.implicitWidth + 28
                        radius: Theme.rPill
                        color: Theme.palette.surface
                        border.width: 1
                        border.color: Theme.palette.border
                        RowLayout {
                            id: userRow
                            anchors.centerIn: parent
                            spacing: 8
                            Rectangle {
                                width: 24; height: 24; radius: 12
                                color: Theme.palette.primary
                                Text {
                                    anchors.centerIn: parent
                                    text: backend.currentUser.length > 0 ? backend.currentUser.charAt(0).toUpperCase() : "?"
                                    color: Theme.palette.primaryFg; font.pixelSize: 12; font.weight: Font.Bold
                                }
                            }
                            Text {
                                text: backend.currentUser
                                color: Theme.palette.fg
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fsSmall
                                font.weight: Theme.wSemibold
                            }
                            Rectangle {
                                width: 1; height: 18; color: Theme.palette.border
                            }
                            Text {
                                text: backend.isAdmin ? "Admin" : "Accounts"
                                color: Theme.palette.fgMuted
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
                    color: Theme.palette.border
                }
            }

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

    // ============ Keyboard shortcuts ============
    // Alt+N → Daily Entry, Alt+D → Dashboard, Alt+L → Ledger
    // Alt+I → Inventory, Alt+P → Parties, Alt+S → Settings
    // Ctrl+K → focus search, Alt+Left/Right → prev/next page
    focus: true
    Keys.onPressed: function(event) {
        if (event.modifiers & Qt.AltModifier) {
            switch (event.key) {
            case Qt.Key_N: shell.navigate("DailyEntryPage"); event.accepted = true; break
            case Qt.Key_D: shell.navigate("DashboardPage"); event.accepted = true; break
            case Qt.Key_L: shell.navigate("LedgerPage"); event.accepted = true; break
            case Qt.Key_I: shell.navigate("InventoryPage"); event.accepted = true; break
            case Qt.Key_P: shell.navigate("PartiesPage"); event.accepted = true; break
            case Qt.Key_S: shell.navigate("SettingsPage"); event.accepted = true; break
            case Qt.Key_O: shell.navigate("OutstandingPage"); event.accepted = true; break
            case Qt.Key_T: shell.navigate("TrialBalancePage"); event.accepted = true; break
            case Qt.Key_B: shell.navigate("DayBookPage"); event.accepted = true; break
            case Qt.Key_A: shell.navigate("AuditPage"); event.accepted = true; break
            case Qt.Key_C: shell.navigate("CreditLedgerPage"); event.accepted = true; break
            case Qt.Key_F: shell.navigate("CreditFollowupPage"); event.accepted = true; break
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+["
        onActivated: shell.collapsed = !shell.collapsed
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: searchOverlay.toggle()
    }

    Shortcut {
        sequence: "Ctrl+/"
        onActivated: shortcutOverlay.toggle()
    }

    GlobalSearch {
        id: searchOverlay
    }

    ShortcutOverlay {
        id: shortcutOverlay
    }
}
