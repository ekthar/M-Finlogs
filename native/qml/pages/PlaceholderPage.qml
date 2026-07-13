import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property string heading: "Coming soon"
    property string note: "This module is being crafted."

    ColumnLayout {
        anchors.centerIn: parent
        spacing: Theme.s3
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "\u2728"
            font.pixelSize: 40
            color: Theme.palette.primary
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: page.heading
            color: Theme.palette.fg
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsSection
            font.weight: Font.Bold
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: page.note
            color: Theme.palette.fgMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fsSmall
        }
    }
}
