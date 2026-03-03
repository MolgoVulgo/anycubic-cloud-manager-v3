import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    anchors.fill: parent

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Text { text: "Settings"; font.pixelSize: 24 }
        CheckBox { text: "Enable debug logs" }
        Row {
            spacing: 8
            Text { text: "Render stride" }
            SpinBox { from: 1; to: 8; value: 1 }
        }
        Row {
            spacing: 8
            Text { text: "Render LOD" }
            SpinBox { from: 0; to: 5; value: 0 }
        }
    }
}
