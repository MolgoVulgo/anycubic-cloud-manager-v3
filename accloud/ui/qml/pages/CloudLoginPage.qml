import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    anchors.fill: parent

    Column {
        anchors.centerIn: parent
        spacing: 10
        Text { text: "Cloud Login"; font.pixelSize: 24 }
        TextField { placeholderText: "Email"; width: 300 }
        TextField { placeholderText: "Password"; echoMode: TextInput.Password; width: 300 }
        Button { text: "Login" }
    }
}
