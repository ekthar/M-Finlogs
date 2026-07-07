import QtQuick
import QtQuick.Controls.Basic
import MFinlogs

// Custom animated date picker: a glass field that opens a hand-built calendar
// popup with month navigation, smooth grid transitions and a selected-day pill.
// `selectedDate` is a JS Date; `isoText` exposes yyyy-MM-dd for the backend.
Item {
    id: root
    property string label: ""
    property date selectedDate: new Date()
    readonly property string isoText: Qt.formatDate(selectedDate, "yyyy-MM-dd")

    implicitHeight: (label.length > 0 ? 20 : 0) + 44
    implicitWidth: 180

    readonly property var monthNames: ["January","February","March","April","May","June",
        "July","August","September","October","November","December"]
    readonly property var dayNames: ["Su","Mo","Tu","We","Th","Fr","Sa"]

    property int viewYear: selectedDate.getFullYear()
    property int viewMonth: selectedDate.getMonth()

    function openCalendar() {
        console.log("[DATEPICKER] openCalendar() - viewYear:", viewYear, "viewMonth:", viewMonth)
        viewYear = selectedDate.getFullYear()
        viewMonth = selectedDate.getMonth()
        rebuild()
        pop.open()
    }
    function shiftMonth(delta) {
        var m = viewMonth + delta
        var y = viewYear
        if (m < 0) { m = 11; y-- }
        else if (m > 11) { m = 0; y++ }
        viewMonth = m; viewYear = y
        rebuild()
    }
    function rebuild() {
        dayModel.clear()
        var first = new Date(viewYear, viewMonth, 1)
        var startDow = first.getDay()
        var daysInMonth = new Date(viewYear, viewMonth + 1, 0).getDate()
        for (var i = 0; i < startDow; i++) dayModel.append({ day: 0 })
        for (var d = 1; d <= daysInMonth; d++) dayModel.append({ day: d })
    }
    function isSelected(d) {
        return d === selectedDate.getDate()
            && viewMonth === selectedDate.getMonth()
            && viewYear === selectedDate.getFullYear()
    }
    function isToday(d) {
        var t = new Date()
        return d === t.getDate() && viewMonth === t.getMonth() && viewYear === t.getFullYear()
    }

    Column {
        anchors.fill: parent
        spacing: 6
        Text {
            visible: root.label.length > 0
            text: root.label
            color: Theme.textDim
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsSmall
            font.weight: Font.DemiBold
        }
        Rectangle {
            id: field
            width: parent.width
            height: 44
            radius: Theme.rMd
            color: pop.visible ? Theme.alpha(Theme.accent, 0.10) : Theme.glass
            border.width: pop.visible ? 1.5 : 1
            border.color: pop.visible ? Theme.accent : Theme.glassBorder
            Behavior on border.color { ColorAnimation { duration: Theme.durFast } }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 14
                text: Qt.formatDate(root.selectedDate, "dd MMM yyyy")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsBody
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 14
                text: "\uD83D\uDCC5"
                font.pixelSize: 15
                opacity: 0.8
            }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.openCalendar() }
        }
    }

    Popup {
        id: pop
        y: root.height + 6
        width: 280
        padding: 14
        modal: false
        focus: true
        onOpened: console.log("[DATEPICKER] popup opened")
        onClosed: console.log("[DATEPICKER] popup closed")

        background: GlassPanel {
            fillColor: Theme.bg2
            radius: Theme.rLg
        }
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durFast }
            NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: Theme.durBase; easing.type: Theme.easeOut }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durFast }
        }

        contentItem: Column {
            spacing: Theme.s3

            // Header: month nav
            Row {
                width: parent.width
                height: 32
                Rectangle {
                    width: 30; height: 30; radius: 8
                    color: navPrevHover.hovered ? Theme.glassStrong : "transparent"
                    Text { anchors.centerIn: parent; text: "\u2039"; color: Theme.text; font.pixelSize: 18 }
                    HoverHandler { id: navPrevHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.shiftMonth(-1) }
                }
                Text {
                    width: parent.width - 60
                    height: 30
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: root.monthNames[root.viewMonth] + " " + root.viewYear
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsBody
                    font.weight: Font.DemiBold
                }
                Rectangle {
                    width: 30; height: 30; radius: 8
                    color: navNextHover.hovered ? Theme.glassStrong : "transparent"
                    Text { anchors.centerIn: parent; text: "\u203A"; color: Theme.text; font.pixelSize: 18 }
                    HoverHandler { id: navNextHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.shiftMonth(1) }
                }
            }

            // Weekday labels
            Row {
                width: parent.width
                Repeater {
                    model: root.dayNames
                    delegate: Text {
                        width: 252 / 7
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData
                        color: Theme.textFaint
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsTiny
                        font.weight: Font.DemiBold
                    }
                }
            }

            // Day grid
            Grid {
                id: grid
                columns: 7
                width: parent.width
                rowSpacing: 2
                columnSpacing: 0

                Repeater {
                    model: ListModel { id: dayModel }
                    delegate: Item {
                        width: 252 / 7
                        height: 32
                        visible: true

                        Rectangle {
                            visible: day > 0
                            anchors.centerIn: parent
                            width: 30; height: 30
                            radius: 8
                            color: root.isSelected(day) ? Theme.accent
                                 : (dayHover.hovered ? Theme.glassStrong : "transparent")
                            border.width: root.isToday(day) && !root.isSelected(day) ? 1 : 0
                            border.color: Theme.accent
                            Behavior on color { ColorAnimation { duration: Theme.durFast } }

                            Text {
                                anchors.centerIn: parent
                                text: day > 0 ? day : ""
                                color: root.isSelected(day) ? Theme.accentInk : Theme.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fsSmall
                                font.weight: root.isSelected(day) ? Font.Bold : Font.Normal
                            }
                            HoverHandler { id: dayHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                onTapped: {
                                    root.selectedDate = new Date(root.viewYear, root.viewMonth, day)
                                    pop.close()
                                }
                            }
                        }
                    }
                }
            }

            // Quick actions
            Row {
                width: parent.width
                spacing: Theme.s2
                Rectangle {
                    width: (parent.width - Theme.s2) / 2
                    height: 30
                    radius: Theme.rSm
                    color: todayHover.hovered ? Theme.glassStrong : Theme.glass
                    Text { anchors.centerIn: parent; text: "Today"; color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.DemiBold }
                    HoverHandler { id: todayHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: { root.selectedDate = new Date(); pop.close() } }
                }
                Rectangle {
                    width: (parent.width - Theme.s2) / 2
                    height: 30
                    radius: Theme.rSm
                    color: closeHover.hovered ? Theme.glassStrong : Theme.glass
                    Text { anchors.centerIn: parent; text: "Close"; color: Theme.textDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Font.DemiBold }
                    HoverHandler { id: closeHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: pop.close() }
                }
            }
        }
    }
}
