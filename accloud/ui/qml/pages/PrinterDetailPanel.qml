import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Rectangle {
    id: root
    objectName: "deviceDetailsPanel"

    property var selectedPrinter: null
    property var selectedPrinterDetails: ({})
    property var selectedLiveJobData: ({})
    property bool loadingPrinterHistory: false
    property var printerHistoryModel: null
    property bool showDebugLabels: false
    property string printersEndpointPath: ""
    property string printersEndpointRawJson: ""
    property string selectedPrinterDetailsRawJson: ""
    property string selectedPrinterProjectsRawJson: ""
    property string selectedPrinterHelpUrlText: ""

    property var statusChipTextProvider: null
    property var progressTextProvider: null
    property var timeTextProvider: null
    property var unixTimeTextProvider: null
    property var printStatusTextProvider: null
    property var prettyJsonProvider: null
    property var localizedTextProvider: null
    property bool embeddedInTabsContainer: false

    signal cloudFileRequested(string printerId)
    signal localFileRequested(string printerId)

    function providerText(provider, arg, fallback) {
        return typeof provider === "function" ? String(provider(arg)) : fallback
    }

    function prettyPayload(rawPayload) {
        if (typeof root.prettyJsonProvider === "function")
            return String(root.prettyJsonProvider(rawPayload || ""))
        return String(rawPayload || "")
    }

    function nonNegativeInt(value) {
        var n = Number(value)
        if (!isFinite(n) || n < 0)
            return -1
        return Math.round(n)
    }

    function localizedText(value) {
        var raw = String(value === undefined || value === null ? "" : value)
        if (typeof root.localizedTextProvider === "function")
            return String(root.localizedTextProvider(raw))
        return raw
    }

    function localizedListText(values, maxItems) {
        if (!values || values.length === undefined || values.length <= 0)
            return ""
        var list = []
        var cap = Number(maxItems)
        if (!isFinite(cap) || cap <= 0)
            cap = values.length
        var upper = Math.min(values.length, cap)
        for (var i = 0; i < upper; ++i) {
            list.push(localizedText(values[i]))
        }
        return list.join(", ")
    }

    function layersProgressText(printer) {
        var printed = nonNegativeInt(printer ? printer.currentLayer : -1)
        var total = nonNegativeInt(printer ? printer.totalLayers : -1)
        if (printed < 0 || total <= 0)
            return "-"
        var normalizedPrinted = Math.max(0, Math.min(printed, total))
        var remaining = Math.max(0, total - normalizedPrinted)
        return qsTr("%1 printed | %2 remaining").arg(normalizedPrinted).arg(remaining)
    }

    function debugEndpointPayloadText() {
        var blocks = []

        var liveJobBlock = debugLiveJobDetailsText()
        if (liveJobBlock.length > 0)
            blocks.push(liveJobBlock)

        var printersPayload = String(root.printersEndpointRawJson || "").trim()
        if (printersPayload.length > 0) {
            blocks.push(qsTr("getPrinters + inline getProjects:\n%1")
                        .arg(prettyPayload(printersPayload)))
        }

        var detailsPayload = String(root.selectedPrinterDetailsRawJson || "").trim()
        if (detailsPayload.length > 0) {
            blocks.push(qsTr("v2/printer/info:\n%1")
                        .arg(prettyPayload(detailsPayload)))
        }

        var projectsPayload = String(root.selectedPrinterProjectsRawJson || "").trim()
        if (projectsPayload.length > 0) {
            blocks.push(qsTr("work/project/getProjects:\n%1")
                        .arg(prettyPayload(projectsPayload)))
        }

        if (blocks.length === 0)
            return qsTr("No debug endpoint payload captured.")
        return blocks.join("\n\n")
    }

    function debugLiveJobDetailsText() {
        var selectedPrinterId = String(root.selectedPrinter && root.selectedPrinter.id !== undefined
                                       ? root.selectedPrinter.id
                                       : "").trim()
        var selectedPrinterState = String(root.selectedPrinter && root.selectedPrinter.state !== undefined
                                          ? root.selectedPrinter.state
                                          : "").trim()

        var liveJob = root.selectedLiveJobData !== null && root.selectedLiveJobData !== undefined
                ? root.selectedLiveJobData
                : ({})

        function pickText(primary, fallback) {
            var first = String(primary || "").trim()
            if (first.length > 0)
                return first
            var second = String(fallback || "").trim()
            return second.length > 0 ? second : "-"
        }

        function pickInt(primary, fallback) {
            var p = Number(primary)
            if (isFinite(p) && p >= 0)
                return Math.round(p)
            var f = Number(fallback)
            if (isFinite(f) && f >= 0)
                return Math.round(f)
            return -1
        }

        var taskId = pickText(liveJob.taskId, "")
        var fileName = pickText(liveJob.currentFile,
                                pickText(liveJob.gcodeName,
                                         root.selectedPrinter ? root.selectedPrinter.currentFile : ""))
        var printStatusCode = pickInt(liveJob.printStatus, -1)
        var printStatusText = (printStatusCode >= 0 && typeof root.printStatusTextProvider === "function")
                ? root.printStatusTextProvider(printStatusCode)
                : "-"
        var progressRaw = pickInt(liveJob.progress,
                                  root.selectedPrinter ? root.selectedPrinter.progress : -1)
        var currentLayerRaw = pickInt(liveJob.currentLayer,
                                      root.selectedPrinter ? root.selectedPrinter.currentLayer : -1)
        var totalLayersRaw = pickInt(liveJob.totalLayers,
                                     root.selectedPrinter ? root.selectedPrinter.totalLayers : -1)
        var elapsedSecRaw = pickInt(liveJob.elapsedSec,
                                    root.selectedPrinter ? root.selectedPrinter.elapsedSec : -1)
        var remainingSecRaw = pickInt(liveJob.remainingSec,
                                      root.selectedPrinter ? root.selectedPrinter.remainingSec : -1)
        var reasonText = pickText(liveJob.reason, "")
        var startText = (typeof root.unixTimeTextProvider === "function")
                ? root.unixTimeTextProvider(pickInt(liveJob.createTime, 0))
                : "-"
        var endText = (typeof root.unixTimeTextProvider === "function")
                ? root.unixTimeTextProvider(pickInt(liveJob.endTime, 0))
                : "-"
        var elapsedText = (typeof root.timeTextProvider === "function")
                ? root.timeTextProvider(elapsedSecRaw)
                : "-"
        var remainingText = (typeof root.timeTextProvider === "function")
                ? root.timeTextProvider(remainingSecRaw)
                : "-"

        var lines = []
        lines.push("Live Job Details:")
        lines.push("  printer_id: " + (selectedPrinterId.length > 0 ? selectedPrinterId : "-"))
        lines.push("  printer_state: " + (selectedPrinterState.length > 0 ? selectedPrinterState : "-"))
        lines.push("  task_id: " + taskId)
        lines.push("  file: " + fileName)
        lines.push("  print_status: " + (printStatusCode >= 0 ? String(printStatusCode) : "-")
                   + " (" + String(printStatusText || "-") + ")")
        lines.push("  progress_raw: " + (progressRaw >= 0 ? String(progressRaw) : "-"))
        lines.push("  layers: printed=" + (currentLayerRaw >= 0 ? String(currentLayerRaw) : "-")
                   + " total=" + (totalLayersRaw >= 0 ? String(totalLayersRaw) : "-")
                   + " remaining="
                   + ((currentLayerRaw >= 0 && totalLayersRaw >= 0)
                      ? String(Math.max(0, totalLayersRaw - Math.min(currentLayerRaw, totalLayersRaw)))
                      : "-"))
        lines.push("  elapsed: " + String(elapsedText || "-")
                   + " (raw=" + (elapsedSecRaw >= 0 ? String(elapsedSecRaw) : "-") + ")")
        lines.push("  remaining: " + String(remainingText || "-")
                   + " (raw=" + (remainingSecRaw >= 0 ? String(remainingSecRaw) : "-") + ")")
        lines.push("  reason: " + reasonText)
        lines.push("  start: " + String(startText || "-"))
        lines.push("  end: " + String(endText || "-"))
        return lines.join("\n")
    }

    Layout.fillWidth: true
    Layout.fillHeight: true
    radius: embeddedInTabsContainer ? 0 : Theme.radiusControl
    color: Theme.bgSurface
    border.width: embeddedInTabsContainer ? 0 : Theme.borderWidth
    border.color: Theme.borderDefault

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Text {
            text: qsTr("Device Details")
            color: Theme.fgPrimary
            font.pixelSize: Theme.fontTitlePx
            font.bold: true
        }

        Text {
            visible: root.selectedPrinter === null
            text: qsTr("Select a printer to view details and remote print entrypoints.")
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontBodyPx
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            visible: root.selectedPrinter !== null
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusControl
                    color: Theme.bgWindow
                    border.width: Theme.borderWidth
                    border.color: Theme.borderSubtle

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Text {
                            text: String(root.selectedPrinter ? root.selectedPrinter.name : "-")
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.fontTitlePx
                            font.bold: true
                        }

                        Text {
                            text: qsTr("Model: ") + String(root.selectedPrinter ? root.selectedPrinter.model : "-")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontBodyPx
                        }

                        Text {
                            text: qsTr("Firmware: ")
                                  + (String(root.selectedPrinterDetails.firmwareVersion || "").length > 0
                                     ? String(root.selectedPrinterDetails.firmwareVersion)
                                     : "-")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontBodyPx
                        }

                        RowLayout {
                            spacing: 8

                            Text {
                                text: qsTr("Status:")
                                color: Theme.fgSecondary
                                font.pixelSize: Theme.fontBodyPx
                            }

                            StatusChip {
                                status: root.providerText(root.statusChipTextProvider,
                                                          root.selectedPrinter ? root.selectedPrinter.state : "READY",
                                                          qsTr("Ready"))
                            }
                        }

                        Text {
                            visible: root.selectedPrinterHelpUrlText.length > 0
                            text: qsTr("Help: ") + root.selectedPrinterHelpUrlText
                            color: Theme.accent
                            font.pixelSize: Theme.fontCaptionPx
                            elide: Text.ElideRight
                        }

                        Text {
                            text: qsTr("Current job: %1 | Progress: %2 | Layers: %3 | Elapsed: %4 | Remaining: %5")
                                .arg(root.selectedPrinter && String(root.selectedPrinter.currentFile || "").length > 0
                                     ? String(root.selectedPrinter.currentFile)
                                     : "-")
                                .arg(root.providerText(root.progressTextProvider,
                                                       root.selectedPrinter ? root.selectedPrinter.progress : -1,
                                                       "-"))
                                .arg(root.layersProgressText(root.selectedPrinter))
                                .arg(root.providerText(root.timeTextProvider,
                                                       root.selectedPrinter ? root.selectedPrinter.elapsedSec : -1,
                                                       "-"))
                                .arg(root.providerText(root.timeTextProvider,
                                                       root.selectedPrinter ? root.selectedPrinter.remainingSec : -1,
                                                       "-"))
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            text: qsTr("Print count: %1 | Total print time: %2 | Material used: %3")
                                .arg(String(root.selectedPrinterDetails.printCount || "").length > 0
                                     ? String(root.selectedPrinterDetails.printCount)
                                     : "-")
                                .arg(String(root.selectedPrinterDetails.printTotalTime || "").length > 0
                                     ? String(root.selectedPrinterDetails.printTotalTime)
                                     : "-")
                                .arg(String(root.selectedPrinterDetails.materialUsed || "").length > 0
                                     ? String(root.selectedPrinterDetails.materialUsed)
                                     : "-")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            visible: String(root.selectedPrinterDetails.helpUrl || "").length > 0
                            text: qsTr("Device help: ") + String(root.selectedPrinterDetails.helpUrl || "")
                            color: Theme.accent
                            font.pixelSize: Theme.fontCaptionPx
                            elide: Text.ElideRight
                        }

                        Text {
                            visible: String(root.selectedPrinterDetails.quickStartUrl || "").length > 0
                            text: qsTr("Quick start: ") + String(root.selectedPrinterDetails.quickStartUrl || "")
                            color: Theme.accent
                            font.pixelSize: Theme.fontCaptionPx
                            elide: Text.ElideRight
                        }

                        Text {
                            visible: Number(root.selectedPrinterDetails.tools ? root.selectedPrinterDetails.tools.length : 0) > 0
                            text: qsTr("Tools: ")
                                  + root.localizedListText(root.selectedPrinterDetails.tools, 6)
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            visible: Number(root.selectedPrinterDetails.advances ? root.selectedPrinterDetails.advances.length : 0) > 0
                            text: qsTr("Advanced: ")
                                  + root.localizedListText(root.selectedPrinterDetails.advances, 4)
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            wrapMode: Text.WordWrap
                        }

                        Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            AppButton {
                                text: qsTr("From Cloud File")
                                variant: "primary"
                                enabled: root.selectedPrinter !== null
                                onClicked: root.cloudFileRequested(String(root.selectedPrinter.id || ""))
                            }

                            AppButton {
                                text: qsTr("From Local File")
                                variant: "secondary"
                                onClicked: root.localFileRequested(
                                               String(root.selectedPrinter && root.selectedPrinter.id !== undefined
                                                      ? root.selectedPrinter.id
                                                      : ""))
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusControl
                    color: Theme.bgWindow
                    border.width: Theme.borderWidth
                    border.color: Theme.borderSubtle

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("Recent Jobs")
                            subtitle: root.loadingPrinterHistory ? qsTr("Loading...") : qsTr("Latest projects for this printer")
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 6
                            model: root.printerHistoryModel
                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                                active: true
                            }

                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 42
                                color: "transparent"

                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 2

                                    Text {
                                        Layout.fillWidth: true
                                        text: String(model.gcodeName || "-") + " • "
                                            + root.providerText(root.printStatusTextProvider, model.printStatus, "-")
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontCaptionPx
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("Task %1 | Start %2 | End %3")
                                            .arg(String(model.taskId || "-"))
                                            .arg(root.providerText(root.unixTimeTextProvider, model.createTime, "-"))
                                            .arg(root.providerText(root.unixTimeTextProvider, model.endTime, "-"))
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            footer: Text {
                                width: parent ? parent.width : 0
                                visible: root.printerHistoryModel && root.printerHistoryModel.count === 0
                                text: root.loadingPrinterHistory
                                      ? qsTr("Loading history...")
                                      : qsTr("No project history for this printer.")
                                color: Theme.fgSecondary
                                font.pixelSize: Theme.fontCaptionPx
                                horizontalAlignment: Text.AlignHCenter
                                padding: 10
                            }
                        }
                    }
                }
            }

            Rectangle {
                objectName: "endpointJsonPanel"
                visible: root.showDebugLabels
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                radius: Theme.radiusControl
                color: Theme.bgWindow
                border.width: Theme.borderWidth
                border.color: Theme.borderSubtle

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Endpoint responses (debug): ") + root.printersEndpointPath
                        color: Theme.warning
                        font.pixelSize: Theme.fontCaptionPx
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        TextArea {
                            readOnly: true
                            text: root.debugEndpointPayloadText()
                            wrapMode: TextEdit.NoWrap
                            color: Theme.fgPrimary
                            font.family: "monospace"
                            font.pixelSize: Theme.fontCaptionPx
                            background: Rectangle {
                                color: "transparent"
                            }
                        }
                    }
                }
            }
        }
    }
}
