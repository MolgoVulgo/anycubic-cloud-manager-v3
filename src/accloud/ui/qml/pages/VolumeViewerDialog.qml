import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components"
import "../components/Theme.js" as Theme

AppDialogFrame {
    id: root
    objectName: "volumeViewerDialog"
    property string sourcePath: ""
    property string displayFileName: ""
    property int workerCount: 4

    title: displayFileName.length > 0
           ? qsTr("3D view — %1").arg(displayFileName)
           : qsTr("3D view")
    subtitle: qsTr("Layer-based reconstruction of the selected cloud file")
    dialogSize: "workspace"
    minimumWidth: 900
    maximumWidth: 1600
    minimumHeight: 680
    maximumHeight: 1200
    allowScrimClose: false

    function openFile(localPath, fileName) {
        sourcePath = String(localPath || "")
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
    }

    footerLeadingData: [
        AppButton {
            objectName: "viewerDialogResetButton"
            text: qsTr("Reset view")
            variant: "secondary"
            onClicked: viewerPage.resetView()
        }
    ]

    footerTrailingData: [
        AppButton {
            objectName: "viewerDialogCloseButton"
            text: qsTr("Close")
            variant: "secondary"
            onClicked: root.close()
        }
    ]
}
