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
        return qsTr("%1 / %2 layers").arg(normalizedPrinted).arg(total)
    }

    function hasTextValue(value) {
        return String(value === undefined || value === null ? "" : value).trim().length > 0
    }

    function hasNonNegativeMetric(value) {
        return nonNegativeInt(value) >= 0
    }

    function currentFileText(printer) {
        if (printer && hasTextValue(printer.currentFile))
            return String(printer.currentFile).trim()
        return "-"
    }

    function currentFileImageSource() {
        var liveJob = root.selectedLiveJobData || ({})
        var details = root.selectedPrinterDetails || ({})
        var printer = root.selectedPrinter || ({})
        var historyActive = ({})
        if (root.printerHistoryModel && root.printerHistoryModel.count !== undefined) {
            for (var i = 0; i < root.printerHistoryModel.count; ++i) {
                var entry = root.printerHistoryModel.get(i)
                if (Number(entry.printStatus) === 1) {
                    historyActive = entry
                    break
                }
            }
            if (Object.keys(historyActive).length === 0 && root.printerHistoryModel.count > 0)
                historyActive = root.printerHistoryModel.get(0)
        }

        var candidates = [
            liveJob.img,
            liveJob.imgRaw,
            liveJob.image,
            liveJob.preview,
            liveJob.thumbnailUrl,
            details.img,
            details.imgRaw,
            details.image,
            details.preview,
            details.thumbnailUrl,
            printer.img,
            printer.imgRaw,
            printer.image,
            printer.preview,
            printer.thumbnailUrl,
            historyActive.img,
            historyActive.imgRaw,
            historyActive.image,
            historyActive.preview,
            historyActive.thumbnailUrl
        ]
        for (var i = 0; i < candidates.length; ++i) {
            if (hasTextValue(candidates[i]))
                return String(candidates[i]).trim()
        }
        return ""
    }

    function normalizedImageSource(raw) {
        var value = String(raw === undefined || raw === null ? "" : raw).trim()
        if (value.length <= 0)
            return ""
        var lowered = value.toLowerCase()
        if (lowered.indexOf("data:image/") === 0)
            return value
        if (lowered.indexOf("file://") === 0 || lowered.indexOf("http://") === 0 || lowered.indexOf("https://") === 0)
            return value
        if (value.indexOf("//") === 0)
            return "https:" + value
        if (value.indexOf("/") === 0)
            return "https://cloud-universe.anycubic.com" + value
        if (lowered.indexOf(".jpg") !== -1 || lowered.indexOf(".jpeg") !== -1 || lowered.indexOf(".png") !== -1 || lowered.indexOf(".webp") !== -1)
            return "https://" + value
        return value
    }

    function imageStatusText(statusCode) {
        if (statusCode === Image.Ready)
            return "Ready"
        if (statusCode === Image.Loading)
            return "Loading"
        if (statusCode === Image.Error)
            return "Error"
        return "Null"
    }

    function printCountText(details) {
        if (!details)
            return "-"
        var value = Number(details.printCount)
        if (isFinite(value) && value >= 0)
            return String(Math.round(value))
        return hasTextValue(details.printCount) ? String(details.printCount).trim() : "-"
    }

    function normalizedTotalPrintTimeText(details) {
        if (!details || !hasTextValue(details.printTotalTime))
            return "-"
        var raw = String(details.printTotalTime).trim()
        var compact = raw.toLowerCase().replace(/\s+/g, "")
        var match = compact.match(/^(\d+)(?:h|hour|hours)(\d+)(?:m|min|mins|minute|minutes)$/)
        if (match)
            return qsTr("%1 h %2 min").arg(Number(match[1])).arg(Number(match[2]))
        return raw
    }

    function normalizedMaterialUsedText(details) {
        if (!details || !hasTextValue(details.materialUsed))
            return "-"
        var numeric = Number(details.materialUsed)
        if (isFinite(numeric) && numeric >= 0) {
            var rounded = Math.round(numeric * 100) / 100
            var text = String(rounded)
            if (text.indexOf(".") >= 0)
                text = text.replace(/0+$/, "").replace(/\.$/, "")
            return qsTr("%1 ml").arg(text)
        }
        var raw = String(details.materialUsed).trim()
        if (/ml$/i.test(raw))
            return raw.replace(/\s*ml$/i, " ml")
        return raw
    }

    function printerStatusCode(printer) {
        var state = String(printer && printer.state !== undefined ? printer.state : "").toUpperCase().trim()
        if (state === "PRINTING")
            return 1
        if (state === "ERROR")
            return 3
        return 0
    }

    function liveJobStatusCode(liveJob) {
        if (!liveJob)
            return -1
        var value = Number(liveJob.printStatus)
        if (isFinite(value))
            return Math.round(value)
        return -1
    }

    function isPrinterPrinting() {
        var liveStatus = liveJobStatusCode(root.selectedLiveJobData)
        if (liveStatus === 1)
            return true
        return printerStatusCode(root.selectedPrinter) === 1
    }

    function printerInfoMode() {
        if (!root.selectedPrinter)
            return "none"
        return isPrinterPrinting() ? "printing" : "basic"
    }

    function releaseFilmText(details) {
        if (!details)
            return "-"
        var candidates = [
            details.releaseFilm,
            details.release_film,
            details.releaseFilmStatus,
            details.release_film_status,
            details.fepStatus,
            details.fep_status
        ]
        for (var i = 0; i < candidates.length; ++i) {
            if (hasTextValue(candidates[i]))
                return String(candidates[i]).trim()
        }
        return "-"
    }

    function recentJobStatusInfo(printStatus) {
        var code = Number(printStatus)
        if (isFinite(code))
            code = Math.round(code)
        else
            code = -1

        if (code === 1)
            return { label: qsTr("In progress"), bg: Theme.info, fg: Theme.bgWindow }
        if (code === 2)
            return { label: qsTr("Finished"), bg: Theme.success, fg: Theme.bgWindow }
        if (code === 3)
            return { label: qsTr("Failed"), bg: Theme.error, fg: Theme.bgWindow }
        if (code === 4)
            return { label: qsTr("Canceled"), bg: Theme.warning, fg: Theme.bgWindow }
        return { label: qsTr("Unknown"), bg: Theme.borderSubtle, fg: Theme.fgPrimary }
    }

    function historyDurationText(startEpoch, endEpoch) {
        var startValue = Number(startEpoch)
        var endValue = Number(endEpoch)
        if (!isFinite(startValue) || !isFinite(endValue) || startValue <= 0 || endValue <= 0 || endValue < startValue)
            return "-"
        if (typeof root.timeTextProvider === "function")
            return String(root.timeTextProvider(endValue - startValue))
        return "-"
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
                            visible: root.printerInfoMode() === "basic"
                            text: qsTr("Firmware: ")
                                  + (String(root.selectedPrinterDetails.firmwareVersion || "").length > 0
                                     ? String(root.selectedPrinterDetails.firmwareVersion)
                                     : "-")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontBodyPx
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                radius: Theme.radiusControl
                                color: Theme.cardAlt
                                border.width: Theme.borderWidth
                                border.color: Theme.borderSubtle
                                Layout.fillWidth: true
                                implicitHeight: 34

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 8

                                    Text {
                                        text: qsTr("Status")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                        font.bold: true
                                    }

                                    StatusChip {
                                        status: root.providerText(root.statusChipTextProvider,
                                                                  root.selectedPrinter ? root.selectedPrinter.state : "READY",
                                                                  qsTr("Ready"))
                                    }
                                }
                            }
                        }

                        Rectangle {
                            visible: root.printerInfoMode() === "printing"
                            Layout.fillWidth: true
                            radius: Theme.radiusControl
                            color: Theme.bgSurface
                            border.width: Theme.borderWidth
                            border.color: Theme.borderSubtle
                            implicitHeight: 220

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 200
                                    Layout.preferredHeight: 200
                                    radius: Theme.radiusControl
                                    color: Theme.bgWindow
                                    border.width: Theme.borderWidth
                                    border.color: Theme.borderSubtle

                                    Image {
                                        id: currentFilePreviewImage
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        source: root.normalizedImageSource(root.currentFileImageSource())
                                        fillMode: Image.PreserveAspectFit
                                        asynchronous: true
                                        cache: true
                                        smooth: true
                                        visible: source.toString().length > 0 && status === Image.Ready
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        visible: root.currentFileImageSource().length <= 0 || currentFilePreviewImage.status === Image.Error
                                        text: qsTr("No preview")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        text: qsTr("Current File")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                        font.bold: true
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.currentFileText(root.selectedPrinter)
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontBodyPx
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("Image source: %1").arg(
                                                  root.currentFileImageSource().length > 0
                                                  ? root.currentFileImageSource()
                                                  : qsTr("(empty)"))
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                        wrapMode: Text.WrapAnywhere
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("Image status: %1 (%2)").arg(
                                                  currentFilePreviewImage.status).arg(
                                                  root.imageStatusText(currentFilePreviewImage.status))
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("Image source normalized: %1").arg(
                                                  currentFilePreviewImage.source.toString().length > 0
                                                  ? currentFilePreviewImage.source.toString()
                                                  : qsTr("(empty)"))
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                        wrapMode: Text.WrapAnywhere
                                    }
                                }
                            }
                        }

                        GridLayout {
                            visible: root.printerInfoMode() === "printing"
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8

                            Rectangle {
                                visible: root.hasNonNegativeMetric(root.selectedPrinter ? root.selectedPrinter.progress : -1)
                                Layout.fillWidth: true
                                radius: Theme.radiusControl
                                color: Theme.bgSurface
                                border.width: Theme.borderWidth
                                border.color: Theme.borderSubtle
                                implicitHeight: 48

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 2

                                    Text {
                                        text: qsTr("Progress")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        text: root.providerText(root.progressTextProvider,
                                                                root.selectedPrinter ? root.selectedPrinter.progress : -1,
                                                                "-")
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontBodyPx
                                        font.bold: true
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                radius: Theme.radiusControl
                                color: Theme.bgSurface
                                border.width: Theme.borderWidth
                                border.color: Theme.borderSubtle
                                implicitHeight: 48

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 2

                                    Text {
                                        text: qsTr("Layers")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        text: root.layersProgressText(root.selectedPrinter)
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontBodyPx
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                radius: Theme.radiusControl
                                color: Theme.bgSurface
                                border.width: Theme.borderWidth
                                border.color: Theme.borderSubtle
                                implicitHeight: 48

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 2

                                    Text {
                                        text: qsTr("Elapsed")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        text: root.providerText(root.timeTextProvider,
                                                                root.selectedPrinter ? root.selectedPrinter.elapsedSec : -1,
                                                                "-")
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontBodyPx
                                        font.bold: true
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                radius: Theme.radiusControl
                                color: Theme.bgSurface
                                border.width: Theme.borderWidth
                                border.color: Theme.borderSubtle
                                implicitHeight: 48

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 2

                                    Text {
                                        text: qsTr("Remaining")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        text: root.providerText(root.timeTextProvider,
                                                                root.selectedPrinter ? root.selectedPrinter.remainingSec : -1,
                                                                "-")
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontBodyPx
                                        font.bold: true
                                    }
                                }
                            }
                        }

                        GridLayout {
                            visible: root.printerInfoMode() === "basic"
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8

                            Rectangle {
                                visible: root.printCountText(root.selectedPrinterDetails) !== "-"
                                Layout.fillWidth: true
                                radius: Theme.radiusControl
                                color: Theme.bgSurface
                                border.width: Theme.borderWidth
                                border.color: Theme.borderSubtle
                                implicitHeight: 48

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 2

                                    Text {
                                        text: qsTr("Print Count")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        text: root.printCountText(root.selectedPrinterDetails)
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontBodyPx
                                        font.bold: true
                                    }
                                }
                            }

                            Rectangle {
                                visible: root.normalizedTotalPrintTimeText(root.selectedPrinterDetails) !== "-"
                                Layout.fillWidth: true
                                radius: Theme.radiusControl
                                color: Theme.bgSurface
                                border.width: Theme.borderWidth
                                border.color: Theme.borderSubtle
                                implicitHeight: 48

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 2

                                    Text {
                                        text: qsTr("Total Print Time")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        text: root.normalizedTotalPrintTimeText(root.selectedPrinterDetails)
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontBodyPx
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            Rectangle {
                                visible: root.normalizedMaterialUsedText(root.selectedPrinterDetails) !== "-"
                                Layout.fillWidth: true
                                radius: Theme.radiusControl
                                color: Theme.bgSurface
                                border.width: Theme.borderWidth
                                border.color: Theme.borderSubtle
                                implicitHeight: 48

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 2

                                    Text {
                                        text: qsTr("Material Used")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        text: root.normalizedMaterialUsedText(root.selectedPrinterDetails)
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontBodyPx
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            Rectangle {
                                visible: root.selectedPrinter && root.hasTextValue(root.selectedPrinter.type)
                                Layout.fillWidth: true
                                radius: Theme.radiusControl
                                color: Theme.bgSurface
                                border.width: Theme.borderWidth
                                border.color: Theme.borderSubtle
                                implicitHeight: 48

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 2

                                    Text {
                                        text: qsTr("Printer Type")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        text: String(root.selectedPrinter ? root.selectedPrinter.type : "-")
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontBodyPx
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            Rectangle {
                                visible: root.releaseFilmText(root.selectedPrinterDetails) !== "-"
                                Layout.fillWidth: true
                                radius: Theme.radiusControl
                                color: Theme.bgSurface
                                border.width: Theme.borderWidth
                                border.color: Theme.borderSubtle
                                implicitHeight: 48

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 2

                                    Text {
                                        text: qsTr("Release Film")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        text: root.releaseFilmText(root.selectedPrinterDetails)
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontBodyPx
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }

                        Text {
                            visible: root.printerInfoMode() === "none"
                            text: qsTr("Waiting for printer data...")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
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
                                height: 74
                                radius: Theme.radiusControl
                                color: Theme.bgSurface
                                border.width: Theme.borderWidth
                                border.color: Theme.borderSubtle

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(model.gcodeName || "-")
                                            color: Theme.fgPrimary
                                            font.pixelSize: Theme.fontBodyPx
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        Rectangle {
                                            readonly property var statusInfo: root.recentJobStatusInfo(model.printStatus)
                                            radius: Theme.radiusControl
                                            color: statusInfo.bg
                                            implicitHeight: 22
                                            implicitWidth: statusText.implicitWidth + 12

                                            Text {
                                                id: statusText
                                                anchors.centerIn: parent
                                                text: parent.statusInfo.label
                                                color: parent.statusInfo.fg
                                                font.pixelSize: Theme.fontCaptionPx
                                                font.bold: true
                                            }
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("Start %1 | End %2 | Duration %3")
                                            .arg(root.providerText(root.unixTimeTextProvider, model.createTime, "-"))
                                            .arg(root.providerText(root.unixTimeTextProvider, model.endTime, "-"))
                                            .arg(root.historyDurationText(model.createTime, model.endTime))
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("Task %1")
                                            .arg(String(model.taskId || "-"))
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
