import QtQuick
import MFinlogs

// Page title + subtitle block used at the top of every page.
Column {
    id: root
    property string title: ""
    property string subtitle: ""
    spacing: 4

    Text {
        text: root.title
        color: Theme.palette.fg
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fsTitle
        font.weight: Font.Bold
    }
    Text {
        visible: root.subtitle.length > 0
        text: root.subtitle
        color: Theme.palette.fgMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fsSmall
    }
}
