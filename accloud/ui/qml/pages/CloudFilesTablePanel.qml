import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme

Rectangle {
    id: root

    property bool loading: false
    property var filesModel: null
    property string selectedFileId: ""
    property int tableRowHorizontalMargin: 8
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
    property int currentPage: 0
    property int totalPages: 1
    property int visibleCount: 0
    property int pageSize: 10

    signal selectedFileChanged(string fileId)
    signal detailsRequested(string fileId)
    signal downloadRequested(string fileId, string fileName)
    signal printRequested(string fileId, string fileName)
    signal renameRequested(string fileId, string fileName)
    signal deleteRequested(string fileId, string fileName)
    signal pageSizeSelected(int value)
    signal previousPageRequested()
    signal nextPageRequested()

    Layout.fillWidth: true
    Layout.fillHeight: true
    radius: Theme.radiusControl
    color: Theme.bgSurface
    border.width: Theme.borderWidth
    border.color: Theme.borderDefault

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        CloudFilesTableHeader {
            tableRowHorizontalMargin: root.tableRowHorizontalMargin
            rightExtraMargin: filesVBar.visible ? filesVBar.width : 0
            colXThumb: root.colXThumb
            colThumbWidth: root.colThumbWidth
            colXName: root.colXName
            colNameWidth: root.colNameWidth
            colXType: root.colXType
            colTypeWidth: root.colTypeWidth
            colXSize: root.colXSize
            colSizeWidth: root.colSizeWidth
            colXDate: root.colXDate
            colDateWidth: root.colDateWidth
            colXActions: root.colXActions
            colActionsWidth: root.colActionsWidth
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.borderWidth
            color: Theme.borderSubtle
        }

        ListView {
            id: filesList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 0
            cacheBuffer: 480
            model: root.filesModel

            ScrollBar.vertical: ScrollBar {
                id: filesVBar
                policy: ScrollBar.AsNeeded
            }

            delegate: CloudFilesTableRow {
                rowSelected: root.selectedFileId === String(model.fileId)
                tableRowHorizontalMargin: root.tableRowHorizontalMargin
                tableViewportWidth: root.tableViewportWidth
                colXThumb: root.colXThumb
                colThumbWidth: root.colThumbWidth
                colXName: root.colXName
                colNameWidth: root.colNameWidth
                colXType: root.colXType
                colTypeWidth: root.colTypeWidth
                colXSize: root.colXSize
                colSizeWidth: root.colSizeWidth
                colXDate: root.colXDate
                colDateWidth: root.colDateWidth
                colXActions: root.colXActions
                colActionsWidth: root.colActionsWidth
                actionDetailsWidth: root.actionDetailsWidth
                actionDownloadWidth: root.actionDownloadWidth
                actionPrintWidth: root.actionPrintWidth
                actionMenuWidth: root.actionMenuWidth
                fileId: String(model.fileId || "")
                fileName: String(model.fileName || "-")
                thumbnailUrl: String(model.thumbnailUrl || "")
                sizeText: String(model.sizeText || "-")
                fileTypeText: String(model.fileTypeText || "-")
                dateText: String(model.dateText || "-")
                onSelectRequested: function(fileId) { root.selectedFileChanged(fileId) }
                onDetailsRequested: function(fileId) { root.detailsRequested(fileId) }
                onDownloadRequested: function(fileId, fileName) { root.downloadRequested(fileId, fileName) }
                onPrintRequested: function(fileId, fileName) { root.printRequested(fileId, fileName) }
                onRenameRequested: function(fileId, fileName) { root.renameRequested(fileId, fileName) }
                onDeleteRequested: function(fileId, fileName) { root.deleteRequested(fileId, fileName) }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: !root.loading && root.visibleCount === 0
            text: qsTr("No file matches current type filter.")
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontBodyPx
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            padding: 10
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.borderWidth
            color: Theme.borderSubtle
            visible: root.visibleCount > 0
        }

        CloudFilesPaginationBar {
            currentPage: root.currentPage
            totalPages: root.totalPages
            rowsCount: root.visibleCount
            pageSize: root.pageSize
            visibleWhen: root.visibleCount > 0
            onPageSizeSelected: function(value) { root.pageSizeSelected(value) }
            onPreviousRequested: root.previousPageRequested()
            onNextRequested: root.nextPageRequested()
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        visible: root.loading
        running: visible
    }
}
