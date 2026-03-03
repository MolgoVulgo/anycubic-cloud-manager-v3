import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Rectangle {
    id: root
    objectName: "progressCard"
    property string stage: "idle"
    property int percent: 0

    radius: 12
    color: Theme.card
    border.width: 1
    border.color: Theme.panelStroke
    implicitHeight: 90

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: stage
                color: Theme.textPrimary
                font.bold: true
            }
            Text {
                Layout.alignment: Qt.AlignRight
                text: percent + "%"
                color: Theme.textSecondary
                font.family: "JetBrains Mono"
            }
        }

        ProgressBar {
            id: bar
            from: 0
            to: 100
            value: percent
            Layout.fillWidth: true

            background: Rectangle {
                implicitHeight: 6
                radius: 3
                color: Theme.panelStroke
            }
            contentItem: Item {
                implicitHeight: 6
                Rectangle {
                    width: bar.visualPosition * parent.width
                    height: parent.height
                    radius: 3
                    color: Theme.accent
                }
            }
        }
    }
}
