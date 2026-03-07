import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme
import "."

Rectangle {
    id: root
    objectName: "busyOverlay"
    property bool running: false
    property string stage: "Running"

    color: Theme.overlayScrim
    visible: running

    Rectangle {
        anchors.centerIn: parent
        width: 280
        height: 140
        radius: 14
        color: Theme.panel
        border.width: 1
        border.color: Theme.panelStroke

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Text {
                text: root.stage
                color: Theme.textPrimary
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            BusyIndicator {
                running: root.running
                Layout.alignment: Qt.AlignHCenter
            }

            AppButton {
                text: qsTr("Cancel")
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
}
