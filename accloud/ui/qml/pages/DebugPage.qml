import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    anchors.fill: parent

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Text { text: "Debug"; font.pixelSize: 24 }
        Button { text: "Open app log" }
        Button { text: "Open http log" }
        Button { text: "Open render3d log" }
        TextArea {
            readOnly: true
            placeholderText: "Structured logs preview"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height - 140
        }
    }
}
