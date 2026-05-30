import QtQuick
import QtQuick.Layouts
import MFinlogs

Item {
    id: page
    property var rows: []

    function load() { rows = backend.parties() }
    Component.onCompleted: load()
    Connections { target: backend; function onDataChanged() { page.load() } }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s8
        anchors.rightMargin: Theme.s8
        anchors.topMargin: Theme.s5
        anchors.bottomMargin: Theme.s6
        spacing: Theme.s5

        SectionHeader {
            title: "Parties"
            subtitle: "Customers, suppliers, banks and expense accounts"
        }

        GlassPanel {
            Layout.fillWidth: true
            implicitHeight: form.implicitHeight + Theme.s8
            radius: Theme.rLg
            RowLayout {
                id: form
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.s5
                spacing: Theme.s3
                FieldInput {
                    id: nameField
                    Layout.fillWidth: true
                    label: "Party name"
                    placeholder: "e.g. Acme Traders"
                    showCompletions: false
                }
                FieldCombo {
                    id: typeField
                    Layout.preferredWidth: 200
                    label: "Type"
                    options: ["Customer", "Credit Customer", "Supplier", "Expense Account", "Bank"]
                }
                ColumnLayout {
                    spacing: 6
                    Text {
                        text: "Allow credit"
                        color: Theme.textDim
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fsSmall
                        font.weight: Font.DemiBold
                    }
                    Rectangle {
                        width: 52; height: 44
                        radius: Theme.rMd
                        color: creditToggle.on ? Theme.alpha(Theme.accent, 0.18) : Theme.glass
                        border.width: 1
                        border.color: creditToggle.on ? Theme.accent : Theme.glassBorder
                        property alias on: creditToggle.on
                        Rectangle {
                            id: knob
                            property bool on: false
                            width: 18; height: 18; radius: 9
                            color: parent.on ? Theme.accent : Theme.textFaint
                            anchors.verticalCenter: parent.verticalCenter
                            x: parent.on ? parent.width - width - 6 : 6
                            Behavior on x { NumberAnimation { duration: Theme.durFast; easing.type: Theme.easeOut } }
                            Behavior on color { ColorAnimation { duration: Theme.durFast } }
                        }
                        MouseArea { anchors.fill: parent; onClicked: knob.on = !knob.on }
                    }
                }
                PrimaryButton {
                    Layout.alignment: Qt.AlignBottom
                    Layout.preferredWidth: 140
                    text: "Create"
                    onClicked: {
                        var res = backend.createParty(nameField.text, typeField.currentText, knob.on)
                        if (res && res.ok === true) {
                            nameField.text = ""
                            knob.on = false
                        }
                    }
                }
            }
        }

        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rLg
            DataTable {
                anchors.fill: parent
                anchors.margins: Theme.s5
                emptyText: "No parties yet"
                rows: page.rows
                columns: [
                    { title: "Name", key: "name", weight: 2.4 },
                    { title: "Type", key: "type", weight: 1.4 }
                ]
            }
        }
    }
}
