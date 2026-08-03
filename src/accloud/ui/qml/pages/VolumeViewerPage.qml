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
        Layout.fillWidth: true
        spacing: Theme.gapRow

        Text {
            visible: !root.showSourceControls
            Layout.fillWidth: true
            text: root.displayFileName.length > 0 ? root.displayFileName : qsTr("PWSZ file")
            color: Theme.fgPrimary
            font.pixelSize: Theme.fontBodyPx
            font.bold: true
            elide: Text.ElideMiddle
        }

        AppTextField {
            id: pathField
            objectName: "viewerPathField"
            visible: root.showSourceControls
            Layout.fillWidth: true
            placeholderText: qsTr("Absolute path or file:/// URL to a .pwsz file")
            onAccepted: viewer.load()
        }

        AppButton {
            objectName: "viewerLoadButton"
            visible: root.showSourceControls
            text: viewer.loading ? qsTr("Loading…") : qsTr("Load PWSZ")
            variant: "primary"
            enabled: !viewer.loading && pathField.text.trim().length > 0
            onClicked: viewer.load()
        }

        ComboBox {
            id: samplingModeCombo
            objectName: "viewerSamplingModeCombo"
            Layout.preferredWidth: 230
            enabled: !viewer.loading
            model: [
                qsTr("Fast preview · 1 layer out of 2"),
                qsTr("Full detail · every layer")
            ]
            currentIndex: viewer.layerStep === 1 ? 1 : 0
            onActivated: function(index) {
                var nextStep = index === 1 ? 1 : 2
                if (viewer.layerStep === nextStep)
                    return
                viewer.layerStep = nextStep
                if (pathField.text.trim().length > 0)
                    viewer.load()
            }
        }

        AppButton {
            objectName: "viewerResetCameraButton"
            text: qsTr("Reset view")
            enabled: viewer.loadedChunkCount > 0
            onClicked: viewer.resetView()
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
        color: Theme.viewportBg
        border.width: Theme.borderWidth
        border.color: Theme.viewportBorder
        clip: true

        VolumeViewer {
            id: viewer
            objectName: "volumeViewerItem"
            anchors.fill: parent
            sourcePath: pathField.text
            backgroundColor: Theme.viewportBg
            meshColor: Theme.accent
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
                    text: viewer.machineName.length > 0
                          ? viewer.machineName
                          : qsTr("No PWSZ loaded")
                    color: Theme.viewportFg
                    font.pixelSize: Theme.fontBodyPx
                    font.bold: true
                }

                Text {
                    visible: viewer.totalLayers > 0
                    text: qsTr("%1 layers · %2 chunks · %3 triangles")
                          .arg(viewer.totalLayers)
                          .arg(viewer.loadedChunkCount)
                          .arg(viewer.triangleCount)
                    color: Theme.viewportFg
                    font.pixelSize: Theme.fontCaptionPx
                }

                Text {
                    visible: viewer.totalLayers > 0
                    text: viewer.layerStep === 1
                          ? qsTr("Mesh sampling: every layer")
                          : qsTr("Mesh sampling: 1 layer out of %1").arg(viewer.layerStep)
                    color: Theme.viewportFg
                    font.pixelSize: Theme.fontCaptionPx
                }

                Text {
                    visible: viewer.totalLayers > 0
                    text: qsTr("Mesh workers: %1").arg(viewer.workerCount)
                    color: Theme.viewportFg
                    font.pixelSize: Theme.fontCaptionPx
                }

                Text {
                    text: qsTr("Left drag: orbit · Right/Shift drag: pan · Wheel: zoom")
                    color: Theme.viewportFg
                    font.pixelSize: Theme.fontCaptionPx
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: viewer.loading && viewer.loadedChunkCount === 0
            color: "#66000000"

            Column {
                anchors.centerIn: parent
                spacing: 12

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: parent.parent.visible
                }

                Text {
                    text: qsTr("Decoding layers and building mesh… %1%").arg(Math.round(viewer.progress * 100))
                    color: Theme.viewportFg
                    font.pixelSize: Theme.fontBodyPx
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        visible: viewer.totalLayers > 0

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("Visible layers")
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontBodyPx
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Text {
                objectName: "viewerVisibleRangeLabel"
                text: qsTr("%1 to %2 · %3 layers · Z %4–%5 mm")
                      .arg(viewer.firstLayer)
                      .arg(viewer.lastLayer)
                      .arg(Math.max(0, viewer.lastLayer - viewer.firstLayer + 1))
                      .arg(((viewer.firstLayer - 1) * viewer.layerHeightMm).toFixed(2))
                      .arg((viewer.lastLayer * viewer.layerHeightMm).toFixed(2))
                color: Theme.fgSecondary
                font.pixelSize: Theme.fontCaptionPx
            }
        }

        RangeSlider {
            id: layerRangeSlider
            objectName: "viewerLayerRangeSlider"
            Layout.fillWidth: true
            from: 1
            to: Math.max(1, viewer.totalLayers)
            stepSize: 1
            snapMode: RangeSlider.SnapAlways
            enabled: viewer.totalLayers > 0

            first.value: Math.max(1, viewer.firstLayer)
            second.value: Math.max(first.value, viewer.lastLayer)

            first.onMoved: viewer.firstLayer = Math.round(first.value)
            second.onMoved: viewer.lastLayer = Math.round(second.value)
        }

        RowLayout {
            Layout.fillWidth: true

            SpinBox {
                id: firstLayerSpin
                objectName: "viewerFirstLayerSpin"
                from: 1
                to: Math.max(1, viewer.lastLayer)
                value: Math.max(1, viewer.firstLayer)
                editable: true
                onValueModified: viewer.firstLayer = value
            }

            Text {
                text: qsTr("to")
                color: Theme.fgSecondary
                font.pixelSize: Theme.fontBodyPx
            }

            SpinBox {
                id: lastLayerSpin
                objectName: "viewerLastLayerSpin"
                from: Math.max(1, viewer.firstLayer)
                to: Math.max(1, viewer.totalLayers)
                value: Math.max(1, viewer.lastLayer)
                editable: true
                onValueModified: viewer.lastLayer = value
            }

            Item { Layout.fillWidth: true }

            ProgressBar {
                Layout.preferredWidth: 220
                visible: viewer.loading
                from: 0
                to: 1
                value: viewer.progress
            }
        }
    }
}
