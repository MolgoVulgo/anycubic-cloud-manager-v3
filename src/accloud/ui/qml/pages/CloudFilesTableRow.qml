import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Rectangle {
    id: root

    property bool rowSelected: false
    property bool batchSelected: false
    property bool selectionEnabled: true
    property int rowVerticalPadding: 6
    property int selectedBleedY: 3
    property int tableRowHorizontalMargin: 0
    property int tableViewportWidth: 0

    property int colXSelect: 0
    property int colSelectWidth: 0
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

    property int actionDetailsWidth: 78
    property int actionDownloadWidth: 104
    property int actionViewerWidth: 48
    property int actionPrintWidth: 82
    property int actionMenuWidth: 36

    property string fileId: ""
    property string fileName: "-"
    property string thumbnailUrl: ""
    property string sizeText: "-"
    property string fileTypeText: "-"
    property string dateText: "-"
    property bool viewerEnabled: false
    property bool viewerBusy: false
    readonly property bool viewerSupported: String(fileName || "").toLowerCase().endsWith(".pwsz")

    signal selectRequested(string fileId)
    signal selectionToggled(string fileId, string fileName, bool checked)
    signal detailsRequested(string fileId)
    signal downloadRequested(string fileId, string fileName)
    signal viewerRequested(string fileId, string fileName)
    signal printRequested(string fileId, string fileName)
    signal deleteRequested(string fileId, string fileName)

    objectName: "cloudFilesTableRow"
    width: ListView.view ? ListView.view.width : 0
    height: 88
    color: Theme.bgSurface
    border.width: 0

    MouseArea {
        id: rowMouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.selectRequested(root.fileId)
    }

    Rectangle {
        visible: root.rowSelected || root.batchSelected || rowMouseArea.containsMouse
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.rowVerticalPadding - root.selectedBleedY
        anchors.bottomMargin: root.rowVerticalPadding - root.selectedBleedY
        color: (root.rowSelected || root.batchSelected) ? Theme.selectionBg : Theme.bgCardSubtle
        border.width: 0
    }

    Rectangle {
        visible: root.rowSelected
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 3
        height: parent.height - 12
        radius: 2
        color: Theme.accent
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Theme.borderWidth
        color: Theme.borderSubtle
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

        AppCheckBox {
            id: selectionCheckBox
            objectName: "fileRowSelectionCheckBox"
            x: root.colXSelect + Math.max(0, (root.colSelectWidth - width) / 2)
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            text: ""
            checked: root.batchSelected
            enabled: root.selectionEnabled && root.fileId.length > 0
            z: 3
            Accessible.name: qsTr("Select %1").arg(root.fileName)
            onToggled: root.selectionToggled(root.fileId, root.fileName, checked)
        }

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
                cache: true
            }

            Text {
                anchors.centerIn: parent
                visible: !(thumbnailImage.visible && thumbnailImage.status === Image.Ready)
                text: root.colThumbWidth + "×" + root.colThumbWidth
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
                    compact: true
                    width: root.actionDetailsWidth
                    onClicked: root.detailsRequested(root.fileId)
                }

                AppButton {
                    text: qsTr("Download")
                    variant: "secondary"
                    compact: true
                    width: root.actionDownloadWidth
                    onClicked: root.downloadRequested(root.fileId, root.fileName)
                }


                AppButton {
                    objectName: "fileRowViewerButton"
                    text: qsTr("3D")
                    variant: "secondary"
                    compact: true
                    width: root.actionViewerWidth
                    visible: root.viewerSupported
                    enabled: root.viewerEnabled && !root.viewerBusy && root.fileId.length > 0
                    ToolTip.visible: hovered
                    ToolTip.delay: 350
                    ToolTip.text: root.viewerEnabled
                                  ? qsTr("Open the layer-based 3D view")
                                  : qsTr("The 3D viewer is disabled in this build.")
                    onClicked: root.viewerRequested(root.fileId, root.fileName)
                }

                AppButton {
                    objectName: "fileRowPrintButton"
                    text: qsTr("Print")
                    variant: "primary"
                    compact: true
                    width: root.actionPrintWidth
                    enabled: root.fileId.length > 0
                    ToolTip.visible: hovered && enabled
                    ToolTip.delay: 350
                    ToolTip.text: qsTr("Open print setup")
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
                        text: qsTr("Delete")
                        onTriggered: root.deleteRequested(root.fileId, root.fileName)
                    }
                }
            }
        }
    }
}
