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
    property bool localFilePrintEnabled: true
    property var printerHistoryModel: null
    property bool showDebugLabels: false
    property string printersEndpointPath: ""
    property string printersEndpointRawJson: ""
    property string selectedPrinterDetailsRawJson: ""
    property string selectedPrinterProjectsRawJson: ""
    readonly property string currentPreviewSource: normalizedImageSource(currentFileImageSource())

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

    function printerDisplayStatus() {
        if (root.selectedPrinter) {
            var mqttState = String(root.selectedPrinter.mqttPrintState || "").trim()
            if (mqttState.length > 0 && mqttState.toLowerCase() !== "finished")
                return mqttState
            var details = root.selectedPrinter.details
            if (details) {
                mqttState = String(details.mqttPrintState || "").trim()
                if (mqttState.length > 0 && mqttState.toLowerCase() !== "finished")
                    return mqttState
            }
        }
        return root.providerText(root.statusChipTextProvider,
                                 root.selectedPrinter ? root.selectedPrinter.state : "READY",
                                 qsTr("Ready"))
    }

    function selectedMqttDetails() {
        if (root.selectedPrinter && root.selectedPrinter.details)
            return root.selectedPrinter.details
        return ({})
    }

    function selectedMqttField(name, fallback) {
        if (root.selectedPrinter && root.selectedPrinter[name] !== undefined
                && root.selectedPrinter[name] !== null
                && String(root.selectedPrinter[name]).trim().length > 0)
            return root.selectedPrinter[name]
        var details = selectedMqttDetails()
        if (details && details[name] !== undefined && details[name] !== null
                && String(details[name]).trim().length > 0)
            return details[name]
        return fallback
    }

    function checkIssueKeys(mapValue) {
        var out = []
        if (!mapValue)
            return out
        var keys = Object.keys(mapValue)
        for (var i = 0; i < keys.length; ++i) {
            var value = Number(mapValue[keys[i]])
            if (isFinite(value) && value !== 0 && value !== -1 && value !== -2)
                out.push(keys[i] + "=" + String(mapValue[keys[i]]))
        }
        return out
    }

    function checkWaitingKeys(mapValue) {
        var out = []
        if (!mapValue)
            return out
        var keys = Object.keys(mapValue)
        for (var i = 0; i < keys.length; ++i) {
            var value = Number(mapValue[keys[i]])
            if (isFinite(value) && value === -1)
                out.push(keys[i] + "=" + String(mapValue[keys[i]]))
        }
        return out
    }

    function mergedCheckIssues() {
        var details = selectedMqttDetails()
        return checkIssueKeys(details.mqttHardwareChecks || ({}))
                .concat(checkIssueKeys(details.mqttAutoChecks || ({})))
    }

    function mergedCheckWaiting() {
        var details = selectedMqttDetails()
        return checkWaitingKeys(details.mqttHardwareChecks || ({}))
                .concat(checkWaitingKeys(details.mqttAutoChecks || ({})))
    }

    function hasCheckData() {
        var details = selectedMqttDetails()
        return Object.keys(details.mqttHardwareChecks || ({})).length > 0
                || Object.keys(details.mqttAutoChecks || ({})).length > 0
    }

    function checkStatus() {
        var details = selectedMqttDetails()
        var resinStatus = String(details.mqttResinStatus || selectedMqttField("mqttResinStatus", "") || "").trim()
        if (mergedCheckIssues().length > 0 || resinStatus === "stop")
            return qsTr("stop")
        if (resinStatus === "warning")
            return qsTr("warning")
        if (mergedCheckWaiting().length > 0)
            return qsTr("wait")
        if (hasCheckData() || resinStatus === "done")
            return qsTr("done")
        return qsTr("pending")
    }

    function checkSummary(stage) {
        var details = selectedMqttDetails()
        var issues = mergedCheckIssues()
        if (issues.length > 0)
            return issues.join(", ")
        var waiting = mergedCheckWaiting()
        if (waiting.length > 0)
            return waiting.join(", ")
        var resinStatus = String(details.mqttResinStatus || selectedMqttField("mqttResinStatus", "") || "").trim()
        if (resinStatus === "stop" || resinStatus === "warning") {
            var resinMessage = String(details.mqttResinMessage || selectedMqttField("mqttResinMessage", "") || "").trim()
            return resinMessage.length > 0 ? resinMessage : resinStatus
        }
        if (resinStatus === "resin fill")
            return qsTr("resin fill")
        if (stage === "checking")
            return qsTr("hardware")
        if (stage === "preheating")
            return qsTr("preheat")
        if (stage === "printing" || stage === "finished")
            return qsTr("done")
        if (hasCheckData() || resinStatus === "done")
            return qsTr("done")
        return qsTr("pending")
    }

    function currentCheckIssues() {
        var details = selectedMqttDetails()
        var issues = []
        var checkIssues = mergedCheckIssues()
        if (checkIssues.length > 0)
            issues.push(qsTr("Check: %1").arg(checkIssues.join(", ")))
        var resinMessage = String(details.mqttResinMessage || selectedMqttField("mqttResinMessage", "") || "").trim()
        var resinStatus = String(details.mqttResinStatus || selectedMqttField("mqttResinStatus", "") || "").trim()
        if (resinMessage.length > 0 && (resinStatus === "stop" || resinStatus === "warning"))
            issues.push(qsTr("Resin: %1").arg(resinMessage))
        var resinDetails = []
        var resinPhase = String(details.mqttResinPhase || "").trim()
        var resinPrePrint = String(details.mqttResinPrePrintFillStatus || "").trim()
        var resinRuntime = String(details.mqttResinRuntimeTopupStatus || "").trim()
        var resinBottle = String(details.mqttResinBottleStatus || "").trim()
        var resinVat = String(details.mqttResinVatStatus || "").trim()
        var resinCode = String(details.mqttResinLastFeedCode || "").trim()
        if (resinPhase.length > 0)
            resinDetails.push(qsTr("phase=%1").arg(resinPhase))
        if (resinPrePrint.length > 0)
            resinDetails.push(qsTr("pre-print=%1").arg(resinPrePrint))
        if (resinRuntime.length > 0)
            resinDetails.push(qsTr("runtime=%1").arg(resinRuntime))
        if (resinBottle.length > 0)
            resinDetails.push(qsTr("bottle=%1").arg(resinBottle))
        if (resinVat.length > 0)
            resinDetails.push(qsTr("vat=%1").arg(resinVat))
        if (resinCode.length > 0 && resinCode !== "-1")
            resinDetails.push(qsTr("code=%1").arg(resinCode))
        if (resinDetails.length > 0 && (resinStatus === "stop" || resinStatus === "warning"))
            issues.push(resinDetails.join(" | "))
        return issues
    }

    function openCheckErrorDialogIfNeeded() {
        var issues = currentCheckIssues()
        if (issues.length <= 0)
            return
        checkErrorDialog.detailsText = issues.join("\n")
        checkErrorDialog.open()
    }

    function workflowStepStatus(stage, printState, step) {
        var stageOrder = {
            "command_sent": 1,
            "downloading": 2,
            "downloaded": 3,
            "loaded": 4,
            "checking": 5,
            "preheating": 6,
            "printing": 7
        }
        var targetOrder = stageOrder[step] || 0
        var currentOrder = stageOrder[stage] || 0
        if (stage === step)
            return qsTr("active")
        if (step === "checking" && printState === "monitoring")
            return qsTr("active")
        if (step === "printing" && (printState === "waiting" || printState === "resuming" || printState === "resumed"))
            return printState
        if (targetOrder > 0 && currentOrder > targetOrder)
            return qsTr("done")
        return qsTr("pending")
    }

    function workflowStatusRows() {
        var details = selectedMqttDetails()
        var stage = String(selectedMqttField("mqttJobStage", "") || "").trim()
        var printState = String(selectedMqttField("mqttPrintState", "") || "").trim()
        var activeTaskId = String(selectedMqttField("mqttActiveTaskId", "") || "").trim()
        var downloadProgress = nonNegativeInt(selectedMqttField("mqttDownloadProgress", -1))
        var checksStatus = checkStatus()
        if (checksStatus === qsTr("pending")
                && (stage === "checking" || stage === "preheating" || printState === "monitoring"))
            checksStatus = qsTr("active")
        if (checksStatus === qsTr("pending") && (stage === "printing" || stage === "finished"))
            checksStatus = qsTr("done")
        var rows = [
            {
                "label": qsTr("Task"),
                "status": workflowStepStatus(stage, printState, "command_sent"),
                "value": activeTaskId.length > 0 ? activeTaskId : qsTr("HTTPS accepted")
            },
            {
                "label": qsTr("Download"),
                "status": workflowStepStatus(stage, printState, downloadProgress >= 100 ? "downloaded" : "downloading"),
                "value": downloadProgress >= 0 ? qsTr("%1 %").arg(downloadProgress) : "-"
            },
            {
                "label": qsTr("Loaded"),
                "status": workflowStepStatus(stage, printState, "loaded"),
                "value": root.currentFileText(root.selectedPrinter)
            },
            {
                "label": qsTr("Check"),
                "status": checksStatus,
                "value": checkSummary(stage)
            },
            {
                "label": qsTr("Printing"),
                "status": workflowStepStatus(stage, printState, "printing"),
                "value": root.layersProgressText(root.selectedPrinter)
            }
        ]
        return rows
    }

    function hasActiveWorkflowStatus() {
        var stage = String(selectedMqttField("mqttJobStage", "") || "").trim()
        var printState = String(selectedMqttField("mqttPrintState", "") || "").trim()
        if (stage === "command_sent"
                || stage === "downloading"
                || stage === "downloaded"
                || stage === "loaded"
                || stage === "checking"
                || stage === "preheating"
                || stage === "printing")
            return true
        if (printState === "downloading"
                || printState === "monitoring"
                || printState === "preheating"
                || printState === "printing"
                || printState === "waiting"
                || printState === "pausing"
                || printState === "paused"
                || printState === "resuming"
                || printState === "resumed")
            return true
        return false
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

    function hasMeaningfulDetailValue(value) {
        if (!hasTextValue(value))
            return false
        return String(value).trim() !== "-"
    }

    function hasNonNegativeMetric(value) {
        return nonNegativeInt(value) >= 0
    }

    function detailText(details, key) {
        if (!details)
            return "-"
        var value = details[key]
        return hasTextValue(value) ? String(value).trim() : "-"
    }

    function effectiveBasicDetails() {
        var out = {}
        function merge(source) {
            if (!source)
                return
            var keys = Object.keys(source)
            for (var i = 0; i < keys.length; ++i) {
                var key = keys[i]
                var value = source[key]
                if (out[key] === undefined || !hasMeaningfulDetailValue(out[key]))
                    out[key] = value
            }
        }
        merge(root.selectedPrinterDetails || ({}))
        if (root.selectedPrinter) {
            merge(root.selectedPrinter.details || ({}))
            merge(root.selectedPrinter)
        }
        return out
    }

    function progressRatio(printer) {
        var value = nonNegativeInt(printer ? printer.progress : -1)
        if (value < 0)
            return 0
        return Math.max(0, Math.min(value, 100)) / 100
    }

    function progressPercentText(printer) {
        return providerText(root.progressTextProvider, printer ? printer.progress : -1, "-")
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

    function normalizedTotalPrintHoursText(details) {
        if (!details || !hasTextValue(details.printTotalTime))
            return "-"
        var raw = String(details.printTotalTime).trim()
        var compact = raw.toLowerCase().replace(/\s+/g, "")
        var match = compact.match(/^(\d+)(?:h|hour|hours)(\d+)(?:m|min|mins|minute|minutes)$/)
        if (match) {
            var totalHours = Number(match[1]) + Number(match[2]) / 60
            return qsTr("%1 h").arg(formatDecimal(totalHours, 2))
        }
        var numeric = Number(raw)
        if (isFinite(numeric) && numeric >= 0)
            return qsTr("%1 h").arg(formatDecimal(numeric, 2))
        return raw
    }

    function normalizedTotalResinText(details) {
        if (!details || !hasTextValue(details.materialUsed))
            return "-"
        var numeric = Number(details.materialUsed)
        if (isFinite(numeric) && numeric >= 0) {
            return qsTr("%1 L").arg(formatDecimal(numeric / 1000, 2))
        }
        var raw = String(details.materialUsed).trim()
        var match = raw.match(/^([0-9]+(?:[.,][0-9]+)?)\s*(ml|l)$/i)
        if (match) {
            var value = Number(String(match[1]).replace(",", "."))
            if (isFinite(value) && value >= 0) {
                if (String(match[2]).toLowerCase() === "ml")
                    value = value / 1000
                return qsTr("%1 L").arg(formatDecimal(value, 2))
            }
        }
        return raw
    }

    function formatDecimal(value, decimals) {
        var rounded = Number(value).toFixed(decimals)
        return rounded.replace(/0+$/, "").replace(/\.$/, "")
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
        var layers = nonNegativeInt(details.releaseFilmLayers)
        var times = nonNegativeInt(details.releaseFilmTimes)
        var statusCode = Number(details.releaseFilmStatusCode)
        if (layers >= 0 || times >= 0 || isFinite(statusCode)) {
            var parts = []
            if (times >= 0)
                parts.push(qsTr("%1 prints").arg(times))
            if (layers >= 0)
                parts.push(qsTr("%1 layers").arg(layers))
            if (isFinite(statusCode) && Math.round(statusCode) !== 0)
                parts.push(qsTr("Film a changer"))
            else if (isFinite(statusCode))
                parts.push(qsTr("OK"))
            return parts.length > 0 ? parts.join(" | ") : "-"
        }
        var candidates = [
            details.releaseFilmStatus,
            details.release_film_status,
            details.fepStatus,
            details.fep_status,
            details.releaseFilm,
            details.release_film
        ]
        for (var i = 0; i < candidates.length; ++i) {
            if (hasTextValue(candidates[i]))
                return String(candidates[i]).trim()
        }
        return "-"
    }

    function lastPrintedFileText() {
        if (!root.printerHistoryModel || root.printerHistoryModel.count === undefined)
            return "-"
        var fallback = ""
        for (var i = 0; i < root.printerHistoryModel.count; ++i) {
            var entry = root.printerHistoryModel.get(i)
            var name = String(entry.gcodeName || entry.currentFile || "").trim()
            if (name.length <= 0)
                continue
            if (fallback.length <= 0)
                fallback = name
            if (Number(entry.printStatus) === 2)
                return name
        }
        return fallback.length > 0 ? fallback : "-"
    }

    function recentJobStatusInfo(printStatus) {
        var taskId = arguments.length > 1 ? String(arguments[1] || "").trim() : ""
        var printerId = arguments.length > 2 ? String(arguments[2] || "").trim() : ""
        if (isActiveRecentJob(taskId, printerId))
            return { label: qsTr("In progress"), bg: Theme.statusInfoBg, fg: Theme.stateRunning, border: Theme.stateRunning }

        var code = Number(printStatus)
        if (isFinite(code))
            code = Math.round(code)
        else
            code = -1

        if (code === 1)
            return { label: qsTr("In progress"), bg: Theme.statusInfoBg, fg: Theme.stateRunning, border: Theme.stateRunning }
        if (code === 2)
            return { label: qsTr("Finished"), bg: Theme.statusSuccessBg, fg: Theme.stateSuccess, border: Theme.stateSuccess }
        if (code === 3)
            return { label: qsTr("Failed"), bg: Theme.statusErrorBg, fg: Theme.stateError, border: Theme.stateError }
        if (code === 4)
            return { label: qsTr("Canceled"), bg: Theme.statusWarningBg, fg: Theme.stateWarning, border: Theme.stateWarning }
        return { label: qsTr("Unknown"), bg: Theme.bgCardSubtle, fg: Theme.fgPrimary, border: Theme.borderDefault }
    }

    function isActiveRecentJob(taskId, printerId) {
        var activeTaskId = String(selectedMqttField("mqttActiveTaskId", "") || "").trim()
        if (taskId.length > 0 && activeTaskId.length > 0 && taskId === activeTaskId)
            return true

        var liveTaskId = String(root.selectedLiveJobData && root.selectedLiveJobData.taskId !== undefined
                                ? root.selectedLiveJobData.taskId : "").trim()
        if (taskId.length > 0 && liveTaskId.length > 0 && taskId === liveTaskId)
            return true

        var livePrinterId = String(root.selectedLiveJobData && root.selectedLiveJobData.printerId !== undefined
                                   ? root.selectedLiveJobData.printerId : "").trim()
        var liveStatus = Number(root.selectedLiveJobData && root.selectedLiveJobData.printStatus !== undefined
                                ? root.selectedLiveJobData.printStatus : -1)
        return printerId.length > 0 && livePrinterId.length > 0 && printerId === livePrinterId
                && isFinite(liveStatus) && Math.round(liveStatus) === 1
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
        var mqttPrintState = pickText(root.selectedPrinter ? root.selectedPrinter.mqttPrintState : "", "")
        var mqttJobStage = pickText(root.selectedPrinter ? root.selectedPrinter.mqttJobStage : "", "")
        var selectedDetails = root.selectedPrinter && root.selectedPrinter.details ? root.selectedPrinter.details : null
        if (mqttPrintState.length <= 0 && selectedDetails)
            mqttPrintState = pickText(selectedDetails.mqttPrintState, "")
        if (mqttJobStage.length <= 0 && selectedDetails)
            mqttJobStage = pickText(selectedDetails.mqttJobStage, "")
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
        lines.push("  mqtt_print_state: " + (mqttPrintState.length > 0 ? mqttPrintState : "-"))
        lines.push("  mqtt_job_stage: " + (mqttJobStage.length > 0 ? mqttJobStage : "-"))
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
                Layout.fillHeight: false
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    Layout.preferredHeight: deviceDetailsContent.implicitHeight + 20
                    radius: Theme.radiusControl
                    color: Theme.bgWindow
                    border.width: Theme.borderWidth
                    border.color: Theme.borderSubtle

                    ColumnLayout {
                        id: deviceDetailsContent
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Text {
                                Layout.fillWidth: true
                                text: String(root.selectedPrinter ? root.selectedPrinter.name : "-")
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.fontTitlePx
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            StatusChip {
                                objectName: "printerHeaderStatusChip"
                                status: root.printerDisplayStatus()
                            }
                        }

                        RowLayout {
                            visible: root.hasActiveWorkflowStatus()
                            Layout.fillWidth: true
                            Layout.preferredHeight: visible ? implicitHeight : 0
                            Layout.maximumHeight: visible ? implicitHeight : 0
                            spacing: 8

                            Rectangle {
                                radius: Theme.radiusControl
                                color: Theme.cardAlt
                                border.width: Theme.borderWidth
                                border.color: Theme.borderSubtle
                                Layout.fillWidth: true
                                implicitHeight: workflowStatusContent.implicitHeight + 16

                                ColumnLayout {
                                    id: workflowStatusContent
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 6

                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: 3
                                        columnSpacing: 10
                                        rowSpacing: 4

                                        Repeater {
                                            model: root.workflowStatusRows()

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 6

                                                Rectangle {
                                                    Layout.preferredWidth: 7
                                                    Layout.preferredHeight: 7
                                                    radius: 3
                                                    color: modelData.status === qsTr("done")
                                                           ? Theme.stateSuccess
                                                           : (modelData.status === qsTr("stop")
                                                              ? Theme.stateError
                                                           : (modelData.status === qsTr("warning")
                                                              ? Theme.stateWarning
                                                           : (modelData.status === qsTr("pending")
                                                              ? Theme.fgSecondary
                                                              : Theme.stateRunning)))
                                                }

                                                Text {
                                                    text: modelData.label
                                                    color: Theme.fgSecondary
                                                    font.pixelSize: Theme.fontCaptionPx
                                                    font.bold: true
                                                }

                                                Text {
                                                    text: String(modelData.status || "-")
                                                    color: modelData.status === qsTr("done")
                                                           ? Theme.stateSuccess
                                                           : (modelData.status === qsTr("stop")
                                                              ? Theme.stateError
                                                           : (modelData.status === qsTr("warning")
                                                              ? Theme.stateWarning
                                                           : (modelData.status === qsTr("pending")
                                                              ? Theme.fgSecondary
                                                              : Theme.stateRunning)))
                                                    font.pixelSize: Theme.fontCaptionPx
                                                    font.bold: true
                                                }

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: String(modelData.value || "-")
                                                    color: Theme.fgPrimary
                                                    font.pixelSize: Theme.fontCaptionPx
                                                    elide: Text.ElideRight
                                                }

                                                AppButton {
                                                    visible: modelData.status === qsTr("stop")
                                                             || modelData.status === qsTr("warning")
                                                    text: qsTr("Details")
                                                    variant: "danger"
                                                    compact: true
                                                    onClicked: root.openCheckErrorDialogIfNeeded()
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            visible: root.printerInfoMode() === "printing"
                            Layout.fillWidth: true
                            Layout.preferredHeight: visible ? 176 : 0
                            Layout.maximumHeight: visible ? 176 : 0
                            radius: Theme.radiusControl
                            color: Theme.bgSurface
                            border.width: Theme.borderWidth
                            border.color: Theme.borderSubtle
                            implicitHeight: 176

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 160
                                    Layout.preferredHeight: 160
                                    radius: Theme.radiusControl
                                    color: Theme.bgWindow
                                    border.width: Theme.borderWidth
                                    border.color: Theme.borderSubtle

                                    Image {
                                        id: currentFilePreviewImage
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        source: root.currentPreviewSource
                                        fillMode: Image.PreserveAspectFit
                                        asynchronous: true
                                        cache: true
                                        smooth: true
                                        visible: root.currentPreviewSource.length > 0 && status === Image.Ready
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        visible: root.currentPreviewSource.length <= 0 || currentFilePreviewImage.status === Image.Error
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

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 10

                                        Text {
                                            text: qsTr("Impression")
                                            color: Theme.stateRunning
                                            font.pixelSize: Theme.fontCaptionPx
                                            font.bold: true
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: root.layersProgressText(root.selectedPrinter)
                                            color: Theme.fgSecondary
                                            font.pixelSize: Theme.fontCaptionPx
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            text: root.progressPercentText(root.selectedPrinter)
                                            color: Theme.fgPrimary
                                            font.pixelSize: Theme.fontBodyPx
                                            font.bold: true
                                        }
                                    }

                                    ProgressBar {
                                        objectName: "printerCurrentPrintProgressBar"
                                        Layout.fillWidth: true
                                        from: 0
                                        to: 1
                                        value: root.progressRatio(root.selectedPrinter)
                                    }

                                    Text {
                                        visible: root.showDebugLabels
                                        Layout.fillWidth: true
                                        text: qsTr("Image source: %1").arg(
                                                  root.currentPreviewSource.length > 0
                                                  ? root.currentPreviewSource
                                                  : qsTr("(empty)"))
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                        wrapMode: Text.WrapAnywhere
                                    }

                                    Text {
                                        visible: root.showDebugLabels
                                        Layout.fillWidth: true
                                        text: qsTr("Image status: %1 (%2)").arg(
                                                  currentFilePreviewImage.status).arg(
                                                  root.imageStatusText(currentFilePreviewImage.status))
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        visible: root.showDebugLabels
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
                            Layout.preferredHeight: visible ? implicitHeight : 0
                            Layout.maximumHeight: visible ? implicitHeight : 0
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8

                            Rectangle {
                                visible: false
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
                                visible: false
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
                            Layout.preferredHeight: visible ? implicitHeight : 0
                            Layout.maximumHeight: visible ? implicitHeight : 0
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8

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
                                        text: qsTr("Firmware")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        objectName: "printerFirmwareValue"
                                        text: root.detailText(root.effectiveBasicDetails(), "firmwareVersion")
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
                                        text: qsTr("Print Count")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        objectName: "printerPrintCountValue"
                                        text: root.printCountText(root.effectiveBasicDetails())
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
                                        text: qsTr("Film State")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        objectName: "printerFilmStateValue"
                                        text: root.releaseFilmText(root.effectiveBasicDetails())
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
                                        text: qsTr("Total Print Time")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        objectName: "printerTotalPrintTimeValue"
                                        text: root.normalizedTotalPrintHoursText(root.effectiveBasicDetails())
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
                                        text: qsTr("Total Resin")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        objectName: "printerTotalResinValue"
                                        text: root.normalizedTotalResinText(root.effectiveBasicDetails())
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
                                        text: qsTr("Last Printed File")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                    }

                                    Text {
                                        objectName: "printerLastPrintedFileValue"
                                        text: root.lastPrintedFileText()
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

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            AppButton {
                                Layout.minimumWidth: implicitWidth
                                text: qsTr("From Local File")
                                variant: "primary"
                                enabled: root.selectedPrinter !== null && root.localFilePrintEnabled
                                ToolTip.visible: hovered && !enabled
                                ToolTip.delay: 350
                                ToolTip.text: qsTr("Local printer file printing is not enabled in this build.")
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
                            id: recentJobsList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 6
                            rightMargin: 14
                            cacheBuffer: 360
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

                                Item {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 18
                                    anchors.topMargin: 8
                                    anchors.bottomMargin: 8

                                    Rectangle {
                                        id: recentJobStatusBadge
                                        objectName: "recentJobStatusBadge"
                                        readonly property var statusInfo: root.recentJobStatusInfo(model.printStatus, model.taskId, model.printerId)
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        width: Math.max(74, statusText.implicitWidth + 12)
                                        height: 22
                                        radius: Theme.radiusControl
                                        color: statusInfo.bg
                                        border.width: Theme.borderWidth
                                        border.color: statusInfo.border

                                        Text {
                                            id: statusText
                                            anchors.centerIn: parent
                                            text: parent.statusInfo.label
                                            color: parent.statusInfo.fg
                                            font.pixelSize: Theme.fontCaptionPx
                                            font.bold: true
                                        }
                                    }

                                    Text {
                                        id: recentJobNameText
                                        anchors.left: parent.left
                                        anchors.right: recentJobStatusBadge.left
                                        anchors.rightMargin: 8
                                        anchors.top: parent.top
                                        height: 22
                                        text: String(model.gcodeName || "-")
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontBodyPx
                                        font.bold: true
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    Text {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: recentJobNameText.bottom
                                        anchors.topMargin: 4
                                        height: 18
                                        text: qsTr("Start %1 | End %2 | Duration %3")
                                            .arg(root.providerText(root.unixTimeTextProvider, model.createTime, "-"))
                                            .arg(root.providerText(root.unixTimeTextProvider, model.endTime, "-"))
                                            .arg(root.historyDurationText(model.createTime, model.endTime))
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    Text {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        height: 18
                                        text: qsTr("Task %1")
                                            .arg(String(model.taskId || "-"))
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
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

            AppDialogFrame {
                id: checkErrorDialog
                title: qsTr("Printer message")
                subtitle: qsTr("Resolve the printer message before continuing.")
                dialogSize: "small"
                minimumWidth: 520
                maximumWidth: 720
                property string detailsText: ""

                Text {
                    Layout.fillWidth: true
                    text: checkErrorDialog.detailsText
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                    wrapMode: Text.WordWrap
                }

                footerTrailingData: [
                    AppButton {
                        text: qsTr("OK")
                        variant: "primary"
                        onClicked: checkErrorDialog.close()
                    }
                ]
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
