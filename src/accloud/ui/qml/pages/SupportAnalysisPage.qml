import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import "../components"
import "../components/Theme.js" as Theme

AppPageFrame {
    id: root
    objectName: "supportAnalysisPage"
    property var analysisBridge: null
    property bool embeddedAnalysisInTabsContainer: false
    property int workerCount: 4
    property string palettePreset: "technical_cyan"
    property color partColor: "#FF7A66"
    property color supportColor: "#55B7C6"
    property color viewportColor: "#111827"
    property string jsonMode: "decisions"
    property bool syncingLayer: false
    embeddedInTabsContainer: root.embeddedAnalysisInTabsContainer
    showSectionHeader: false

    function localPathFromUrl(value) {
        var text = String(value || "")
        if (text.indexOf("file://") === 0) {
            text = decodeURIComponent(text.replace(/^file:\/\//, ""))
            if (Qt.platform.os === "windows" && text.charAt(0) === "/")
                text = text.substring(1)
        }
        return text
    }

    function refreshViewer() {
        if (!root.analysisBridge || root.analysisBridge.layerCount <= 0)
            return
        var source = String(root.analysisBridge.sourcePath || "")
        if (source.length > 0 && viewerPane.sourcePath !== source)
            viewerPane.loadSource(source, source.split(/[\\/]/).pop())
        root.syncingLayer = true
        viewerPane.showThroughLayer(root.analysisBridge.currentLayer)
        root.syncingLayer = false
    }

    function selectedJson() {
        if (!root.analysisBridge)
            return "{}"
        if (root.jsonMode === "analysis")
            return String(root.analysisBridge.analysisJson || "{}")
        if (root.jsonMode === "layer")
            return String(root.analysisBridge.currentLayerJson || "{}")
        return String(root.analysisBridge.currentDecisionJson || "[]")
    }

    FileDialog {
        id: pwszDialog
        objectName: "supportAnalysisPwszDialog"
        title: qsTr("Select the PWSZ part to analyze")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("PWSZ files (*.pwsz)"), qsTr("All files (*)")]
        onAccepted: {
            sourceField.text = root.localPathFromUrl(selectedFile)
            if (root.analysisBridge)
                root.analysisBridge.analyze(sourceField.text)
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Theme.gapRow

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapRow

            AppTextField {
                id: sourceField
                objectName: "supportAnalysisSourceField"
                Layout.fillWidth: true
                placeholderText: qsTr("Select a local .pwsz file")
                text: root.analysisBridge ? String(root.analysisBridge.sourcePath || "") : ""
                onAccepted: {
                    if (root.analysisBridge)
                        root.analysisBridge.analyze(text)
                }
            }

            AppButton {
                objectName: "supportAnalysisSelectButton"
                text: qsTr("Select part")
                onClicked: pwszDialog.open()
            }

            AppButton {
                objectName: "supportAnalysisRunButton"
                text: root.analysisBridge && root.analysisBridge.running
                      ? qsTr("Analyzing…")
                      : qsTr("Analyze")
                variant: "primary"
                enabled: root.analysisBridge
                         && !root.analysisBridge.running
                         && sourceField.text.trim().length > 0
                onClicked: root.analysisBridge.analyze(sourceField.text)
            }

            AppButton {
                objectName: "supportAnalysisCancelButton"
                text: qsTr("Cancel")
                visible: root.analysisBridge && root.analysisBridge.running
                onClicked: root.analysisBridge.cancel()
            }
        }

        ErrorBanner {
            Layout.fillWidth: true
            visible: root.analysisBridge
                     && String(root.analysisBridge.errorString || "").length > 0
            message: root.analysisBridge ? String(root.analysisBridge.errorString || "") : ""
            severity: "danger"
            operationId: "support.analysis"
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.analysisBridge
                     && (root.analysisBridge.running || root.analysisBridge.layerCount > 0)
            spacing: Theme.gapRow

            ProgressBar {
                objectName: "supportAnalysisProgress"
                Layout.fillWidth: true
                from: 0
                to: 1
                value: root.analysisBridge ? root.analysisBridge.progress : 0
            }

            Text {
                text: root.analysisBridge ? String(root.analysisBridge.phase || "") : ""
                color: Theme.fgSecondary
                font.pixelSize: Theme.fontCaptionPx
            }

            Text {
                visible: root.analysisBridge && root.analysisBridge.bundlePath.length > 0
                text: root.analysisBridge ? String(root.analysisBridge.bundlePath || "") : ""
                color: Theme.fgMuted
                font.pixelSize: Theme.fontCaptionPx
                elide: Text.ElideMiddle
                Layout.maximumWidth: 360
            }
        }

        SplitView {
            id: verticalSplit
            objectName: "supportAnalysisVerticalSplit"
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical

            SplitView {
                id: upperSplit
                objectName: "supportAnalysisUpperSplit"
                SplitView.fillWidth: true
                SplitView.fillHeight: true
                SplitView.minimumHeight: 380
                orientation: Qt.Horizontal

                VolumeViewerPage {
                    id: viewerPane
                    objectName: "supportAnalysisViewer"
                    SplitView.preferredWidth: upperSplit.width * 0.56
                    SplitView.fillHeight: true
                    showSourceControls: false
                    showViewerHeader: false
                    embeddedViewerInTabsContainer: true
                    workerCount: root.workerCount
                    palettePreset: root.palettePreset
                    partColor: root.partColor
                    supportColor: root.supportColor
                    viewportColor: root.viewportColor
                    supportColoringEnabled: true
                    onLastLayerChanged: {
                        if (!root.syncingLayer && root.analysisBridge
                                && totalLayers > 0) {
                            root.analysisBridge.setCurrentLayer(lastLayer)
                        }
                    }
                }

                Rectangle {
                    objectName: "supportAnalysisSlicePanel"
                    SplitView.fillWidth: true
                    SplitView.fillHeight: true
                    SplitView.minimumWidth: 360
                    color: Theme.bgSurfaceAlt
                    border.width: Theme.borderWidth
                    border.color: Theme.borderDefault
                    radius: Theme.radiusControl

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.paddingCard
                        spacing: Theme.gapRow

                        RowLayout {
                            Layout.fillWidth: true

                            AppButton {
                                objectName: "supportAnalysisPreviousLayerButton"
                                text: qsTr("Previous")
                                enabled: root.analysisBridge
                                         && root.analysisBridge.currentLayer > 1
                                onClicked: root.analysisBridge.setCurrentLayer(
                                               root.analysisBridge.currentLayer - 1)
                            }

                            Slider {
                                id: layerSlider
                                objectName: "supportAnalysisLayerSlider"
                                Layout.fillWidth: true
                                from: 1
                                to: root.analysisBridge
                                    ? Math.max(1, root.analysisBridge.layerCount)
                                    : 1
                                stepSize: 1
                                value: root.analysisBridge
                                       ? root.analysisBridge.currentLayer
                                       : 1
                                enabled: root.analysisBridge
                                         && root.analysisBridge.layerCount > 0
                                onMoved: root.analysisBridge.setCurrentLayer(Math.round(value))
                            }

                            Text {
                                text: root.analysisBridge
                                      ? qsTr("Layer %1 / %2")
                                            .arg(root.analysisBridge.currentLayer)
                                            .arg(root.analysisBridge.layerCount)
                                      : qsTr("No analysis")
                                color: Theme.fgPrimary
                                font.bold: true
                            }

                            AppButton {
                                objectName: "supportAnalysisNextLayerButton"
                                text: qsTr("Next")
                                enabled: root.analysisBridge
                                         && root.analysisBridge.currentLayer
                                            < root.analysisBridge.layerCount
                                onClicked: root.analysisBridge.setCurrentLayer(
                                               root.analysisBridge.currentLayer + 1)
                            }
                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            Image {
                                id: diagnosticImage
                                objectName: "supportAnalysisDiagnosticImage"
                                width: Math.max(parent.width, implicitWidth)
                                height: Math.max(parent.height, implicitHeight)
                                source: root.analysisBridge
                                        ? root.analysisBridge.currentImageUrl
                                        : ""
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                cache: false
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Diagnostic: raw mask | semantic result | decisions. "
                                       + "Red = part, cyan = support, amber = raft; "
                                       + "yellow = candidate contact, magenta = confirmed contact, purple = mixed component.")
                            color: Theme.fgSecondary
                            wrapMode: Text.WordWrap
                            font.pixelSize: Theme.fontCaptionPx
                        }
                    }
                }
            }

            Rectangle {
                objectName: "supportAnalysisJsonPanel"
                SplitView.preferredHeight: 260
                SplitView.minimumHeight: 160
                SplitView.fillWidth: true
                color: Theme.bgSurfaceAlt
                border.width: Theme.borderWidth
                border.color: Theme.borderDefault
                radius: Theme.radiusControl

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.paddingCard
                    spacing: Theme.gapRow

                    RowLayout {
                        Layout.fillWidth: true

                        AppButton {
                            objectName: "supportAnalysisDecisionJsonButton"
                            text: qsTr("Decisions")
                            variant: root.jsonMode === "decisions" ? "primary" : "secondary"
                            onClicked: root.jsonMode = "decisions"
                        }

                        AppButton {
                            objectName: "supportAnalysisLayerJsonButton"
                            text: qsTr("Layer")
                            variant: root.jsonMode === "layer" ? "primary" : "secondary"
                            onClicked: root.jsonMode = "layer"
                        }

                        AppButton {
                            objectName: "supportAnalysisGlobalJsonButton"
                            text: qsTr("Global summary")
                            variant: root.jsonMode === "analysis" ? "primary" : "secondary"
                            onClicked: root.jsonMode = "analysis"
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: root.analysisBridge && root.analysisBridge.layerCount > 0
                                  ? qsTr("Surface comparisons and decision reasons for layer %1")
                                        .arg(root.analysisBridge.currentLayer)
                                  : qsTr("No JSON loaded")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        TextArea {
                            objectName: "supportAnalysisJsonText"
                            text: root.selectedJson()
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.NoWrap
                            font.family: "monospace"
                            color: Theme.fgPrimary
                            background: Rectangle { color: Theme.bgInput }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: root.analysisBridge
        enabled: root.analysisBridge !== null
        function onBundleChanged() { root.refreshViewer() }
        function onCurrentLayerChanged() { root.refreshViewer() }
    }
}
