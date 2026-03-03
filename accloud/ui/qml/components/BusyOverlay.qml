import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    color: "#88000000"
    visible: false

    Column {
        anchors.centerIn: parent
        spacing: 10
        BusyIndicator { running: parent.parent.visible }
        Button { text: "Cancel" }
    }
}
