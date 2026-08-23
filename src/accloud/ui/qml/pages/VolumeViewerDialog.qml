import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components"
import "../components/Theme.js" as Theme

AppDialogFrame {
    id: root
    objectName: "volumeViewerDialog"
    property string sourcePath: ""
    property string sourceFileId: ""
    property string displayFileName: ""
    signal printRequested(string fileId, string fileName)
    property int workerCount: 4
    property string palettePreset: "technical_cyan"
    property color partColor: "#55B7C6"
    property color viewportColor: "#171A1F"

    title: qsTr("3D view")
    subtitle: qsTr("Layer-based reconstruction of the selected cloud file")
    dialogSize: "workspace"
    minimumWidth: 900
    maximumWidth: 1600
    minimumHeight: 680
    maximumHeight: 1200
    allowScrimClose: false

    function toggleFullScreen() {
        root.fullScreen = !root.fullScreen
    }

    function openFile(localPath, fileName, fileId) {
        sourcePath = String(localPath || "")
        sourceFileId = String(fileId || "")
        displayFileName = String(fileName || "")
        if (!visible) {
            open()
            return
        }
        viewerPage.loadSource(sourcePath, displayFileName)
    }

    onOpened: Qt.callLater(function() {
        viewerPage.loadSource(root.sourcePath, root.displayFileName)
    })
    onClosed: root.fullScreen = false

    headerActionsData: [
        AppButton {
            objectName: "viewerDialogResetButton"
            text: qsTr("Reset view")
            variant: "secondary"
            compact: true
            enabled: viewerPage.loadedChunkCount > 0
            onClicked: viewerPage.resetView()
        },
        AppButton {
            objectName: "viewerDialogFullscreenButton"
            text: root.fullScreen ? qsTr("Exit full screen") : qsTr("Full screen")
            variant: "secondary"
            compact: true
            onClicked: root.toggleFullScreen()
        }
    ]

    VolumeViewerPage {
        id: viewerPage
        objectName: "cloudFileVolumeViewerPage"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: Math.max(520, root.overlayHeight * 0.7)
        showSourceControls: false
        showViewerHeader: false
        embeddedViewerInTabsContainer: false
        workerCount: root.workerCount
        palettePreset: root.palettePreset
        partColor: root.partColor
        viewportColor: root.viewportColor
    }

    footerTrailingData: [
        AppButton {
            objectName: "viewerDialogPrintButton"
            text: qsTr("Print")
            variant: "primary"
            enabled: root.sourceFileId.trim().length > 0
            onClicked: {
                var fileId = root.sourceFileId
                var fileName = root.displayFileName
                root.close()
                root.printRequested(fileId, fileName)
            }
        },
        AppButton {
            objectName: "viewerDialogCloseButton"
            text: qsTr("Close")
            variant: "secondary"
            onClicked: root.close()
        }
    ]
}
