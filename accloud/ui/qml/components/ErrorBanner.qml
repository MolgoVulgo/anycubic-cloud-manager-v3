import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    property string message: ""
    property string operationId: ""

    color: "#5a1414"
    radius: 8
    height: 56

    Row {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8
        Text { text: message; color: "white" }
        Text { text: operationId; color: "#ffb3b3" }
    }
}
