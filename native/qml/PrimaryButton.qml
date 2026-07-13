import QtQuick
import MFinlogs

Button {
    property bool danger: false
    variant: danger ? "danger" : "primary"
}
