import QtQuick 2.15
import QtQuick.Controls 2.15

Frame {
    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        Text { text: "Layer Inspector" }
        Slider { from: 0; to: 1; value: 0 }
        Button { text: "Export layer" }
    }
}
