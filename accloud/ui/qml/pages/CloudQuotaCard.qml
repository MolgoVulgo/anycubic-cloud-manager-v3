import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme

Rectangle {
    id: root

    property string usedText: ""
    property string freeText: ""
    property int filesCount: 0
    property real usedRatio: 0
    property color barColor: Theme.accent
    property color backgroundColor: Theme.bgWindow

    Layout.fillWidth: true
    Layout.preferredHeight: 48
    radius: Theme.radiusControl
    color: Theme.bgSurface
    border.width: Theme.borderWidth
    border.color: Theme.borderDefault

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Text {
                text: root.usedText
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontBodyPx
                font.bold: true
            }

            Text {
                text: root.freeText
                color: Theme.fgSecondary
                font.pixelSize: Theme.fontCaptionPx
            }

            Text {
                text: qsTr("Files ") + root.filesCount
                color: Theme.fgSecondary
                font.pixelSize: Theme.fontCaptionPx
            }

            Item { Layout.fillWidth: true }
        }

        Item {
            objectName: "quotaFreeSpaceBar"
            Layout.fillWidth: true
            Layout.preferredHeight: 12

            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: root.backgroundColor
                border.width: Theme.borderWidth
                border.color: Theme.borderDefault
            }

            Rectangle {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width * Math.max(0, Math.min(1, root.usedRatio))
                height: parent.height
                radius: height / 2
                color: root.barColor
            }
        }
    }
}
