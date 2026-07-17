import QtQuick
import QtQuick.Controls.Basic
import MFinlogs

Item {
    id: root
    property string label: ""
    property date selectedDate: new Date()
    readonly property string isoText: Qt.formatDate(selectedDate, "yyyy-MM-dd")
    signal accepted()

    implicitHeight: (label.length > 0 ? 20 : 0) + 44
    implicitWidth: 180

    readonly property var monthNames: ["January","February","March","April","May","June",
        "July","August","September","October","November","December"]
    readonly property var dayNames: ["Su","Mo","Tu","We","Th","Fr","Sa"]

    property int viewYear: selectedDate.getFullYear()
    property int viewMonth: selectedDate.getMonth()

    function openCalendar() {
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
            color: Theme.palette.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsSmall
            font.weight: Theme.wSemibold
        }
        Rectangle {
            id: field
            width: parent.width
            height: 44
            radius: Theme.rMd
            color: pop.visible ? Theme.alpha(Theme.palette.primary, 0.10) : (fieldFocus.activeFocus ? Theme.alpha(Theme.palette.primary, 0.08) : "transparent")
            border.width: (pop.visible || fieldFocus.activeFocus) ? 1.5 : Theme.bwDefault
            border.color: (pop.visible || fieldFocus.activeFocus) ? Theme.palette.primary : Theme.palette.border
            Behavior on border.color { ColorAnimation { duration: Theme.durFast } }

            FocusScope {
                id: fieldFocus
                anchors.fill: parent
                activeFocusOnTab: true

                Keys.onReturnPressed: { root.openCalendar() }
                Keys.onEnterPressed: { root.openCalendar() }
                Keys.onSpacePressed: { root.openCalendar() }
                Keys.onEscapePressed: { if (pop.visible) pop.close() }

                Accessible.role: Accessible.Button
                Accessible.name: root.label.length > 0 ? root.label + " date picker" : "Date picker"

                Rectangle { anchors.fill: parent; color: "transparent" }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 14
                text: Qt.formatDate(root.selectedDate, "dd MMM yyyy")
                color: Theme.palette.fg
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
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: GlassPanel {
            fillColor: Theme.palette.bg
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

            Row {
                width: parent.width
                height: 32
                Rectangle {
                    width: 30; height: 30; radius: 8
                    color: navPrevHover.hovered ? Theme.alpha(Theme.palette.fg, 0.08) : "transparent"
                    Text { anchors.centerIn: parent; text: "\u2039"; color: Theme.palette.fg; font.pixelSize: 18 }
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: "Previous month"
                    FocusRing { visible: parent.activeFocus }
                    HoverHandler { id: navPrevHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.shiftMonth(-1) }
                }
                Text {
                    width: parent.width - 60
                    height: 30
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: root.monthNames[root.viewMonth] + " " + root.viewYear
                    color: Theme.palette.fg
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsBody
                    font.weight: Theme.wSemibold
                }
                Rectangle {
                    width: 30; height: 30; radius: 8
                    color: navNextHover.hovered ? Theme.alpha(Theme.palette.fg, 0.08) : "transparent"
                    Text { anchors.centerIn: parent; text: "\u203A"; color: Theme.palette.fg; font.pixelSize: 18 }
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: "Next month"
                    FocusRing { visible: parent.activeFocus }
                    HoverHandler { id: navNextHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.shiftMonth(1) }
                }
            }

            Row {
                width: parent.width
                Repeater {
                    model: root.dayNames
                    delegate: Text {
                        width: 252 / 7
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData
                        color: Theme.palette.fgSubtle
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsTiny
                        font.weight: Theme.wSemibold
                    }
                }
            }

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
                            color: root.isSelected(day) ? Theme.palette.primary
                                 : (dayHover.hovered ? Theme.alpha(Theme.palette.fg, 0.08) : "transparent")
                            border.width: root.isToday(day) && !root.isSelected(day) ? 1 : 0
                            border.color: Theme.palette.primary
                            Behavior on color { ColorAnimation { duration: Theme.durFast } }
                            activeFocusOnTab: true
                            Accessible.role: Accessible.Button
                            Accessible.name: "Day " + day

                            Text {
                                anchors.centerIn: parent
                                text: day > 0 ? day : ""
                                color: root.isSelected(day) ? Theme.palette.primaryFg : Theme.palette.fg
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fsSmall
                                font.weight: root.isSelected(day) ? Theme.wBold : Font.Normal
                            }
                            FocusRing { visible: parent.activeFocus }
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

            Row {
                width: parent.width
                spacing: Theme.s2
                Rectangle {
                    width: (parent.width - Theme.s2) / 2
                    height: 30
                    radius: Theme.rSm
                    color: todayHover.hovered ? Theme.alpha(Theme.palette.fg, 0.08) : "transparent"
                    border.width: Theme.bwDefault
                    border.color: Theme.palette.border
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: "Today"
                    FocusRing { visible: parent.activeFocus }
                    Text { anchors.centerIn: parent; text: "Today"; color: Theme.palette.fg; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Theme.wSemibold }
                    HoverHandler { id: todayHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: { root.selectedDate = new Date(); pop.close() } }
                }
                Rectangle {
                    width: (parent.width - Theme.s2) / 2
                    height: 30
                    radius: Theme.rSm
                    color: closeHover.hovered ? Theme.alpha(Theme.palette.fg, 0.08) : "transparent"
                    border.width: Theme.bwDefault
                    border.color: Theme.palette.border
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: "Close"
                    FocusRing { visible: parent.activeFocus }
                    Text { anchors.centerIn: parent; text: "Close"; color: Theme.palette.fgMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fsTiny; font.weight: Theme.wSemibold }
                    HoverHandler { id: closeHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: pop.close() }
                }
            }
        }
    }
}
