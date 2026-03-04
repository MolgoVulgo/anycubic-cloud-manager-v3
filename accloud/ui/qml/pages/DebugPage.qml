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
        ScrollView {
            id: debugLogScroll
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height - 140
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                active: true
            }
            ScrollBar.horizontal: ScrollBar {
                policy: ScrollBar.AsNeeded
                active: true
            }

            TextArea {
                width: Math.max(debugLogScroll.availableWidth, implicitWidth)
                height: Math.max(debugLogScroll.availableHeight, implicitHeight)
                readOnly: true
                placeholderText: "Structured logs preview"
                background: null
            }
        }
    }
}
