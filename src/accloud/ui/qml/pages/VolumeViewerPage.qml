import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Accloud.Render3D 1.0
import "../components"
import "../components/Theme.js" as Theme

AppPageFrame {
    id: root
    objectName: "volumeViewerPage"
    property alias sourcePath: pathField.text
    property string displayFileName: ""
    property bool showSourceControls: true
    property bool showViewerHeader: true
    property bool embeddedViewerInTabsContainer: false
    property int workerCount: 4
    property string palettePreset: "technical_cyan"
    property color partColor: "#55B7C6"
    property color supportColor: "#F28C28"
    property color viewportColor: "#171A1F"
    property bool supportColoringEnabled: true
    readonly property int totalLayers: viewer.totalLayers
    readonly property int loadedChunkCount: viewer.loadedChunkCount
    embeddedInTabsContainer: root.embeddedViewerInTabsContainer
    showSectionHeader: root.showViewerHeader
    sectionTitle: root.displayFileName.length > 0
                  ? qsTr("3D view — %1").arg(root.displayFileName)
                  : qsTr("3D layer viewer")
    sectionSubtitle: qsTr("Reconstruct and inspect the printed volume from PWSZ layer images")

    function loadSource(localPath, fileName) {
        root.sourcePath = String(localPath || "")
        root.displayFileName = String(fileName || "")
        if (root.sourcePath.trim().length === 0)
            return
        Qt.callLater(function() { viewer.load() })
    }

    function resetView() {
        viewer.resetView()
    }

    RowLayout {
        visible: root.showSourceControls
        Layout.fillWidth: true
        spacing: Theme.gapRow

        AppTextField {
            id: pathField
            objectName: "viewerPathField"
            Layout.fillWidth: true
            placeholderText: qsTr("Absolute path or file:/// URL to a .pwsz file")
            onAccepted: viewer.load()
        }

        AppButton {
            objectName: "viewerLoadButton"
            text: viewer.loading ? qsTr("Loading…") : qsTr("Load PWSZ")
            variant: "primary"
            enabled: !viewer.loading && pathField.text.trim().length > 0
            onClicked: viewer.load()
        }
    }

    ErrorBanner {
        Layout.fillWidth: true
        visible: viewer.errorString.length > 0
        message: viewer.errorString
        severity: "danger"
        operationId: "viewer.pwsz.load"
    }

    Rectangle {
        id: viewportFrame
        objectName: "viewerViewportFrame"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 360
        radius: Theme.radiusDialog
        color: root.viewportColor
        border.width: Theme.borderWidth
        border.color: Theme.viewportBorder
        clip: true

        VolumeViewer {
            id: viewer
            objectName: "volumeViewerItem"
            anchors.fill: parent
            sourcePath: pathField.text
            backgroundColor: root.viewportColor
            meshColor: root.partColor
            supportColor: root.supportColor
            supportColoringEnabled: root.supportColoringEnabled
            workerCount: root.workerCount
        }

        MouseArea {
            id: navigationArea
            objectName: "viewerNavigationArea"
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            hoverEnabled: true
            property real previousX: 0
            property real previousY: 0

            onPressed: function(mouse) {
                previousX = mouse.x
                previousY = mouse.y
            }

            onPositionChanged: function(mouse) {
                if (pressedButtons === Qt.NoButton)
                    return
                var dx = mouse.x - previousX
                var dy = mouse.y - previousY
                previousX = mouse.x
                previousY = mouse.y
                if ((pressedButtons & Qt.RightButton) !== 0
                        || (pressedButtons & Qt.MiddleButton) !== 0
                        || (mouse.modifiers & Qt.ShiftModifier) !== 0) {
                    viewer.panPixels(dx, dy)
                } else {
                    viewer.orbitPixels(dx, dy)
                }
            }

            onWheel: function(wheel) {
                viewer.zoomSteps(wheel.angleDelta.y / 120.0)
                wheel.accepted = true
            }
        }

        Rectangle {
            objectName: "viewerStatusOverlay"
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 12
            radius: Theme.radiusControl
            color: "#99000000"
            border.width: 1
            border.color: "#55777777"
            implicitWidth: statusColumn.implicitWidth + 20
            implicitHeight: statusColumn.implicitHeight + 16

            Column {
                id: statusColumn
                anchors.centerIn: parent
                spacing: 4

                Text {
                    objectName: "viewerMachineLabel"
                    text: viewer.machineName.length > 0
                          ? viewer.machineName
                          : qsTr("No PWSZ loaded")
                    color: Theme.viewportFg
                    font.pixelSize: Theme.fontBodyPx
                    font.bold: true
                }

                Text {
                    objectName: "viewerFileSummaryLabel"
                    visible: root.displayFileName.length > 0
                    text: viewer.totalLayers > 0
                          ? qsTr("%1 · %2 layers").arg(root.displayFileName).arg(viewer.totalLayers)
                          : root.displayFileName
                    color: Theme.viewportFg
                    font.pixelSize: Theme.fontBodyPx
                    font.bold: true
                }

                Text {
                    objectName: "viewerNavigationHint"
                    text: qsTr("Left drag: orbit · Right/Shift drag: pan · Wheel: zoom")
                    color: Theme.viewportFg
                    font.pixelSize: Theme.fontCaptionPx
                }
            }
        }

        VerticalLayerRangeSlider {
            id: layerRangeControl
            objectName: "viewerLayerRangeControl"
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            width: 64
            visible: viewer.totalLayers > 0
            minimumLayer: 1
            maximumLayer: Math.max(1, viewer.totalLayers)
            lowerLayer: Math.max(1, viewer.firstLayer)
            upperLayer: Math.max(lowerLayer, viewer.lastLayer)
            z: 3

            onLowerLayerMoved: function(layer) {
                viewer.firstLayer = layer
            }

            onUpperLayerMoved: function(layer) {
                viewer.lastLayer = layer
            }
        }

        Rectangle {
            objectName: "viewerSupportsControl"
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 12
            radius: Theme.radiusControl
            color: "#99000000"
            border.width: 1
            border.color: "#55777777"
            implicitWidth: supportsCheckBox.implicitWidth + 20
            implicitHeight: supportsCheckBox.implicitHeight + 12
            z: 3

            AppCheckBox {
                id: supportsCheckBox
                objectName: "viewerSupportsCheckBox"
                anchors.centerIn: parent
                text: qsTr("Supports")
                checked: root.supportColoringEnabled
                enabled: !viewer.loading
                onToggled: root.supportColoringEnabled = checked

                ToolTip.visible: hovered
                ToolTip.delay: 350
                ToolTip.text: qsTr("Analyze all native layers before building the 3D view. When disabled, the classic viewer path is used.")
            }
        }

        ViewerBuildModal {
            objectName: "viewerBuildModal"
            anchors.fill: parent
            running: viewer.loading
            progress: viewer.progress
            phaseText: viewer.loadingPhase
        }
    }

}
