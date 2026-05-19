import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme

Rectangle {
    id: root

    property int tableRowHorizontalMargin: 0
    property int rightExtraMargin: 0
    property int colXThumb: 0
    property int colThumbWidth: 0
    property int colXName: 0
    property int colNameWidth: 0
    property int colXType: 0
    property int colTypeWidth: 0
    property int colXSize: 0
    property int colSizeWidth: 0
    property int colXDate: 0
    property int colDateWidth: 0
    property int colXActions: 0
    property int colActionsWidth: 0

    Layout.fillWidth: true
    Layout.preferredHeight: 36
    color: Theme.bgSurface
    border.width: 0

    Item {
        id: fileTableHeaderRow
        objectName: "fileTableHeaderRow"
        anchors.fill: parent
        anchors.leftMargin: root.tableRowHorizontalMargin
        anchors.rightMargin: root.tableRowHorizontalMargin + root.rightExtraMargin
        clip: true

        Text {
            objectName: "fileHeaderThumb"
            x: root.colXThumb
            width: root.colThumbWidth
            height: parent.height
            text: qsTr("Thumb")
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontCaptionPx
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            objectName: "fileHeaderName"
            x: root.colXName
            width: root.colNameWidth
            height: parent.height
            text: qsTr("File name")
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontCaptionPx
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            objectName: "fileHeaderType"
            x: root.colXType
            width: root.colTypeWidth
            height: parent.height
            text: qsTr("Type")
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontCaptionPx
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            objectName: "fileHeaderSize"
            x: root.colXSize
            width: root.colSizeWidth
            height: parent.height
            text: qsTr("Size")
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontCaptionPx
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            objectName: "fileHeaderDate"
            x: root.colXDate
            width: root.colDateWidth
            height: parent.height
            text: qsTr("Date")
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontCaptionPx
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            objectName: "fileHeaderActions"
            x: root.colXActions
            width: root.colActionsWidth
            height: parent.height
            text: qsTr("Actions")
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontCaptionPx
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
