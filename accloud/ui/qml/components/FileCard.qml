import QtQuick 2.15
import QtQuick.Controls 2.15

Frame {
    property string fileName: ""

    Column {
        spacing: 6
        Text { text: fileName; font.pixelSize: 16 }
        Row {
            spacing: 8
            Button { text: "Open" }
            Button { text: "Download" }
        }
    }
}
