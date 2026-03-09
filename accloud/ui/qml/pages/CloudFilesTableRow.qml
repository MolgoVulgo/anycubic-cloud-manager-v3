import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Rectangle {
    id: root

    property bool rowVisible: true
    property bool rowSelected: false
    property int rowVerticalPadding: 6
    property int selectedBleedY: 3
    property int tableRowHorizontalMargin: 0
    property int tableViewportWidth: 0

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

    property int actionDetailsWidth: 92
    property int actionDownloadWidth: 112
    property int actionPrintWidth: 96
    property int actionMenuWidth: 42

    property string fileId: ""
    property string fileName: "-"
    property string thumbnailUrl: ""
    property string sizeText: "-"
    property string fileTypeText: "-"
    property string dateText: "-"

    signal selectRequested(string fileId)
    signal detailsRequested(string fileId)
    signal downloadRequested(string fileId, string fileName)
    signal printRequested(string fileId, string fileName)
    signal renameRequested(string fileId, string fileName)
    signal deleteRequested(string fileId, string fileName)

    width: ListView.view ? ListView.view.width : 0
    height: rowVisible ? 112 : 0
    visible: rowVisible
    color: Theme.bgSurface
    border.width: 0

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.selectRequested(root.fileId)
    }

    Rectangle {
        visible: root.rowSelected
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.rowVerticalPadding - root.selectedBleedY
        anchors.bottomMargin: root.rowVerticalPadding - root.selectedBleedY
        color: Theme.selectionBg
        border.width: 0
    }

    Item {
        objectName: "fileTableDataRow"
        anchors.left: parent.left
        anchors.leftMargin: root.tableRowHorizontalMargin
        anchors.top: parent.top
        anchors.topMargin: root.rowVerticalPadding
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.rowVerticalPadding
        width: root.tableViewportWidth
        clip: true

        Rectangle {
            objectName: "fileRowThumb"
            x: root.colXThumb
            width: root.colThumbWidth
            height: root.colThumbWidth
            radius: 6
            color: Theme.accentSoft
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault
            clip: true

            Image {
                id: thumbnailImage
                anchors.fill: parent
                source: root.thumbnailUrl
                fillMode: Image.PreserveAspectFit
                visible: String(source).length > 0
                asynchronous: true
            }

            Text {
                anchors.centerIn: parent
                visible: !(thumbnailImage.visible && thumbnailImage.status === Image.Ready)
                text: qsTr("100x100")
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontCaptionPx
                font.bold: true
            }
        }

        Item {
            objectName: "fileRowNameCell"
            x: root.colXName
            width: root.colNameWidth
            height: parent.height

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    objectName: "fileRowName"
                    width: parent.width
                    text: root.fileName
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    objectName: "fileRowThumbnailPath"
                    width: parent.width
                    text: root.thumbnailUrl.length > 0 ? root.thumbnailUrl : "-"
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontCaptionPx
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    objectName: "fileRowThumbnailStatus"
                    width: parent.width
                    text: (thumbnailImage.status === Image.Ready
                           ? "thumb=ready"
                           : (thumbnailImage.status === Image.Loading
                              ? "thumb=loading"
                              : (thumbnailImage.status === Image.Error
                                 ? "thumb=error"
                                 : "thumb=null")))
                          + " vis=" + (thumbnailImage.visible ? "1" : "0")
                          + " s=" + thumbnailImage.status
                    color: thumbnailImage.status === Image.Error
                           ? Theme.danger
                           : Theme.fgSecondary
                    font.pixelSize: Theme.fontCaptionPx
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Text {
            objectName: "fileRowType"
            x: root.colXType
            width: root.colTypeWidth
            height: parent.height
            text: root.fileTypeText
            color: Theme.fgPrimary
            font.pixelSize: Theme.fontBodyPx
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            objectName: "fileRowSize"
            x: root.colXSize
            width: root.colSizeWidth
            height: parent.height
            text: root.sizeText
            color: Theme.fgPrimary
            font.pixelSize: Theme.fontBodyPx
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            objectName: "fileRowDate"
            x: root.colXDate
            width: root.colDateWidth
            height: parent.height
            text: root.dateText
            color: Theme.fgPrimary
            font.pixelSize: Theme.fontBodyPx
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Rectangle {
            objectName: "fileRowActions"
            x: root.colXActions
            width: root.colActionsWidth
            height: parent.height
            color: "transparent"

            Row {
                anchors.centerIn: parent
                spacing: 6

                AppButton {
                    text: qsTr("Details")
                    variant: "secondary"
                    width: root.actionDetailsWidth
                    onClicked: root.detailsRequested(root.fileId)
                }

                AppButton {
                    text: qsTr("Download")
                    variant: "secondary"
                    width: root.actionDownloadWidth
                    onClicked: root.downloadRequested(root.fileId, root.fileName)
                }

                AppButton {
                    text: qsTr("Print")
                    variant: "primary"
                    width: root.actionPrintWidth
                    onClicked: root.printRequested(root.fileId, root.fileName)
                }

                AppButton {
                    text: qsTr("...")
                    variant: "secondary"
                    compact: true
                    width: root.actionMenuWidth
                    onClicked: rowMenu.open()
                }

                Menu {
                    id: rowMenu

                    MenuItem {
                        text: qsTr("Rename")
                        onTriggered: root.renameRequested(root.fileId, root.fileName)
                    }

                    MenuItem {
                        text: qsTr("Delete")
                        onTriggered: root.deleteRequested(root.fileId, root.fileName)
                    }
                }
            }
        }
    }
}
