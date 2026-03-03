import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    anchors.fill: parent

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Text { text: "Cloud Files"; font.pixelSize: 24 }
        Row {
            spacing: 8
            TextField { placeholderText: "Filter"; width: 280 }
            ComboBox { model: ["Name", "Date", "Size"] }
            Button { text: "Refresh" }
        }

        ListView {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 8
            anchors.bottom: parent.bottom
            model: 0
            delegate: Rectangle {
                width: parent.width
                height: 64
                color: "#1f1f1f"
                border.color: "#3a3a3a"
                radius: 8
            }
        }
    }
}
