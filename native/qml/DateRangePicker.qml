import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import MFinlogs

GlassPanel {
    id: root
    implicitWidth: 480
    implicitHeight: 52
    radius: Theme.rMd
    elevated: false
    fillColor: Theme.alpha(Theme.palette.bgSubtle, 0.5)

    property date fromDate: new Date()
    property date toDate: new Date()
    property string fromIso: ""
    property string toIso: ""

    signal rangeChanged()

    function setRange(dateFrom, dateTo) {
        fromDate = dateFrom
        toDate = dateTo
        fromIso = dateFrom.getFullYear() + "-" + String(dateFrom.getMonth() + 1).padStart(2, "0") + "-" + String(dateFrom.getDate()).padStart(2, "0")
        toIso = dateTo.getFullYear() + "-" + String(dateTo.getMonth() + 1).padStart(2, "0") + "-" + String(dateTo.getDate()).padStart(2, "0")
    }

    function preset(daysBack) {
        var now = new Date()
        var from = new Date(now)
        from.setDate(from.getDate() - daysBack)
        setRange(from, now)
        rangeChanged()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s4
        anchors.rightMargin: Theme.s4
        spacing: Theme.s3

        Text {
            text: "\u231A"
            color: Theme.palette.primary
            font.pixelSize: 16
        }

        ColumnLayout {
            spacing: 0
            Text {
                text: "From"
                color: Theme.palette.fgSubtle
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsTiny
            }
            TextField {
                id: fromField
                implicitWidth: 120
                implicitHeight: 26
                text: root.fromIso
                color: Theme.palette.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsSmall
                placeholderText: "YYYY-MM-DD"
                placeholderTextColor: Theme.palette.fgSubtle
                background: Rectangle {
                    radius: Theme.rSm
                    color: Theme.alpha(Theme.palette.fg, 0.04)
                    border.width: Theme.bwDefault
                    border.color: Theme.palette.border
                }
                onTextChanged: {
                    if (text.length === 10) {
                        var d = new Date(text.substr(0,4), parseInt(text.substr(5,2))-1, text.substr(8,2))
                        if (!isNaN(d.getTime())) {
                            root.fromDate = d
                            root.fromIso = text
                            root.rangeChanged()
                        }
                    }
                }
            }
        }

        ColumnLayout {
            spacing: 0
            Text {
                text: "To"
                color: Theme.palette.fgSubtle
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsTiny
            }
            TextField {
                id: toField
                implicitWidth: 120
                implicitHeight: 26
                text: root.toIso
                color: Theme.palette.fg
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fsSmall
                placeholderText: "YYYY-MM-DD"
                placeholderTextColor: Theme.palette.fgSubtle
                background: Rectangle {
                    radius: Theme.rSm
                    color: Theme.alpha(Theme.palette.fg, 0.04)
                    border.width: Theme.bwDefault
                    border.color: Theme.palette.border
                }
                onTextChanged: {
                    if (text.length === 10) {
                        var d = new Date(text.substr(0,4), parseInt(text.substr(5,2))-1, text.substr(8,2))
                        if (!isNaN(d.getTime())) {
                            root.toDate = d
                            root.toIso = text
                            root.rangeChanged()
                        }
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        Repeater {
            model: ["7d", "30d", "90d", "1y"]
            delegate: Rectangle {
                Layout.preferredHeight: 26
                Layout.preferredWidth: labelText.implicitWidth + 16
                radius: Theme.rPill
                color: Theme.alpha(Theme.palette.primary, 0.08)
                border.width: 1
                border.color: Theme.alpha(Theme.palette.primary, 0.15)
                Text {
                    id: labelText
                    anchors.centerIn: parent
                    text: modelData
                    color: Theme.palette.primary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fsTiny
                    font.weight: Theme.wSemibold
                }
                TapHandler {
                    onTapped: {
                        var days = modelData === "7d" ? 7 : modelData === "30d" ? 30 : modelData === "90d" ? 90 : 365
                        root.preset(days)
                    }
                }
                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }
    }

    Component.onCompleted: {
        var now = new Date()
        var from = new Date(now)
        from.setDate(from.getDate() - 30)
        setRange(from, now)
    }
}
