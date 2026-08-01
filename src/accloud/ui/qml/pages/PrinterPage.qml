import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Accloud.Models 1.0
import "../components/Theme.js" as Theme
import "../components"

Item {
    id: root
    objectName: "printerPage"
    Layout.fillWidth: true
    Layout.fillHeight: true
    property bool embeddedInTabsContainer: false
    property bool deferStartupInitialization: false
    property bool pageActive: true
    property bool directDeleteLocalOnFailurePreference: false
    signal statusBroadcast(string message, string severity, string operationId)
    signal remotePrintAccepted(string printerId, string taskId)

    property bool loading: false
    property string statusMsg: qsTr("Ready.")
    property string statusSev: "info" // info | success | warn | error
    property string selectedPrinterId: ""
    property bool debugUi: false
    property bool showDebugLabels: false
    property string printersEndpointPath: "/p/p/workbench/api/work/printer/getPrinters + /p/p/workbench/api/work/project/getProjects?printer_id=<id>&print_status=1"
    property string printersEndpointRawJson: ""

    property string remotePrinterId: ""
    property string selectedCloudFileId: ""
    property string selectedPrinterLocalFileName: ""
    property string localFilesTargetPrinterId: ""
    property string pendingLocalFilesPrinterId: ""
    property string pendingCloudFilesPrinterId: ""
    property bool localFilesLoading: false
    property bool cloudFilesLoading: false
    property int localFilesPrepareOrderId: 1231
    property int localFilesListOrderId: 103
    property int localFileDeleteOrderId: 104
    property int localFileStartPrintOrderId: 1
    property int resinFeedOrderId: 1224
    readonly property bool localFilePrintEnabled: true
    property bool optionDeleteAfterPrint: false
    property string remotePrintMode: "cloud"
    property string directPrintLocalPath: ""
    property string directPrintLocalName: ""
    property bool directPrintCompletePreview: false
    property bool directPrintDeleteLocalOnFailureSnapshot: false
    property string pendingDirectUploadContext: ""
    property bool optionLiftCompensation: false
    property bool optionAutoResinCheck: true
    property bool remotePrintAllowed: true
    property string remotePrintBlockReason: ""
    property bool remotePrintPreparing: false
    property bool remotePrintSubmitting: false
    property string remotePrintPrepareMessage: ""
    property var remotePrintCompatibilityResult: null
    property bool remotePrintPrinterBootstrapPending: false
    property string pendingRemotePrintBootstrapFileId: ""
    property string pendingRemotePrintBootstrapFileName: ""
    property string pendingPrintPrinterId: ""
    property string pendingPrintFileId: ""
    property var pendingPrintFileData: null
    property var selectedPrinterDetails: ({})
    property string selectedPrinterDetailsRawJson: ""
    property string selectedPrinterProjectsRawJson: ""
    property var liveProjectData: ({})
    property var selectedPrinterLiveSnapshot: null
    property bool loadingPrinterDetails: false
    property bool loadingPrinterHistory: false
    property bool reasonCatalogLoaded: false
    property bool reasonCatalogLoading: false
    property var reasonCatalogByCode: ({})
    property bool startupInitialized: false
    property bool startupJobsRefreshed: false
    property var printerHadActiveJobById: ({})
    property var pendingRemotePrintByPrinterId: ({})
    property bool pendingPrintDeleteAfterPrint: false
    property string lastJobsRefreshReason: ""
    property string mqttDetailsTitle: ""
    property string mqttDetailsText: ""
    property bool resinFeedActive: false
    property int resinFeedType: 0
    property bool resinFeedStopSubmitting: false
    property string resinFeedPrinterId: ""
    property bool resinFeedObservedRunningState: false
    property int autoRefreshIntervalMs: 30000
    property int autoRefreshPrintingIntervalMs: 5000
    property int mqttRealtimeDebounceMs: 700
    property bool mqttRealtimeRefreshPending: false
    function emitStatusToShell() {
        var msg = String(statusMsg || "").trim()
        if (msg.length === 0)
            return
        root.statusBroadcast(msg, String(statusSev || "info"), "op_printer_refresh")
    }

    onStatusMsgChanged: root.emitStatusToShell()
    onStatusSevChanged: root.emitStatusToShell()
    onSelectedPrinterIdChanged: {
        root.updatePrintersAutoRefreshInterval()
        root.refreshSelectedPrinterLiveSnapshot()
    }
    onPageActiveChanged: {
        root.updatePrintersAutoRefreshInterval()
        if (root.pageActive && root.mqttRealtimeRefreshPending) {
            root.mqttRealtimeRefreshPending = false
            if (mqttRealtimeDebounceTimer.running)
                mqttRealtimeDebounceTimer.restart()
            else
                mqttRealtimeDebounceTimer.start()
        }
    }

    PrintersModel {
        id: printersModel
    }

    PrintersModel {
        id: remoteCompatiblePrintersModel
        objectName: "remoteCompatiblePrintersModel"
    }

    PrinterFilesModel {
        id: printCloudFilesModel
        objectName: "printCloudFilesModel"
    }

    PrinterFilesModel {
        id: printerLocalFilesModel
        objectName: "printerLocalFilesModel"
    }

    RecentJobsModel {
        id: printerHistoryModel
        objectName: "printerHistoryModel"
    }

    component DebugTag: Rectangle {
        property string label: ""
        visible: root.showDebugLabels
        z: 200
        radius: 4
        color: Qt.rgba(1.0, 0.95, 0.82, 0.95)
        border.width: 1
        border.color: Theme.warning
        implicitWidth: debugTagText.implicitWidth + 10
        implicitHeight: debugTagText.implicitHeight + 6

        Text {
            id: debugTagText
            anchors.centerIn: parent
            text: parent.label
            color: Theme.warning
            font.pixelSize: 10
            font.bold: true
        }
    }

    function hasCloudBridge() {
        return (typeof cloudBridge !== "undefined")
                && cloudBridge !== null
                && typeof cloudBridge.fetchPrinters === "function"
                && typeof cloudBridge.fetchFiles === "function"
                && typeof cloudBridge.sendPrintOrder === "function"
    }

    function hasQObjectCloudBridge() {
        return hasCloudBridge()
                && cloudBridge.objectName !== undefined
    }

    function hasPrintWorkflowBridge() {
        return (typeof printWorkflowBridge !== "undefined")
                && printWorkflowBridge !== null
                && typeof printWorkflowBridge.trackDirectPrint === "function"
                && typeof printWorkflowBridge.reconcileDirectPrints === "function"
    }

    function hasRemotePrintWorkflowBridge() {
        return (typeof printWorkflowBridge !== "undefined")
                && printWorkflowBridge !== null
                && typeof printWorkflowBridge.beginRemotePrintPreparation === "function"
                && typeof printWorkflowBridge.evaluateRemotePrintGuard === "function"
    }

    function hasPrinterOrderEndpoint() {
        return hasCloudBridge()
                && (typeof cloudBridge.sendPrinterOrderAsync === "function"
                    || typeof cloudBridge.sendPrinterOrder === "function")
    }

    function hasLocalCompatibilityEvaluator() {
        return hasCloudBridge() && typeof cloudBridge.evaluateLocalPrinterFileCompatibility === "function"
    }

    function hasConnectedMqttBridge() {
        return (typeof mqttBridge !== "undefined")
                && mqttBridge !== null
                && mqttBridge.connected === true
    }

    function translateLocalizedText(rawText) {
        var text = String(rawText || "")
        if (text.length === 0)
            return text

        var replacements = {
            "请求被接受": qsTr("Request accepted"),
            "操作成功": qsTr("Operation successful"),
            "连接成功": qsTr("Connection successful"),
            "用户不存在": qsTr("User does not exist"),
            "设备离线": qsTr("Printer offline"),
            "打印中": qsTr("Printing in progress"),
            "料盒清理": qsTr("Vat cleaning"),
            "曝光检测": qsTr("Exposure test"),
            "移动Z轴": qsTr("Move Z axis"),
            "文件管理": qsTr("File management"),
            "离型膜状态": qsTr("Release film status"),
            "智能料盒": qsTr("Smart resin vat"),
            "打印功能设置": qsTr("Print feature settings"),
            "失败": qsTr("Failed"),
            "成功": qsTr("Success"),
            "错误": qsTr("Error"),
            "超时": qsTr("Timeout")
        }

        for (var key in replacements) {
            if (Object.prototype.hasOwnProperty.call(replacements, key))
                text = text.split(key).join(replacements[key])
        }

        return text
    }

    function backendStatusDetail(rawMessage, fallbackMessage) {
        var detail = translateLocalizedText(String(rawMessage || "").trim())
        return detail.length > 0 ? detail : String(fallbackMessage || qsTr("unknown error"))
    }

    function fileType(fileName) {
        var name = String(fileName || "")
        var dot = name.lastIndexOf(".")
        if (dot < 0 || dot + 1 >= name.length)
            return "other"
        return name.slice(dot + 1).toLowerCase()
    }


    function uploadIsReady(uploadStatus, gcodeId) {
        var normalizedGcodeId = String(gcodeId || "").trim()
        return Number(uploadStatus) === 1
                || (normalizedGcodeId.length > 0 && normalizedGcodeId !== "0")
    }

    function bytesText(sizeBytes) {
        var value = Number(sizeBytes)
        if (!isFinite(value) || value < 0)
            return "-"
        if (value >= 1024 * 1024 * 1024)
            return (value / (1024 * 1024 * 1024)).toFixed(1) + " GB"
        if (value >= 1024 * 1024)
            return (value / (1024 * 1024)).toFixed(1) + " MB"
        if (value >= 1024)
            return (value / 1024).toFixed(1) + " KB"
        return Math.round(value) + " B"
    }

    function statusChipText(state) {
        var raw = String(state || "").toUpperCase()
        if (raw === "OFFLINE") return qsTr("Offline")
        if (raw === "PRINTING") return qsTr("Printing")
        if (raw === "ERROR") return qsTr("Error")
        return qsTr("Ready")
    }

    function progressText(progress) {
        var value = Number(progress)
        if (!isFinite(value) || value < 0)
            return "-"
        return qsTr("%1 %").arg(Math.max(0, Math.min(100, Math.round(value))))
    }

    function timeText(seconds) {
        var sec = Number(seconds)
        if (!isFinite(sec) || sec < 0)
            return "-"
        var totalMin = Math.floor(sec / 60)
        var days = Math.floor(totalMin / (24 * 60))
        var hours = Math.floor((totalMin % (24 * 60)) / 60)
        var minutes = totalMin % 60

        if (days > 0)
            return qsTr("%1 j %2 h %3 min").arg(days).arg(hours).arg(minutes)
        if (hours > 0)
            return qsTr("%1 h %2 min").arg(hours).arg(minutes)
        return qsTr("%1 min").arg(minutes)
    }

    function unixTimeText(epochSeconds) {
        var value = Number(epochSeconds)
        if (!isFinite(value) || value <= 0)
            return "-"
        var d = new Date(value * 1000)
        return Qt.formatDateTime(d, "yyyy-MM-dd hh:mm")
    }

    function printStatusText(printStatus) {
        var s = Number(printStatus)
        if (!isFinite(s))
            return "-"
        if (s === 1) return qsTr("Printing")
        if (s === 2) return qsTr("Finished")
        if (s === 3) return qsTr("Failed")
        if (s === 4) return qsTr("Canceled")
        return String(s)
    }


    function hasPrinterJob(printer) {
        if (printer === null || printer === undefined)
            return false

        var state = String(printer.state || "").toUpperCase()
        if (state === "PRINTING")
            return true

        var directStatus = Number(printer.printStatus)
        if (isFinite(directStatus) && Math.round(directStatus) === 1)
            return true

        var projects = printer.projects
        if (projects !== undefined && projects !== null && projects.length !== undefined) {
            for (var i = 0; i < projects.length; ++i) {
                if (Number(projects[i].printStatus) === 1)
                    return true
            }
        }
        return false
    }


    function printerTabTitle(printer) {
        var name = String(printer && printer.name !== undefined ? printer.name : "-")
        var status = statusChipText(printer ? printer.state : "READY")
        return name + " | " + status
    }


    function canStartFromPrinterState(printer) {
        if (!printer)
            return { "ok": false, "reason": qsTr("Select a printer first.") }

        var state = String(printer.state || "").toUpperCase()
        if (state === "OFFLINE")
            return { "ok": false, "reason": qsTr("Printer offline.") }
        if (state === "PRINTING")
            return { "ok": false, "reason": qsTr("Printer is currently printing.") }
        if (state === "ERROR")
            return { "ok": false, "reason": qsTr("Printer reported an error.") }
        return { "ok": true, "reason": "" }
    }

    function remotePrintReasonText(reasonKey, rawReason) {
        var key = String(reasonKey || "")
        if (key === "select_printer") return qsTr("Select a printer first.")
        if (key === "select_file") return qsTr("Select a cloud file first.")
        if (key === "printer_offline") return qsTr("Printer offline.")
        if (key === "printer_printing") return qsTr("Printer is currently printing.")
        if (key === "printer_error") return qsTr("Printer reported an error.")
        if (key === "unsupported_format") return qsTr("Unsupported file format.")
        if (key === "machine_type_mismatch") return qsTr("Slice file does not match machine type.")
        if (key === "missing_metadata") return qsTr("Missing local compatibility metadata.")
        if (key === "model_mismatch") return qsTr("Slice file does not match selected printer model.")
        if (key === "no_printer") return qsTr("No printer available for remote print.")
        if (key === "no_compatible_printer") {
            var detail = translateLocalizedText(String(rawReason || "").trim())
            return detail.length > 0 ? detail : qsTr("No compatible printer available for this file.")
        }
        return translateLocalizedText(String(rawReason || ""))
    }

    function evaluateRemotePrintGuard() {
        var printer = printerDataById(remotePrinterId)
        var fileData = selectedCloudFileData()
        if (hasRemotePrintWorkflowBridge()) {
            return printWorkflowBridge.evaluateRemotePrintGuard(remotePrintMode,
                                                                 printer || ({}),
                                                                 fileData || ({}))
        }
        var stateCheck = canStartFromPrinterState(printer)
        if (stateCheck.ok !== true)
            return stateCheck
        if (remotePrintMode !== "direct" && !fileData)
            return { "ok": false, "reason": qsTr("Select a cloud file first."), "reasonKey": "select_file" }
        return { "ok": true, "reason": "", "reasonKey": "" }
    }

    function refreshRemotePrintGuard() {
        var result = evaluateRemotePrintGuard()
        remotePrintAllowed = (result.ok === true)
        remotePrintBlockReason = remotePrintReasonText(result.reasonKey, result.reason)
    }

    function prettyJson(rawPayload) {
        var text = String(rawPayload || "").trim()
        if (text.length === 0)
            return "{\n  \"message\": \"No endpoint response captured.\"\n}"
        try {
            return JSON.stringify(JSON.parse(text), null, 2)
        } catch (error) {
            return text
        }
    }

    function selectedPrinterData() {
        if (selectedPrinterId.length === 0)
            return null
        for (var i = 0; i < printersModel.count; ++i) {
            var p = printersModel.get(i)
            if (String(p.id) === selectedPrinterId)
                return p
        }
        return null
    }

    function cloudFileIdValue(fileEntry) {
        if (!fileEntry)
            return ""
        var idValue = String(fileEntry.fileId !== undefined ? fileEntry.fileId : "").trim()
        if (idValue.length > 0)
            return idValue
        idValue = String(fileEntry.id !== undefined ? fileEntry.id : "").trim()
        if (idValue.length > 0)
            return idValue
        return String(fileEntry.gcodeId !== undefined ? fileEntry.gcodeId : "").trim()
    }

    function cloudFilePrintTimeText(fileEntry) {
        if (!fileEntry)
            return "-"
        var printTimeValue = String(fileEntry.printTime !== undefined ? fileEntry.printTime : "").trim()
        if (printTimeValue.length > 0 && printTimeValue !== "-")
            return printTimeValue
        printTimeValue = String(fileEntry.print_time !== undefined ? fileEntry.print_time : "").trim()
        if (printTimeValue.length > 0 && printTimeValue !== "-")
            return printTimeValue
        var estimateSeconds = Number(fileEntry.printTimeSec !== undefined
                                     ? fileEntry.printTimeSec
                                     : (fileEntry.estimateSec !== undefined
                                        ? fileEntry.estimateSec
                                        : fileEntry.estimateSeconds))
        if (isFinite(estimateSeconds) && estimateSeconds > 0)
            return timeText(estimateSeconds)
        return "-"
    }

    function cloudFileResinUsageText(fileEntry) {
        if (!fileEntry)
            return "-"
        var resinValue = String(fileEntry.resinUsage !== undefined ? fileEntry.resinUsage : "").trim()
        if (resinValue.length > 0 && resinValue !== "-")
            return resinValue
        var fallbackKeys = ["resin_volume", "resinVolume", "weight", "supplies_usage"]
        for (var i = 0; i < fallbackKeys.length; ++i) {
            var key = fallbackKeys[i]
            var raw = String(fileEntry[key] !== undefined ? fileEntry[key] : "").trim()
            if (raw.length <= 0 || raw === "-")
                continue
            if (/^\d+(\.\d+)?$/.test(raw))
                return raw + " ml"
            return raw
        }
        return "-"
    }

    function cloudFileDataById(fileId) {
        var normalizedFileId = String(fileId || "").trim()
        if (normalizedFileId.length === 0)
            return null
        for (var i = 0; i < printCloudFilesModel.count; ++i) {
            var fileEntry = printCloudFilesModel.get(i)
            if (cloudFileIdValue(fileEntry) === normalizedFileId)
                return fileEntry
        }
        return null
    }


    function loadCloudFileDetailsById(fileId) {
        var normalizedFileId = String(fileId || "").trim()
        if (normalizedFileId.length === 0)
            return null
        return cloudFileDataById(normalizedFileId)
    }

    function ensureSelectedCloudFile() {
        if (selectedCloudFileId.length > 0 && selectedCloudFileData())
            return true
        if (printCloudFilesModel.count <= 0)
            return false

        var fallbackFileId = cloudFileIdValue(printCloudFilesModel.get(0))
        if (fallbackFileId.length <= 0)
            return false
        selectedCloudFileId = fallbackFileId
        return true
    }

    function selectedCloudFileData() {
        if (selectedCloudFileId.length === 0)
            return null
        return cloudFileDataById(selectedCloudFileId)
    }

    function printerDataById(printerId) {
        var targetId = String(printerId || "")
        if (targetId.length === 0)
            return null
        for (var i = 0; i < printersModel.count; ++i) {
            var p = printersModel.get(i)
            if (String(p.id) === targetId)
                return p
        }
        return null
    }


    function activeProjectFromHistory() {
        for (var i = 0; i < printerHistoryModel.count; ++i) {
            var item = printerHistoryModel.get(i)
            if (Number(item.printStatus) === 1)
                return item
        }
        return printerHistoryModel.count > 0 ? printerHistoryModel.get(0) : null
    }

    function firstActiveProject(projectsList) {
        var list = projectsList !== undefined && projectsList !== null ? projectsList : []
        if (list.length === undefined || list.length <= 0)
            return null
        for (var i = 0; i < list.length; ++i) {
            if (Number(list[i].printStatus) === 1)
                return list[i]
        }
        return list[0]
    }

    function setLiveProjectFromList(projectsList) {
        var list = projectsList !== undefined && projectsList !== null ? projectsList : []
        if (hasPrintWorkflowBridge())
            printWorkflowBridge.reconcileDirectPrints(list)
        var active = firstActiveProject(list)
        if (active) {
            liveProjectData = active
        } else {
            var pending = pendingRemotePrintForPrinter(selectedPrinterId)
            liveProjectData = pending ? pending : ({})
        }
        refreshSelectedPrinterLiveSnapshot()
    }

    function hasLiveProjectData() {
        return liveProjectData !== null
                && liveProjectData !== undefined
                && Object.keys(liveProjectData).length > 0
    }

    function applyRecentJobsCard(jobsList) {
        var list = jobsList !== undefined && jobsList !== null ? jobsList : []
        return printerHistoryModel.replaceOrPatchJobs(list)
    }

    function replaceRecentJobsCard(projectsList) {
        var list = projectsList !== undefined && projectsList !== null ? projectsList : []
        applyRecentJobsCard(list)
    }

    function mergeRecentJobsCard(projectsList) {
        var list = projectsList !== undefined && projectsList !== null ? projectsList : []
        printerHistoryModel.mergeOrPatchJobs(list)
    }

    function selectedPrinterLiveData() {
        var selected = selectedPrinterData()
        if (!selected)
            return null

        var merged = {}
        for (var key in selected)
            merged[key] = selected[key]

        var selectedId = String(selected.id || selectedPrinterId || "")
        var pendingPrint = pendingRemotePrintForPrinter(selectedId)
        var selectedState = String(selected.state || "").toUpperCase()
        if (pendingPrint && selectedState !== "PRINTING") {
            merged.state = "PRINTING"
            merged.reason = qsTr("Print order accepted; waiting for printer telemetry")
        }

        function durationFromValue(raw) {
            var numeric = Number(raw)
            if (isFinite(numeric) && numeric >= 0)
                return Math.round(numeric)

            var text = String(raw === undefined || raw === null ? "" : raw).trim()
            if (text.length <= 0)
                return -1

            if (/^\d+(:\d+){1,2}$/.test(text)) {
                var parts = text.split(":")
                var total = 0
                for (var i = 0; i < parts.length; ++i) {
                    var p = Number(parts[i])
                    if (!isFinite(p) || p < 0)
                        return -1
                    total = total * 60 + p
                }
                return Math.round(total)
            }

            return -1
        }

        function durationFromKeys(source, secondKeys, minuteKeys) {
            if (source === null || source === undefined)
                return -1

            for (var i = 0; i < secondKeys.length; ++i) {
                var sec = durationFromValue(source[secondKeys[i]])
                if (sec >= 0)
                    return sec
            }
            for (var j = 0; j < minuteKeys.length; ++j) {
                var min = durationFromValue(source[minuteKeys[j]])
                if (min >= 0)
                    return min * 60
            }
            return -1
        }

        function elapsedSeconds(source) {
            return durationFromKeys(
                        source,
                        ["elapsedSec", "elapsed", "elapsedSeconds"],
                        ["print_time", "printTime", "elapsedMin", "elapsedMinutes", "printTimeMin"])
        }

        function remainingSeconds(source) {
            return durationFromKeys(
                        source,
                        ["remainingSec", "remaining", "remainingSeconds"],
                        ["remain_time", "remainTime", "remainingMin", "remainingMinutes", "timeLeftMin"])
        }

        function mergeTextField(fieldName, incomingValue) {
            var current = String(merged[fieldName] || "").trim()
            if (current.length > 0)
                return
            var incoming = String(incomingValue || "").trim()
            if (incoming.length > 0)
                merged[fieldName] = incoming
        }

        function mergeNumberField(fieldName, incomingValue) {
            var current = Number(merged[fieldName])
            if (isFinite(current) && current >= 0)
                return
            var incoming = Number(incomingValue)
            if (isFinite(incoming) && incoming >= 0)
                merged[fieldName] = incoming
        }

        function mergeDurationField(fieldName, source, resolver) {
            var resolved = resolver(source)
            if (resolved >= 0)
                merged[fieldName] = resolved
        }

        mergeDurationField("elapsedSec", selected, elapsedSeconds)
        mergeDurationField("remainingSec", selected, remainingSeconds)

        var details = selectedPrinterDetails || ({})
        mergeTextField("currentFile", details.currentFile)
        mergeTextField("img", details.img)
        mergeTextField("image", details.image)
        mergeTextField("preview", details.preview)
        mergeTextField("thumbnailUrl", details.thumbnailUrl)
        mergeNumberField("progress", details.progress)
        mergeDurationField("elapsedSec", details, elapsedSeconds)
        mergeDurationField("remainingSec", details, remainingSeconds)
        mergeNumberField("currentLayer", details.currentLayer)
        mergeNumberField("totalLayers", details.totalLayers)

        var activeProject = hasLiveProjectData() ? liveProjectData : activeProjectFromHistory()
        if (activeProject) {
            mergeTextField("currentFile", String(activeProject.currentFile || activeProject.gcodeName || ""))
            mergeTextField("img", activeProject.img)
            mergeTextField("image", activeProject.image)
            mergeTextField("preview", activeProject.preview)
            mergeTextField("thumbnailUrl", activeProject.thumbnailUrl)
            mergeNumberField("progress", activeProject.progress)
            mergeDurationField("elapsedSec", activeProject, elapsedSeconds)
            mergeDurationField("remainingSec", activeProject, remainingSeconds)
            mergeNumberField("currentLayer", activeProject.currentLayer)
            mergeNumberField("totalLayers", activeProject.totalLayers)
        }

        return merged
    }

    function pendingRemotePrintForPrinter(printerId) {
        var key = String(printerId || "").trim()
        if (key.length === 0)
            return null
        var pending = pendingRemotePrintByPrinterId[key]
        return pending !== undefined && pending !== null ? pending : null
    }

    function markRemotePrintAccepted(printerId, fileData, taskId) {
        var key = String(printerId || "").trim()
        if (key.length === 0)
            return
        var fileName = String(fileData && fileData.fileName !== undefined ? fileData.fileName : "").trim()
        var fileId = String(fileData && fileData.fileId !== undefined ? fileData.fileId : selectedCloudFileId).trim()
        var next = {}
        for (var existingKey in pendingRemotePrintByPrinterId)
            next[existingKey] = pendingRemotePrintByPrinterId[existingKey]
        next[key] = {
            "taskId": String(taskId || ""),
            "printerId": key,
            "gcodeName": fileName.length > 0 ? fileName : fileId,
            "currentFile": fileName.length > 0 ? fileName : fileId,
            "fileId": fileId,
            "printStatus": 1,
            "progress": -1,
            "currentLayer": -1,
            "totalLayers": -1,
            "elapsedSec": -1,
            "remainingSec": -1,
            "reason": qsTr("Waiting for printer telemetry"),
            "createTime": Math.floor(Date.now() / 1000),
            "endTime": 0,
            "img": "",
            "deleteAfterPrint": fileData && fileData.deleteAfterPrint === true,
            "directMode": fileData && fileData.directMode === true
        }
        pendingRemotePrintByPrinterId = next
        liveProjectData = next[key]
        if (hasPrintWorkflowBridge()
                && typeof printWorkflowBridge.trackRemotePrintCleanup === "function")
            printWorkflowBridge.trackRemotePrintCleanup(key, fileData || ({}))
        if (fileData && fileData.directMode === true) {
            var operation = {
                "printerId": key,
                "cloudFileId": fileId,
                "cloudGcodeId": String(fileData.gcodeId || ""),
                "cloudFileName": fileName,
                "cloudFileSize": Number(fileData.sizeBytes || 0),
                "printTaskId": String(taskId || ""),
                "printMsgId": String(fileData.printMsgId || ""),
                "printerLocalFilename": "",
                "printerLocalPath": "/",
                "deleteAfterSuccess": fileData.deleteAfterPrint === true,
                "deleteLocalOnFailure": fileData.deleteLocalOnFailure === true,
                "observedActive": false,
                "state": "PRINT_COMMAND_SENT",
                "createdAt": Math.floor(Date.now() / 1000)
            }
            if (hasPrintWorkflowBridge())
                printWorkflowBridge.trackDirectPrint(operation)
        }
        refreshSelectedPrinterLiveSnapshot()
    }

    function clearPendingRemotePrint(printerId) {
        var key = String(printerId || "").trim()
        if (key.length === 0 || pendingRemotePrintByPrinterId[key] === undefined)
            return
        var next = {}
        for (var existingKey in pendingRemotePrintByPrinterId) {
            if (existingKey !== key)
                next[existingKey] = pendingRemotePrintByPrinterId[existingKey]
        }
        pendingRemotePrintByPrinterId = next
    }

    function choosePrinter(printerId) {
        var nextPrinterId = String(printerId || "")
        if (nextPrinterId === selectedPrinterId)
            return
        selectedPrinterId = nextPrinterId
        updatePrintersAutoRefreshInterval()
        loadSelectedPrinterInsights("selection", false, true, false)
    }

    function printerHasActiveJob(printer) {
        if (!printer)
            return false
        if (hasPrinterJob(printer))
            return true
        var state = String(printer.state || "").toUpperCase()
        return state === "PRINTING"
    }

    function hasAnyPrintingPrinter() {
        for (var i = 0; i < printersModel.count; ++i) {
            if (hasPrinterJob(printersModel.get(i)))
                return true
        }
        return false
    }

    function updatePrintersAutoRefreshInterval() {
        var targetInterval = hasAnyPrintingPrinter()
                ? Math.min(autoRefreshIntervalMs, autoRefreshPrintingIntervalMs)
                : autoRefreshIntervalMs
        targetInterval = Math.max(20, Number(targetInterval))
        if (printersAutoRefreshTimer.interval !== targetInterval)
            printersAutoRefreshTimer.interval = targetInterval
        printersAutoRefreshTimer.running = root.pageActive
                && startupInitialized
                && printersModel.count > 0
    }

    function refreshSelectedPrinterLiveSnapshot() {
        selectedPrinterLiveSnapshot = selectedPrinterLiveData()
        updateResinFeedOperationState()
    }

    function updateResinFeedOperationState() {
        if (!resinFeedActive)
            return
        var targetPrinterId = String(resinFeedPrinterId || "").trim()
        if (targetPrinterId.length <= 0)
            return
        var printer = printerDataById(targetPrinterId)
        if (!printer)
            return

        var state = String(printer.state || "").toUpperCase().trim()
        var reason = String(printer.reason || "").toLowerCase().trim()
        var mqttPrintState = String(printer.mqttPrintState || "").toLowerCase().trim()
        var isBusyState = state === "BUSY" || reason === "busy" || mqttPrintState === "busy"
        var isPrintingState = state === "PRINTING"
                || mqttPrintState === "printing"
                || mqttPrintState === "preheating"
                || mqttPrintState === "monitoring"
                || mqttPrintState === "downloading"
        if (isBusyState || isPrintingState) {
            resinFeedObservedRunningState = true
            return
        }

        if (resinFeedObservedRunningState) {
            resinFeedActive = false
            resinFeedType = 0
            resinFeedStopSubmitting = false
            resinFeedPrinterId = ""
            resinFeedObservedRunningState = false
            statusMsg = qsTr("Resin operation finished.")
            statusSev = "success"
        }
    }

    function hasMeaningfulDetailValue(value) {
        if (value === undefined || value === null)
            return false
        var text = String(value).trim()
        return text.length > 0 && text !== "-"
    }

    function mergePrinterDetails(current, incoming) {
        var merged = {}
        function copy(source, overwrite) {
            if (source === undefined || source === null)
                return
            var keys = Object.keys(source)
            for (var i = 0; i < keys.length; ++i) {
                var key = keys[i]
                var value = source[key]
                if (overwrite || merged[key] === undefined)
                    merged[key] = value
            }
        }
        copy(current || ({}), false)
        if (incoming !== undefined && incoming !== null) {
            var incomingKeys = Object.keys(incoming)
            for (var i = 0; i < incomingKeys.length; ++i) {
                var key = incomingKeys[i]
                var value = incoming[key]
                if (hasMeaningfulDetailValue(value))
                    merged[key] = value
            }
        }
        return merged
    }

    function updateSelectedPrinterDetails(incoming) {
        selectedPrinterDetails = mergePrinterDetails(selectedPrinterDetails, incoming || ({}))
    }

    function syncSelectedPrinterDetailsFromModel() {
        var selected = selectedPrinterData()
        if (!selected)
            return
        if (selected.details !== undefined)
            updateSelectedPrinterDetails(selected.details)
        if (selected.detailsRawJson !== undefined)
            selectedPrinterDetailsRawJson = String(selected.detailsRawJson || "")
        if (selected.projectsRawJson !== undefined)
            selectedPrinterProjectsRawJson = String(selected.projectsRawJson || "")
    }

    function refreshPrintersFromTimer() {
        if (!root.pageActive || !startupInitialized)
            return
        if (!hasCloudBridge()) {
            loadPrinters()
            return
        }
        if (typeof cloudBridge.refreshPrintersAsync === "function") {
            cloudBridge.refreshPrintersAsync(false)
            return
        }
        loadPrinters()
    }

    function ensureReasonCatalogLoaded() {
        if (reasonCatalogLoaded || reasonCatalogLoading)
            return
        if (!hasCloudBridge())
            return

        if (typeof cloudBridge.refreshReasonCatalogAsync === "function") {
            reasonCatalogLoading = true
            cloudBridge.refreshReasonCatalogAsync(false)
            return
        }

        if (typeof cloudBridge.fetchReasonCatalog === "function") {
            reasonCatalogLoading = true
            var r = cloudBridge.fetchReasonCatalog()
            reasonCatalogLoading = false
            if (r.ok !== true) {
                statusMsg = qsTr("Reason catalog unavailable: %1")
                        .arg(backendStatusDetail(r.message, qsTr("Catalog fetch failed.")))
                statusSev = "warn"
                return
            }

            var map = {}
            var reasons = r.reasons !== undefined ? r.reasons : []
            for (var i = 0; i < reasons.length; ++i) {
                var entry = reasons[i]
                map[String(entry.reason)] = entry
            }
            reasonCatalogByCode = map
            reasonCatalogLoaded = true
        }
    }

    function reasonEntryFromText(reasonText) {
        var text = String(reasonText || "").trim()
        if (text.length === 0)
            return null

        var code = ""
        if (/^-?\d+$/.test(text)) {
            code = text
        } else {
            var m = text.match(/-?\d+/)
            if (m && m.length > 0)
                code = String(m[0])
        }
        if (code.length === 0)
            return null

        var entry = reasonCatalogByCode[code]
        return entry !== undefined ? entry : null
    }

    function displayReason(reasonText) {
        var text = String(reasonText || "").trim()
        if (text.length === 0)
            return ""

        var entry = reasonEntryFromText(text)
        if (!entry)
            return translateLocalizedText(text)

        var desc = String(entry.desc || "").trim()
        if (desc.length === 0)
            return translateLocalizedText(text)
        return translateLocalizedText(desc) + " (" + translateLocalizedText(text) + ")"
    }

    function reasonHelpUrl(reasonText) {
        var entry = reasonEntryFromText(reasonText)
        if (!entry)
            return ""
        return String(entry.helpUrl || "").trim()
    }

    function loadSelectedPrinterInsights(reason, force, resetHistory, refreshCloud) {
        lastJobsRefreshReason = String(reason || "")
        selectedPrinterDetails = ({})
        selectedPrinterDetailsRawJson = ""
        selectedPrinterProjectsRawJson = ""
        liveProjectData = ({})

        if (selectedPrinterId.length === 0)
            return

        if (resetHistory === true)
            printerHistoryModel.clear()

        var selected = selectedPrinterData()
        if (selected) {
            if (selected.details !== undefined)
                updateSelectedPrinterDetails(selected.details)
            if (selected.detailsRawJson !== undefined)
                selectedPrinterDetailsRawJson = String(selected.detailsRawJson || "")
            if (selected.projectsRawJson !== undefined)
                selectedPrinterProjectsRawJson = String(selected.projectsRawJson || "")
        }

        if (!hasCloudBridge())
            return

        if (typeof cloudBridge.loadCachedPrinterProjectsAsync === "function") {
            cloudBridge.loadCachedPrinterProjectsAsync(selectedPrinterId, 1, 20)
        } else if (typeof cloudBridge.loadCachedPrinterProjects === "function") {
            var cachedProjectsRes = cloudBridge.loadCachedPrinterProjects(selectedPrinterId, 1, 20)
            if (cachedProjectsRes.ok === true) {
                var cachedProjectsList = cachedProjectsRes.projects !== undefined ? cachedProjectsRes.projects : []
                setLiveProjectFromList(cachedProjectsList)
                replaceRecentJobsCard(cachedProjectsList)
            }
        } else if (selected && selected.projects !== undefined) {
            var inlineProjects = selected.projects
            setLiveProjectFromList(inlineProjects)
            replaceRecentJobsCard(inlineProjects)
        }

        if (refreshCloud !== true) {
            loadingPrinterDetails = false
            loadingPrinterHistory = false
            updatePrintersAutoRefreshInterval()
            return
        }

        if (typeof cloudBridge.refreshPrinterInsightsAsync === "function") {
            loadingPrinterDetails = true
            loadingPrinterHistory = true
            cloudBridge.refreshPrinterInsightsAsync(selectedPrinterId, 1, 20, force === true)
        } else {
            if (typeof cloudBridge.fetchPrinterDetails === "function") {
                loadingPrinterDetails = true
                var detailsRes = cloudBridge.fetchPrinterDetails(selectedPrinterId)
                loadingPrinterDetails = false
                if (detailsRes.ok === true && detailsRes.details !== undefined) {
                    var fetchedDetails = detailsRes.details
                    var hasFetchedDetails = fetchedDetails !== null
                            && fetchedDetails !== undefined
                            && Object.keys(fetchedDetails).length > 0
                    var hasCurrentDetails = selectedPrinterDetails !== null
                            && selectedPrinterDetails !== undefined
                            && Object.keys(selectedPrinterDetails).length > 0
                    if (hasFetchedDetails || !hasCurrentDetails)
                        updateSelectedPrinterDetails(fetchedDetails)
                }
                if (detailsRes.rawJson !== undefined)
                    selectedPrinterDetailsRawJson = String(detailsRes.rawJson || selectedPrinterDetailsRawJson)
            }

            if (typeof cloudBridge.fetchPrinterProjects === "function") {
                loadingPrinterHistory = true
                var projectsRes = cloudBridge.fetchPrinterProjects(selectedPrinterId, 1, 20)
                loadingPrinterHistory = false
                if (projectsRes.ok === true) {
                    var cloudProjects = projectsRes.projects !== undefined ? projectsRes.projects : []
                    setLiveProjectFromList(cloudProjects)
                    replaceRecentJobsCard(cloudProjects)
                }
                if (projectsRes.rawJson !== undefined)
                    selectedPrinterProjectsRawJson = String(projectsRes.rawJson || selectedPrinterProjectsRawJson)
            }
        }

        updatePrintersAutoRefreshInterval()
    }

    function refreshSelectedPrinterJobs(reason, force, resetHistory) {
        if (selectedPrinterId.length === 0)
            return
        loadSelectedPrinterInsights(reason, force === true, resetHistory === true, true)
    }

    function loadMockPrinters() {
        printersEndpointPath = "demo://printers"
        printersEndpointRawJson = "{\n  \"mode\": \"demo\",\n  \"message\": \"Backend unavailable\"\n}"
        printersModel.replaceOrPatchPrinters([
        {
            "id": "demo-printer-1",
            "name": "M7-Workshop-A",
            "model": "Photon Mono M7",
            "type": "LCD",
            "state": "READY",
            "reason": "free",
            "available": 1,
            "progress": -1,
            "elapsedSec": -1,
            "remainingSec": -1,
            "currentFile": "",
            "lastSeen": "just now"
        },
        {
            "id": "demo-printer-2",
            "name": "M5S-Line-2",
            "model": "Photon Mono M5s",
            "type": "LCD",
            "state": "PRINTING",
            "reason": "printing",
            "available": 1,
            "progress": 43,
            "elapsedSec": 7800,
            "remainingSec": 10320,
            "currentFile": "atlas_plate_v12.pwmb",
            "lastSeen": "1 min ago"
        },
        {
            "id": "demo-printer-3",
            "name": "Backup-X2",
            "model": "Photon Mono X2",
            "type": "LCD",
            "state": "OFFLINE",
            "reason": "offline",
            "available": 0,
            "progress": -1,
            "elapsedSec": -1,
            "remainingSec": -1,
            "currentFile": "",
            "lastSeen": "23 min ago"
        }
        ])

        if (selectedPrinterId.length === 0 && printersModel.count > 0)
            selectedPrinterId = String(printersModel.get(0).id)

        statusMsg = qsTr("Demo mode (backend unavailable).")
        statusSev = "warn"
        loading = false
    }

    function applyPrintersLoadResult(r, useCacheFlow) {
        loading = false
        printersEndpointPath = String(r.endpoint || printersEndpointPath)
        printersEndpointRawJson = String(r.rawJson || "")

        var printers = r.printers !== undefined ? r.printers : []
        replacePrintersModel(printers, false)
        if (!startupJobsRefreshed && !remotePrintPrinterBootstrapPending) {
            startupJobsRefreshed = true
            refreshSelectedPrinterJobs("startup", true, true)
        }
        updatePrintersAutoRefreshInterval()
        if (printersModel.count > 0) {
            if (useCacheFlow) {
                statusMsg = qsTr("%1 printer(s) loaded from local cache. Syncing cloud...").arg(String(printersModel.count))
                statusSev = "info"
            } else {
                statusMsg = qsTr("%1 printer(s) loaded").arg(String(printersModel.count))
                statusSev = "success"
            }
        } else {
            if (useCacheFlow) {
                statusMsg = qsTr("No local cache yet. Syncing cloud...")
                statusSev = "warn"
            } else {
                statusMsg = qsTr("No printer found.")
                statusSev = "warn"
            }
        }
        if (useCacheFlow)
            cloudBridge.refreshPrintersAsync(true)
    }

    function loadPrinters() {
        if (loading)
            return

        loading = true
        statusMsg = qsTr("Loading printers from local cache...")
        statusSev = "info"

        if (!hasCloudBridge()) {
            loadMockPrinters()
            return
        }

        var useCacheFlow = typeof cloudBridge.loadCachedPrinters === "function"
                && typeof cloudBridge.refreshPrintersAsync === "function"
        var useAsyncCacheFlow = typeof cloudBridge.loadCachedPrintersAsync === "function"
                && typeof cloudBridge.refreshPrintersAsync === "function"
        if (useAsyncCacheFlow) {
            cloudBridge.loadCachedPrintersAsync()
            return
        }
        var r = useCacheFlow ? cloudBridge.loadCachedPrinters() : cloudBridge.fetchPrinters()
        applyPrintersLoadResult(r, useCacheFlow)
    }

    function ensureStartupInitialized() {
        if (startupInitialized)
            return
        startupInitialized = true
        ensureReasonCatalogLoaded()
        loadPrinters()
        updatePrintersAutoRefreshInterval()
    }

    function applyPrintersModel(printers) {
        var list = printers !== undefined ? printers : []
        return printersModel.replaceOrPatchPrinters(list)
    }

    function replacePrintersModel(printers, refreshInsights) {
        var changed = applyPrintersModel(printers)

        if (printersModel.count > 0) {
            var keepSelection = false
            for (var j = 0; j < printersModel.count; ++j) {
                if (String(printersModel.get(j).id) === selectedPrinterId) {
                    keepSelection = true
                    break
                }
            }
            if (!keepSelection)
                selectedPrinterId = String(printersModel.get(0).id)
        } else {
            selectedPrinterId = ""
        }

        if (changed)
            detectPrintCompletionTransitions()
        var shouldRefreshInsights = refreshInsights === true
        if (shouldRefreshInsights)
            refreshSelectedPrinterJobs("explicit", true, true)
        else if (changed)
            syncSelectedPrinterDetailsFromModel()
        if (changed || shouldRefreshInsights)
            refreshSelectedPrinterLiveSnapshot()
        updatePrintersAutoRefreshInterval()
        return changed
    }

    function detectPrintCompletionTransitions() {
        var nextState = {}
        var finishedPrinterIds = []
        for (var i = 0; i < printersModel.count; ++i) {
            var printer = printersModel.get(i)
            var printerId = String(printer.id || "")
            if (printerId.length === 0)
                continue
            var active = printerHasActiveJob(printer)
            var hadActive = printerHadActiveJobById[printerId] === true
            nextState[printerId] = active
            if (hadActive && !active)
                finishedPrinterIds.push(printerId)
        }
        printerHadActiveJobById = nextState
        for (var j = 0; j < finishedPrinterIds.length; ++j) {
            var finishedPrinterId = finishedPrinterIds[j]
            if (hasPrintWorkflowBridge()
                    && typeof printWorkflowBridge.beginRemotePostPrintCleanup === "function")
                printWorkflowBridge.beginRemotePostPrintCleanup(finishedPrinterId)
            else
                clearPendingRemotePrint(finishedPrinterId)
        }
        if (finishedPrinterIds.indexOf(selectedPrinterId) >= 0)
            refreshSelectedPrinterJobs("print_finished", true, false)
    }

    function refreshPrintersFromCacheOnly() {
        if (!hasCloudBridge())
            return
        if (typeof cloudBridge.loadCachedPrintersAsync === "function") {
            cloudBridge.loadCachedPrintersAsync()
            return
        }
        if (typeof cloudBridge.loadCachedPrinters !== "function")
            return
        var r = cloudBridge.loadCachedPrinters()
        if (r.ok !== true)
            return
        printersEndpointPath = String(r.endpoint || printersEndpointPath)
        printersEndpointRawJson = String(r.rawJson || "")
        var list = r.printers !== undefined ? r.printers : []
        replacePrintersModel(list, false)
    }

    function firstPrinterId() {
        if (printersModel.count <= 0)
            return ""
        return String(printersModel.get(0).id || "")
    }

    function printersSnapshotForWorkflow() {
        var rows = []
        for (var i = 0; i < printersModel.count; ++i) {
            var source = printersModel.get(i)
            var row = ({})
            for (var key in source)
                row[key] = source[key]
            rows.push(row)
        }
        return rows
    }

    function applyRemotePrintPreparation(result) {
        var preparation = result || ({})
        remotePrintCompatibilityResult = preparation.compatibilityResult || null
        remoteCompatiblePrintersModel.replaceOrPatchPrinters(preparation.compatiblePrinters || [])
        var targetPrinterId = String(preparation.selectedPrinterId || "").trim()
        remotePrinterId = targetPrinterId
        if (targetPrinterId.length > 0)
            choosePrinter(targetPrinterId)
        remotePrintAllowed = preparation.allowed === true
        remotePrintBlockReason = remotePrintReasonText(preparation.blockReasonKey, preparation.blockReason)
        remotePrintPreparing = false
        remotePrintPrepareMessage = ""

        if (preparation.bestEffortWarning === true && targetPrinterId.length > 0) {
            statusMsg = qsTr("Compatibility check failed. Continuing with best-effort printer selection.")
            statusSev = "warn"
            return
        }
        if (targetPrinterId.length === 0 || preparation.allowed !== true) {
            statusMsg = remotePrintBlockReason.length > 0
                    ? qsTr("Print blocked: %1").arg(remotePrintBlockReason)
                    : qsTr("Print blocked by compatibility checks.")
            statusSev = "warn"
            return
        }
        var displayName = String(preparation.fileName || preparation.fileId || "")
        statusMsg = qsTr("Remote print prepared for %1").arg(displayName)
        statusSev = "info"
    }

    function setSingleRemotePrintFile(fileId, fileName, fileData) {
        var normalizedFileId = String(fileId || "").trim()
        if (normalizedFileId.length === 0)
            return

        var normalizedFileName = String(fileName || "").trim()
        var existing = fileData !== null && fileData !== undefined
                && Object.keys(fileData).length > 0
                ? fileData
                : cloudFileDataById(normalizedFileId)
        if (!existing)
            existing = loadCloudFileDetailsById(normalizedFileId)

        var entry = ({})
        if (existing) {
            var existingKeys = Object.keys(existing)
            for (var i = 0; i < existingKeys.length; ++i) {
                var key = existingKeys[i]
                entry[key] = existing[key]
            }
        }
        entry.fileId = normalizedFileId
        entry.fileName = normalizedFileName.length > 0
                ? normalizedFileName
                : String(entry.fileName || qsTr("Cloud file"))
        entry.sizeText = String(entry.sizeText || "-")
        entry.status = String(entry.status || "READY")
        entry.printTime = cloudFilePrintTimeText(entry)
        entry.resinUsage = cloudFileResinUsageText(entry)

        printCloudFilesModel.replaceOrPatchFiles([entry])
        selectedCloudFileId = normalizedFileId
    }

    function openRemotePrintFromFile(fileId, fileName, fileData) {
        var normalizedFileId = String(fileId || "").trim()
        var normalizedFileName = String(fileName || "").trim()
        if (normalizedFileId.length === 0) {
            statusMsg = qsTr("Cannot start remote print: missing file id.")
            statusSev = "warn"
            return
        }

        remotePrintMode = "cloud"
        directPrintLocalPath = ""
        directPrintLocalName = ""
        setSingleRemotePrintFile(normalizedFileId, normalizedFileName, fileData)
        remotePrintCompatibilityResult = null
        remoteCompatiblePrintersModel.clear()
        remotePrinterId = selectedPrinterId.length > 0 ? selectedPrinterId : firstPrinterId()
        remotePrintPreparing = true
        remotePrintPrepareMessage = qsTr("Checking printer compatibility...")
        remotePrintAllowed = false
        remotePrintBlockReason = ""
        optionDeleteAfterPrint = false
        remotePrintConfigDialog.open()

        Qt.callLater(function() {
            root.prepareRemotePrintDialog(normalizedFileId, normalizedFileName)
        })
    }

    function openDirectPrintFromLocalFile(localPath, fileName, completePreview,
                                                deleteLocalOnFailurePreference) {
        var normalizedPath = String(localPath || "").trim()
        var normalizedName = String(fileName || "").trim()
        if (normalizedPath.length === 0 || normalizedName.length === 0) {
            statusMsg = qsTr("Cannot start direct print: missing local file.")
            statusSev = "warn"
            return
        }
        remotePrintMode = "direct"
        directPrintLocalPath = normalizedPath
        directPrintLocalName = normalizedName
        directPrintCompletePreview = completePreview === true
        directPrintDeleteLocalOnFailureSnapshot = deleteLocalOnFailurePreference === true
        setSingleRemotePrintFile("direct-local-pending", normalizedName, {
            "fileId": "direct-local-pending",
            "fileName": normalizedName,
            "status": "LOCAL",
            "printTime": "-",
            "resinUsage": "-",
            "directMode": true
        })
        remotePrintCompatibilityResult = null
        remoteCompatiblePrintersModel.clear()
        remotePrinterId = selectedPrinterId.length > 0 ? selectedPrinterId : firstPrinterId()
        remotePrintPreparing = true
        remotePrintPrepareMessage = qsTr("Checking printer compatibility...")
        remotePrintAllowed = false
        remotePrintBlockReason = ""
        optionDeleteAfterPrint = false
        remotePrintConfigDialog.open()
        Qt.callLater(function() {
            root.prepareRemotePrintDialog("", normalizedName)
        })
    }

    function resumeRemotePrintPreparationAfterPrinterLoad() {
        if (!remotePrintPrinterBootstrapPending || !remotePrintPreparing
                || printersModel.count <= 0)
            return
        var fileId = pendingRemotePrintBootstrapFileId
        var fileName = pendingRemotePrintBootstrapFileName
        remotePrintPrinterBootstrapPending = false
        Qt.callLater(function() { root.prepareRemotePrintDialog(fileId, fileName) })
    }

    function prepareRemotePrintDialog(fileId, fileName) {
        var normalizedFileId = String(fileId || "").trim()
        var normalizedFileName = String(fileName || "").trim()
        if (printersModel.count <= 0) {
            pendingRemotePrintBootstrapFileId = normalizedFileId
            pendingRemotePrintBootstrapFileName = normalizedFileName
            if (!remotePrintPrinterBootstrapPending) {
                remotePrintPrinterBootstrapPending = true
                remotePrintPrepareMessage = qsTr("Loading available printers...")
                loadPrinters()
            }
            if (printersModel.count <= 0)
                return
            remotePrintPrinterBootstrapPending = false
        }

        if (!hasRemotePrintWorkflowBridge()) {
            remotePrintPreparing = false
            remotePrintAllowed = false
            remotePrintPrepareMessage = ""
            remotePrintBlockReason = qsTr("Print workflow backend is unavailable.")
            statusMsg = remotePrintBlockReason
            statusSev = "warn"
            return
        }

        var preferredPrinterId = selectedPrinterId.length > 0 ? selectedPrinterId : firstPrinterId()
        var fileData = selectedCloudFileData() || ({})
        remotePrintPreparing = true
        remotePrintPrepareMessage = qsTr("Checking printer compatibility...")
        printWorkflowBridge.beginRemotePrintPreparation(remotePrintMode,
                                                        normalizedFileId,
                                                        normalizedFileName,
                                                        fileData,
                                                        printersSnapshotForWorkflow(),
                                                        preferredPrinterId)
    }

    function applyCloudFilesForRemotePrint(files, loadedFromLocalCache) {
        var list = files !== undefined && files !== null ? files : []
        var compatibleRows = []
        var hiddenIncompatibleCount = 0
        var hiddenMissingMetadataCount = 0

        for (var i = 0; i < list.length; ++i) {
            var file = list[i]
            var printer = printerDataById(pendingCloudFilesPrinterId)
            var localCompat = hasLocalCompatibilityEvaluator()
                    ? cloudBridge.evaluateLocalPrinterFileCompatibility(printer || ({}), file || ({}))
                    : ({ "ok": false, "score": 0, "reason": qsTr("Missing local compatibility metadata.") })
            if (localCompat.ok === true) {
                compatibleRows.push({
                    "score": Number(localCompat.score || 0),
                    "file": file
                })
                continue
            }
            hiddenIncompatibleCount += 1
            if (String(localCompat.reason || "").toLowerCase().indexOf("metadata") !== -1)
                hiddenMissingMetadataCount += 1
        }

        compatibleRows.sort(function(a, b) {
            var scoreA = Number(a.score || 0)
            var scoreB = Number(b.score || 0)
            if (scoreA !== scoreB)
                return scoreB - scoreA
            var nameA = String(a.file && a.file.fileName !== undefined ? a.file.fileName : "").toLowerCase()
            var nameB = String(b.file && b.file.fileName !== undefined ? b.file.fileName : "").toLowerCase()
            if (nameA < nameB)
                return -1
            if (nameA > nameB)
                return 1
            return 0
        })

        var compatibleFiles = []
        for (var rowIndex = 0; rowIndex < compatibleRows.length; ++rowIndex) {
            var row = compatibleRows[rowIndex]
            if (row.file !== undefined)
                compatibleFiles.push(row.file)
        }
        printCloudFilesModel.replaceOrPatchFiles(compatibleFiles)

        if (printCloudFilesModel.count > 0) {
            selectedCloudFileId = cloudFileIdValue(printCloudFilesModel.get(0))
            if (hiddenIncompatibleCount > 0) {
                statusMsg = qsTr("%1 compatible cloud file(s) shown. %2 hidden as incompatible.")
                        .arg(String(printCloudFilesModel.count))
                        .arg(String(hiddenIncompatibleCount))
                statusSev = "info"
            } else {
                statusMsg = loadedFromLocalCache
                        ? qsTr("%1 compatible cloud file(s) loaded from local cache.")
                            .arg(String(printCloudFilesModel.count))
                        : qsTr("%1 compatible cloud file(s) loaded.")
                            .arg(String(printCloudFilesModel.count))
                statusSev = "success"
            }
            cloudFilesLoading = false
            pendingCloudFilesPrinterId = ""
            return
        }

        if (list.length <= 0) {
            statusMsg = loadedFromLocalCache
                    ? qsTr("No local cloud cache yet.")
                    : qsTr("No cloud file available.")
            statusSev = "warn"
            cloudFilesLoading = false
            pendingCloudFilesPrinterId = ""
            return
        }
        if (hiddenMissingMetadataCount > 0 && hiddenMissingMetadataCount === list.length) {
            statusMsg = qsTr("No compatible cloud file: local metadata missing for all files.")
            statusSev = "warn"
            cloudFilesLoading = false
            pendingCloudFilesPrinterId = ""
            return
        }
        statusMsg = qsTr("No compatible cloud file available for this printer.")
        statusSev = "warn"
        cloudFilesLoading = false
        pendingCloudFilesPrinterId = ""
    }

    function loadCloudFilesForRemotePrint(printerId) {
        var targetPrinterId = String(printerId || "").trim()
        cloudFilesLoading = true
        printCloudFilesModel.clear()
        selectedCloudFileId = ""
        pendingCloudFilesPrinterId = targetPrinterId

        if (targetPrinterId.length === 0) {
            statusMsg = qsTr("No printer selected for cloud file filtering.")
            statusSev = "warn"
            cloudFilesLoading = false
            pendingCloudFilesPrinterId = ""
            return
        }

        var files = []
        var loadedFromLocalCache = false
        if (hasCloudBridge()) {
            if (typeof cloudBridge.loadCachedFilesAsync === "function") {
                cloudBridge.loadCachedFilesAsync(1, 200)
                return
            } else if (typeof cloudBridge.loadCachedFiles === "function") {
                var cached = cloudBridge.loadCachedFiles(1, 200)
                if (cached.ok === true) {
                    files = cached.files !== undefined ? cached.files : []
                    loadedFromLocalCache = true
                }
            }

            if (files.length <= 0) {
                var listing = cloudBridge.fetchFiles(1, 200)
                if (listing.ok === true) {
                    files = listing.files !== undefined ? listing.files : []
                    loadedFromLocalCache = false
                } else {
                    statusMsg = qsTr("Cannot load cloud files for print: %1")
                            .arg(backendStatusDetail(listing.message, qsTr("Cloud listing unavailable.")))
                    statusSev = "error"
                    cloudFilesLoading = false
                    return
                }
            }
        } else {
            files = [
                {
                    "fileId": "demo-001",
                    "fileName": "rook_plate_v12.pwmb",
                    "machine": "Anycubic Photon Mono M7 Pro",
                    "sizeText": "42.6 MB",
                    "status": "READY",
                    "printTime": "02h 15m",
                    "resinUsage": "67 ml"
                },
                {
                    "fileId": "demo-002",
                    "fileName": "calibration_tower.pws",
                    "machine": "Anycubic Photon M3 Plus",
                    "sizeText": "11.8 MB",
                    "status": "READY",
                    "printTime": "00h 48m",
                    "resinUsage": "14 ml"
                }
            ]
        }

        applyCloudFilesForRemotePrint(files, loadedFromLocalCache)
    }

    function openSelectCloudFileDialog(printerId) {
        remotePrinterId = String(printerId || selectedPrinterId)
        selectCloudFileDialog.open()
        if (typeof Qt.callLater === "function") {
            Qt.callLater(function() {
                root.loadCloudFilesForRemotePrint(root.remotePrinterId)
            })
        } else {
            loadCloudFilesForRemotePrint(remotePrinterId)
        }
    }

    function startCloudFileRemotePrint(printerId) {
        var targetPrinterId = String(printerId || selectedPrinterId).trim()
        if (targetPrinterId.length === 0 && printersModel.count <= 0)
            loadPrinters()
        if (targetPrinterId.length === 0)
            targetPrinterId = firstPrinterId()
        if (targetPrinterId.length === 0) {
            statusMsg = qsTr("No printer available for remote print.")
            statusSev = "warn"
            return
        }

        choosePrinter(targetPrinterId)
        openSelectCloudFileDialog(targetPrinterId)
        var targetPrinter = printerDataById(targetPrinterId)
        statusMsg = qsTr("Select a cloud file for %1.")
                .arg(targetPrinter ? String(targetPrinter.name || targetPrinterId) : targetPrinterId)
        statusSev = "info"
    }

    function openLocalFileDialogForRemotePrint(printerId) {
        var targetPrinterId = String(printerId || selectedPrinterId).trim()
        if (targetPrinterId.length === 0 && printersModel.count <= 0)
            loadPrinters()
        if (targetPrinterId.length === 0)
            targetPrinterId = firstPrinterId()
        if (targetPrinterId.length === 0) {
            statusMsg = qsTr("No printer available for remote print.")
            statusSev = "warn"
            return
        }

        var printer = printerDataById(targetPrinterId)
        var stateCheck = canStartFromPrinterState(printer)
        if (stateCheck.ok !== true) {
            statusMsg = translateLocalizedText(String(stateCheck.reason || qsTr("Printer is not ready.")))
            statusSev = "warn"
            return
        }

        choosePrinter(targetPrinterId)
        localFilesTargetPrinterId = targetPrinterId
        selectedPrinterLocalFileName = ""
        printerLocalFilesModel.clear()
        localFilesLoading = true
        selectLocalPrinterFileDialog.open()

        if (!hasPrinterOrderEndpoint()) {
            localFilesLoading = false
            statusMsg = qsTr("Printer local file listing requires sendOrder backend support.")
            statusSev = "warn"
            return
        }
        if (!hasConnectedMqttBridge()) {
            pendingLocalFilesPrinterId = targetPrinterId
            statusMsg = qsTr("Connecting MQTT to retrieve printer local files...")
            statusSev = "info"
            if (typeof mqttBridge !== "undefined"
                    && mqttBridge !== null
                    && typeof mqttBridge.ensureAutoConnected === "function") {
                mqttBridge.ensureAutoConnected()
            } else {
                localFilesLoading = false
                pendingLocalFilesPrinterId = ""
                statusMsg = qsTr("MQTT must be connected to receive printer local file list.")
                statusSev = "warn"
            }
            return
        }

        pendingLocalFilesPrinterId = ""
        requestPrinterLocalFiles(targetPrinterId)
    }

    function requestPrinterLocalFiles(targetPrinterId) {
        if (typeof cloudBridge.sendPrinterOrderAsync === "function") {
            cloudBridge.sendPrinterOrderAsync(targetPrinterId,
                                              localFilesPrepareOrderId,
                                              {},
                                              targetPrinterId,
                                              { "kind": "localFilesPrepare" })
            cloudBridge.sendPrinterOrderAsync(targetPrinterId,
                                              localFilesListOrderId,
                                              { "path": "/" },
                                              targetPrinterId,
                                              { "kind": "localFilesList" })
        } else {
            var prepareResult = cloudBridge.sendPrinterOrder(targetPrinterId,
                                                             localFilesPrepareOrderId,
                                                             {},
                                                             targetPrinterId)
            if (prepareResult.ok !== true) {
                statusMsg = qsTr("Printer file manager warmup failed: %1")
                        .arg(backendStatusDetail(prepareResult.message, qsTr("Warmup request failed.")))
                statusSev = "warn"
            }

            var listResult = cloudBridge.sendPrinterOrder(targetPrinterId,
                                                          localFilesListOrderId,
                                                          { "path": "/" },
                                                          targetPrinterId)
            if (listResult.ok !== true) {
                localFilesLoading = false
                statusMsg = qsTr("Cannot request printer local files: %1")
                        .arg(backendStatusDetail(listResult.message, qsTr("List request failed.")))
                statusSev = "error"
                return
            }
        }

        statusMsg = qsTr("Loading local files from printer...")
        statusSev = "info"
    }

    function openPrinterDetailsDialog(printerId) {
        var normalizedPrinterId = String(printerId || "").trim()
        if (normalizedPrinterId.length > 0)
            choosePrinter(normalizedPrinterId)
        printerDetailsDialog.open()
    }

    function applyPrinterLocalFilesFromMqtt(printerId, source, records, state, code, message) {
        var normalizedPrinterId = String(printerId || "").trim()
        var normalizedSource = String(source || "").toLowerCase()
        if (normalizedSource !== "local")
            return
        if (normalizedPrinterId.length === 0)
            return

        var targetPrinterId = String(localFilesTargetPrinterId || "").trim()
        var targetPrinter = printerDataById(targetPrinterId)
        var targetPrinterKey = targetPrinter
                ? String(targetPrinter.printerKey || "").trim()
                : ""
        if (normalizedPrinterId !== targetPrinterId
                && (targetPrinterKey.length === 0 || normalizedPrinterId !== targetPrinterKey))
            return

        localFilesLoading = false
        selectedPrinterLocalFileName = ""

        var localFiles = []
        var list = records !== undefined ? records : []
        for (var i = 0; i < list.length; ++i) {
            var record = list[i]
            var fileName = String(record.filename || record.fileName || record.file_name || record.name || "").trim()
            if (fileName.length === 0)
                continue
            var isDir = record.isDir === true || record.is_dir === true
                    || Number(record.isDir) > 0 || Number(record.is_dir) > 0
            if (isDir)
                continue

            var sizeValue = Number(record.size)
            if (!isFinite(sizeValue) || sizeValue < 0)
                sizeValue = 0
            var timestampValue = Number(record.timestamp)
            if (!isFinite(timestampValue) || timestampValue < 0)
                timestampValue = 0

            localFiles.push({
                "fileId": fileName,
                "fileName": fileName,
                "sizeText": bytesText(sizeValue),
                "status": qsTr("On printer"),
                "printTime": timestampValue > 0 ? unixTimeText(timestampValue) : "-"
            })
        }
        printerLocalFilesModel.replaceOrPatchFiles(localFiles)

        if (printerLocalFilesModel.count > 0) {
            selectedPrinterLocalFileName = String(printerLocalFilesModel.get(0).fileId || "")
            statusMsg = qsTr("%1 local file(s) loaded from printer.")
                    .arg(String(printerLocalFilesModel.count))
            statusSev = "success"
            return
        }

        var normalizedState = String(state || "").toLowerCase()
        if (normalizedState === "failed" || Number(code) >= 400) {
            statusMsg = backendStatusDetail(message, qsTr("Failed to load printer local files."))
            statusSev = "error"
            return
        }

        statusMsg = qsTr("No local file available on printer.")
        statusSev = "warn"
    }

    function startPrintFromPrinterLocalFile() {
        var targetPrinterId = String(localFilesTargetPrinterId || selectedPrinterId).trim()
        if (targetPrinterId.length === 0) {
            statusMsg = qsTr("Select a printer first.")
            statusSev = "warn"
            return
        }

        var printer = printerDataById(targetPrinterId)
        var stateCheck = canStartFromPrinterState(printer)
        if (stateCheck.ok !== true) {
            statusMsg = translateLocalizedText(String(stateCheck.reason || qsTr("Printer is not ready.")))
            statusSev = "warn"
            return
        }

        var selectedFileName = String(selectedPrinterLocalFileName || "").trim()
        if (selectedFileName.length === 0) {
            statusMsg = qsTr("Select a printer local file first.")
            statusSev = "warn"
            return
        }

        if (!hasPrinterOrderEndpoint()) {
            statusMsg = qsTr("Local printer print requires sendOrder backend support.")
            statusSev = "warn"
            return
        }

        loading = true
        statusMsg = qsTr("Sending local print task for %1...")
                .arg(selectedFileName)
        statusSev = "info"

        if (typeof cloudBridge.sendPrinterOrderAsync === "function") {
            cloudBridge.sendPrinterOrderAsync(targetPrinterId,
                                              localFileStartPrintOrderId,
                                              {
                                                  "filename": selectedFileName,
                                                  "path": "/"
                                              },
                                              targetPrinterId,
                                              { "kind": "localFileStart" })
        } else {
            var uploadRes = cloudBridge.sendPrinterOrder(targetPrinterId,
                                                         localFileStartPrintOrderId,
                                                         {
                                                             "filename": selectedFileName,
                                                             "path": "/"
                                                         },
                                                         targetPrinterId)
            loading = false
            applyLocalPrintCommandResult(targetPrinterId, uploadRes)
        }
    }

    function applyLocalPrintCommandResult(targetPrinterId, result) {
        if (result.ok !== true) {
            statusMsg = qsTr("Local print task failed: %1")
                    .arg(backendStatusDetail(result.message, qsTr("Task rejected.")))
            statusSev = "error"
            return
        }

        selectLocalPrinterFileDialog.close()
        if (targetPrinterId === selectedPrinterId)
            refreshSelectedPrinterJobs("print_started", true, false)
        loadPrinters()
        var msgId = String(result.msgId || "").trim()
        statusMsg = qsTr("Local print task sent (order_id=%1, msgid=%2).")
                .arg(String(localFileStartPrintOrderId))
                .arg(msgId.length > 0 ? msgId : "-")
        statusSev = "success"
    }

    function removePrinterLocalFileFromModel(fileName) {
        var normalized = String(fileName || "").trim()
        if (normalized.length === 0)
            return false
        for (var i = 0; i < printerLocalFilesModel.count; ++i) {
            if (String(printerLocalFilesModel.get(i).fileId || "") === normalized) {
                printerLocalFilesModel.remove(i)
                if (selectedPrinterLocalFileName === normalized)
                    selectedPrinterLocalFileName = printerLocalFilesModel.count > 0
                            ? String(printerLocalFilesModel.get(0).fileId || "")
                            : ""
                return true
            }
        }
        return false
    }

    function deletePrinterLocalFile(fileName) {
        var targetPrinterId = String(localFilesTargetPrinterId || selectedPrinterId).trim()
        var normalizedFileName = String(fileName || selectedPrinterLocalFileName || "").trim()
        if (targetPrinterId.length === 0) {
            statusMsg = qsTr("Select a printer first.")
            statusSev = "warn"
            return
        }
        if (normalizedFileName.length === 0) {
            statusMsg = qsTr("Select a printer local file first.")
            statusSev = "warn"
            return
        }
        if (!hasPrinterOrderEndpoint()) {
            statusMsg = qsTr("Local printer file deletion requires sendOrder backend support.")
            statusSev = "warn"
            return
        }

        statusMsg = qsTr("Deleting local file %1...").arg(normalizedFileName)
        statusSev = "info"
        var payload = {
            "filename": normalizedFileName,
            "path": "/"
        }
        if (typeof cloudBridge.sendPrinterOrderAsync === "function") {
            cloudBridge.sendPrinterOrderAsync(targetPrinterId,
                                              localFileDeleteOrderId,
                                              payload,
                                              targetPrinterId,
                                              { "kind": "localFileDelete", "fileName": normalizedFileName })
        } else {
            var result = cloudBridge.sendPrinterOrder(targetPrinterId,
                                                      localFileDeleteOrderId,
                                                      payload,
                                                      targetPrinterId)
            applyLocalFileDeleteResult(targetPrinterId, normalizedFileName, result)
        }
    }

    function applyLocalFileDeleteResult(targetPrinterId, fileName, result) {
        if (result.ok !== true) {
            statusMsg = qsTr("Local file deletion failed: %1")
                    .arg(backendStatusDetail(result.message, qsTr("Task rejected.")))
            statusSev = "error"
            return
        }
        removePrinterLocalFileFromModel(fileName)
        statusMsg = qsTr("Local file deleted: %1").arg(fileName)
        statusSev = "success"
    }

    function startResinFeedOperation(printerId, feedType) {
        var targetPrinterId = String(printerId || selectedPrinterId).trim()
        if (targetPrinterId.length === 0) {
            statusMsg = qsTr("Select a printer first.")
            statusSev = "warn"
            return
        }
        var normalizedFeedType = Number(feedType)
        if (!(normalizedFeedType === 1 || normalizedFeedType === 2)) {
            statusMsg = qsTr("Invalid resin operation.")
            statusSev = "warn"
            return
        }
        if (!hasPrinterOrderEndpoint()) {
            statusMsg = qsTr("Resin operation requires sendOrder backend support.")
            statusSev = "warn"
            return
        }

        var payload = {
            "feed_type": normalizedFeedType,
            "type": 1
        }
        var label = normalizedFeedType === 1 ? qsTr("resin fill") : qsTr("resin drain")
        var context = { "kind": "resinFeedStart", "feedType": normalizedFeedType }
        statusMsg = qsTr("Starting %1 operation...").arg(label)
        statusSev = "info"
        resinFeedActive = true
        resinFeedType = normalizedFeedType
        resinFeedStopSubmitting = false
        resinFeedPrinterId = targetPrinterId
        resinFeedObservedRunningState = false

        if (typeof cloudBridge.sendPrinterOrderAsync === "function") {
            cloudBridge.sendPrinterOrderAsync(targetPrinterId,
                                              resinFeedOrderId,
                                              payload,
                                              targetPrinterId,
                                              context)
        } else {
            var result = cloudBridge.sendPrinterOrder(targetPrinterId,
                                                      resinFeedOrderId,
                                                      payload,
                                                      targetPrinterId)
            applyResinFeedStartResult(targetPrinterId, normalizedFeedType, result)
        }
    }

    function stopResinFeedOperation(printerId, feedType) {
        var targetPrinterId = String(printerId || resinFeedPrinterId || selectedPrinterId).trim()
        var normalizedFeedType = Number(feedType)
        if (!(normalizedFeedType === 1 || normalizedFeedType === 2))
            normalizedFeedType = Number(resinFeedType)
        if (targetPrinterId.length <= 0 || !(normalizedFeedType === 1 || normalizedFeedType === 2))
            return
        if (!hasPrinterOrderEndpoint())
            return
        resinFeedStopSubmitting = true
        statusMsg = qsTr("Stopping resin operation...")
        statusSev = "info"
        var payload = {
            "feed_type": normalizedFeedType,
            "type": 0
        }
        var context = { "kind": "resinFeedStop", "feedType": normalizedFeedType }
        if (typeof cloudBridge.sendPrinterOrderAsync === "function") {
            cloudBridge.sendPrinterOrderAsync(targetPrinterId,
                                              resinFeedOrderId,
                                              payload,
                                              targetPrinterId,
                                              context)
        } else {
            var result = cloudBridge.sendPrinterOrder(targetPrinterId,
                                                      resinFeedOrderId,
                                                      payload,
                                                      targetPrinterId)
            applyResinFeedStopResult(targetPrinterId, normalizedFeedType, result)
        }
    }

    function applyResinFeedStartResult(targetPrinterId, feedType, result) {
        if (result.ok !== true) {
            resinFeedActive = false
            resinFeedType = 0
            resinFeedStopSubmitting = false
            resinFeedPrinterId = ""
            resinFeedObservedRunningState = false
            statusMsg = qsTr("Resin operation failed: %1")
                    .arg(backendStatusDetail(result.message, qsTr("Task rejected.")))
            statusSev = "error"
            return
        }
        var msgId = String(result.msgId || "").trim()
        var label = Number(feedType) === 1 ? qsTr("resin fill") : qsTr("resin drain")
        statusMsg = qsTr("%1 sent (order_id=%2, msgid=%3).")
                .arg(label)
                .arg(String(resinFeedOrderId))
                .arg(msgId.length > 0 ? msgId : "-")
        statusSev = "success"
        loadPrinters()
        if (String(targetPrinterId || "") === selectedPrinterId)
            refreshSelectedPrinterJobs("resin_feed_start", true, false)
    }

    function applyResinFeedStopResult(targetPrinterId, feedType, result) {
        resinFeedStopSubmitting = false
        if (result.ok !== true) {
            statusMsg = qsTr("Resin stop failed: %1")
                    .arg(backendStatusDetail(result.message, qsTr("Task rejected.")))
            statusSev = "error"
            return
        }
        resinFeedActive = false
        resinFeedType = 0
        resinFeedPrinterId = ""
        resinFeedObservedRunningState = false
        var msgId = String(result.msgId || "").trim()
        statusMsg = qsTr("Resin operation stopped (order_id=%1, msgid=%2).")
                .arg(String(resinFeedOrderId))
                .arg(msgId.length > 0 ? msgId : "-")
        statusSev = "success"
        loadPrinters()
        if (String(targetPrinterId || "") === selectedPrinterId)
            refreshSelectedPrinterJobs("resin_feed_stop", true, false)
    }

    function openRemotePrintConfig() {
        if (!ensureSelectedCloudFile())
            return

        optionDeleteAfterPrint = false
        remotePrintConfigDialog.open()
        if (!remotePrintPreparing) {
            var selectedFile = selectedCloudFileData()
            remotePrintPreparing = true
            remotePrintPrepareMessage = qsTr("Checking printer compatibility...")
            prepareRemotePrintDialog(selectedCloudFileId,
                                     selectedFile ? String(selectedFile.fileName || "") : "")
        }
    }

    function submitPrintOrder(fileData) {
        var fileId = cloudFileIdValue(fileData)
        remotePrintSubmitting = true
        pendingPrintPrinterId = remotePrinterId
        pendingPrintFileId = fileId
        pendingPrintFileData = fileData
        pendingPrintDeleteAfterPrint = optionDeleteAfterPrint === true
        statusMsg = qsTr("Sending print order...")
        statusSev = "info"
        if (typeof cloudBridge.sendPrintOrderAsync === "function") {
            cloudBridge.sendPrintOrderAsync(remotePrinterId, fileId, false, false)
            return
        }
        var result = cloudBridge.sendPrintOrder(remotePrinterId, fileId, false, false)
        remotePrintSubmitting = false
        applyPrintOrderResult(remotePrinterId, fileId, result, fileData,
                              optionDeleteAfterPrint === true)
    }

    function startRemotePrint() {
        if (remotePrintPreparing || remotePrintSubmitting) {
            statusMsg = remotePrintPreparing
                    ? qsTr("Remote print setup is still loading.")
                    : qsTr("Print order is already being sent.")
            statusSev = "warn"
            return
        }
        if (remotePrinterId.length === 0) {
            statusMsg = qsTr("Select a printer first.")
            statusSev = "warn"
            return
        }
        refreshRemotePrintGuard()
        if (!remotePrintAllowed) {
            statusMsg = remotePrintBlockReason.length > 0
                    ? qsTr("Print blocked: %1").arg(remotePrintBlockReason)
                    : qsTr("Print blocked by compatibility checks.")
            statusSev = "warn"
            return
        }
        if (!hasCloudBridge()) {
            statusMsg = qsTr("Cloud backend is unavailable.")
            statusSev = "warn"
            return
        }

        if (remotePrintMode === "direct") {
            if (directPrintLocalPath.length === 0
                    || typeof cloudBridge.startUploadLocalFile !== "function") {
                statusMsg = qsTr("Direct print upload is unavailable.")
                statusSev = "error"
                return
            }
            remotePrintSubmitting = true
            remotePrintPreparing = true
            remotePrintPrepareMessage = qsTr("Uploading the file before printing...")
            pendingDirectUploadContext = "direct-print-" + String(Date.now())
            cloudBridge.startUploadLocalFile(directPrintLocalPath,
                                             directPrintCompletePreview,
                                             pendingDirectUploadContext)
            return
        }

        if (!ensureSelectedCloudFile()) {
            statusMsg = qsTr("Select a cloud file first.")
            statusSev = "warn"
            return
        }
        var fileData = selectedCloudFileData()
        if (!fileData) {
            statusMsg = qsTr("Select a cloud file first.")
            statusSev = "warn"
            return
        }
        optionDeleteAfterPrint = false
        submitPrintOrder(fileData)
    }

    function applyPrintOrderResult(printerId, fileId, r, fileData, deleteAfterPrint) {
        if (r.ok === true) {
            var taskId = String(r.taskId || "")
            var successMessage = taskId.length > 0
                    ? (qsTr("Print order sent (task_id=%1)").arg(taskId))
                    : qsTr("Print order sent.")
            var trackedFileData = fileData ? fileData : ({})
            trackedFileData.deleteAfterPrint = deleteAfterPrint === true
            trackedFileData.printMsgId = String(r.msgId || "")
            if (remotePrintMode === "direct") {
                trackedFileData.directMode = true
                trackedFileData.deleteLocalOnFailure = directPrintDeleteLocalOnFailureSnapshot === true
            }
            markRemotePrintAccepted(printerId, trackedFileData, taskId)
            remotePrintConfigDialog.close()
            if (printerId !== selectedPrinterId)
                choosePrinter(printerId)
            else
                refreshSelectedPrinterLiveSnapshot()
            refreshSelectedPrinterJobs("print_started", true, false)
            loadPrinters()
            statusMsg = successMessage
            statusSev = "success"
            root.remotePrintAccepted(printerId, taskId)
        } else {
            statusMsg = qsTr("Print order failed: %1")
                    .arg(backendStatusDetail(r.message, qsTr("Order rejected by backend.")))
            statusSev = "error"
        }
    }

    Component.onCompleted: {
        if (!deferStartupInitialization)
            ensureStartupInitialized()
    }

    Timer {
        id: printersAutoRefreshTimer
        objectName: "printersAutoRefreshTimer"
        interval: root.autoRefreshIntervalMs
        repeat: true
        running: false
        triggeredOnStart: false
        onTriggered: root.refreshPrintersFromTimer()
    }

    Timer {
        id: mqttRealtimeDebounceTimer
        interval: root.mqttRealtimeDebounceMs
        repeat: false
        running: false
        onTriggered: root.refreshPrintersFromCacheOnly()
    }

    Connections {
        target: (root && root.hasQObjectCloudBridge()) ? cloudBridge : null
        ignoreUnknownSignals: true

        function onPrintersUpdatedFromCloud(printers, message) {
            var list = printers !== undefined ? printers : []
            var changed = replacePrintersModel(list, false)
            root.resumeRemotePrintPreparationAfterPrinterLoad()
            if (!changed)
                return
            statusMsg = qsTr("%1 printer(s) refreshed from cloud.").arg(String(list.length))
            statusSev = "success"
        }

        function onCachedPrintersLoaded(result) {
            root.applyPrintersLoadResult(result, true)
            root.resumeRemotePrintPreparationAfterPrinterLoad()
        }

        function onCachedFilesLoaded(result) {
            if (!root.cloudFilesLoading || String(root.pendingCloudFilesPrinterId || "").length <= 0)
                return

            var list = result && result.files !== undefined ? result.files : []
            if (result && result.ok === true && list.length > 0) {
                root.applyCloudFilesForRemotePrint(list, true)
                return
            }

            if (typeof cloudBridge.refreshFilesAsync === "function") {
                cloudBridge.refreshFilesAsync(1, 200, true)
                return
            }

            root.applyCloudFilesForRemotePrint(list, true)
        }

        function onFilesUpdatedFromCloud(files, message) {
            if (!root.cloudFilesLoading || String(root.pendingCloudFilesPrinterId || "").length <= 0)
                return
            root.applyCloudFilesForRemotePrint(files !== undefined ? files : [], false)
        }

        function onCachedPrinterProjectsLoaded(printerId, result) {
            if (String(printerId || "") !== String(root.selectedPrinterId || ""))
                return
            if (result.ok === true) {
                var cachedProjectsList = result.projects !== undefined ? result.projects : []
                root.setLiveProjectFromList(cachedProjectsList)
                root.replaceRecentJobsCard(cachedProjectsList)
            }
        }

        function onUploadContextProgressChanged(requestContext, progress, phase) {
            if (String(requestContext || "") !== root.pendingDirectUploadContext)
                return
            root.remotePrintPrepareMessage = qsTr("Uploading before direct print: %1%")
                    .arg(String(Math.round(Number(progress) * 100)))
        }

        function onUploadContextFinished(requestContext, result) {
            if (String(requestContext || "") !== root.pendingDirectUploadContext)
                return
            root.pendingDirectUploadContext = ""
            root.remotePrintPreparing = false
            root.remotePrintPrepareMessage = ""
            if (!result || result.ok !== true) {
                root.remotePrintSubmitting = false
                root.statusMsg = qsTr("Direct print upload failed: %1")
                        .arg(root.backendStatusDetail(result ? result.message : "",
                                                      qsTr("Transfer failed.")))
                root.statusSev = "error"
                return
            }
            var fileId = String(result.fileId || "").trim()
            if (fileId.length === 0 || !root.uploadIsReady(result.uploadStatus, result.gcodeId)) {
                root.remotePrintSubmitting = false
                root.statusMsg = qsTr("The file was uploaded, but cloud processing is not ready for direct printing.")
                root.statusSev = "warn"
                return
            }
            var directData = {
                "fileId": fileId,
                "fileName": root.directPrintLocalName,
                "gcodeId": String(result.gcodeId || ""),
                "sizeBytes": 0,
                "status": "READY",
                "directMode": true,
                "deleteAfterPrint": root.optionDeleteAfterPrint === true,
                "deleteLocalOnFailure": root.directPrintDeleteLocalOnFailureSnapshot === true
            }
            root.setSingleRemotePrintFile(fileId, root.directPrintLocalName, directData)
            root.remotePrintSubmitting = false
            root.submitPrintOrder(directData)
        }

        function onPrintOrderFinished(printerId, fileId, result) {
            if (String(printerId || "") !== String(root.pendingPrintPrinterId || "")
                    || String(fileId || "") !== String(root.pendingPrintFileId || "")) {
                return
            }
            var fileData = root.pendingPrintFileData
            var deleteAfterPrint = root.pendingPrintDeleteAfterPrint
            root.remotePrintSubmitting = false
            root.pendingPrintPrinterId = ""
            root.pendingPrintFileId = ""
            root.pendingPrintFileData = null
            root.pendingPrintDeleteAfterPrint = false
            root.applyPrintOrderResult(printerId, fileId, result, fileData, deleteAfterPrint)
        }

        function onPrinterOrderFinished(context, printerId, orderId, result) {
            var commandContext = context || ({})
            var commandKind = String(commandContext.kind || "")
            if (commandKind === "localFilesPrepare") {
                if (result.ok !== true) {
                    statusMsg = qsTr("Printer file manager warmup failed: %1")
                            .arg(backendStatusDetail(result.message, qsTr("Warmup request failed.")))
                    statusSev = "warn"
                }
                return
            }

            if (commandKind === "localFilesList") {
                if (result.ok !== true) {
                    localFilesLoading = false
                    statusMsg = qsTr("Cannot request printer local files: %1")
                            .arg(backendStatusDetail(result.message, qsTr("List request failed.")))
                    statusSev = "error"
                }
                return
            }

            if (commandKind === "localFileStart") {
                loading = false
                root.applyLocalPrintCommandResult(String(printerId || ""), result)
                return
            }

            if (commandKind === "localFileDelete") {
                root.applyLocalFileDeleteResult(String(printerId || ""),
                                                String(commandContext.fileName || ""), result)
                return
            }

            if (commandKind === "resinFeedStart") {
                root.applyResinFeedStartResult(String(printerId || ""),
                                               Number(commandContext.feedType || 0),
                                               result)
                return
            }

            if (commandKind === "resinFeedStop") {
                root.applyResinFeedStopResult(String(printerId || ""),
                                              Number(commandContext.feedType || 0),
                                              result)
                return
            }
        }

        function onReasonCatalogUpdatedFromCloud(reasons, message) {
            var map = {}
            var list = reasons !== undefined ? reasons : []
            for (var i = 0; i < list.length; ++i) {
                var entry = list[i]
                map[String(entry.reason)] = entry
            }
            reasonCatalogByCode = map
            reasonCatalogLoaded = true
            reasonCatalogLoading = false
        }

        function onPrinterInsightsUpdatedFromCloud(printerId, details, projects, detailsRawJson, projectsRawJson, message) {
            if (String(printerId || "") !== String(root.selectedPrinterId || ""))
                return

            var resolvedDetails = details !== undefined ? details : ({})
            if (resolvedDetails !== null && Object.keys(resolvedDetails).length > 0)
                root.updateSelectedPrinterDetails(resolvedDetails)

            var list = projects !== undefined ? projects : []
            setLiveProjectFromList(list)
            mergeRecentJobsCard(list)

            if (detailsRawJson !== undefined)
                root.selectedPrinterDetailsRawJson = String(detailsRawJson || root.selectedPrinterDetailsRawJson)
            if (projectsRawJson !== undefined)
                root.selectedPrinterProjectsRawJson = String(projectsRawJson || root.selectedPrinterProjectsRawJson)

            root.loadingPrinterDetails = false
            root.loadingPrinterHistory = false
        }

        function onSyncFailed(scope, message) {
            var normalizedScope = String(scope || "")
            if (normalizedScope === "printers") {
                if (root.remotePrintPrinterBootstrapPending) {
                    root.remotePrintPrinterBootstrapPending = false
                    root.remotePrintPreparing = false
                    root.remotePrintAllowed = false
                    root.remotePrintPrepareMessage = ""
                    root.remotePrintBlockReason = qsTr("No printer available for remote print.")
                }
                statusMsg = qsTr("Background sync failed (printers): %1")
                        .arg(backendStatusDetail(message, qsTr("Retry later.")))
                statusSev = "warn"
                return
            }
            if (normalizedScope === "reason_catalog") {
                reasonCatalogLoading = false
                statusMsg = qsTr("Reason catalog unavailable: %1")
                        .arg(backendStatusDetail(message, qsTr("Catalog fetch failed.")))
                statusSev = "warn"
                return
            }
            if (normalizedScope === "printer_insights") {
                root.loadingPrinterDetails = false
                root.loadingPrinterHistory = false
                statusMsg = qsTr("Background sync failed (printer insights): %1")
                        .arg(backendStatusDetail(message, qsTr("Retry later.")))
                statusSev = "warn"
                return
            }
            if (normalizedScope === "files" && root.cloudFilesLoading) {
                root.cloudFilesLoading = false
                root.pendingCloudFilesPrinterId = ""
                statusMsg = qsTr("Background sync failed (files): %1")
                        .arg(backendStatusDetail(message, qsTr("Retry later.")))
                statusSev = "warn"
            }
        }
    }

    Connections {
        target: (typeof printWorkflowBridge !== "undefined" && printWorkflowBridge !== null)
                ? printWorkflowBridge : null
        ignoreUnknownSignals: true

        function onDirectPrintTrackingReleased(printerId) {
            root.clearPendingRemotePrint(printerId)
        }

        function onDirectCleanupNotice(noticeKind) {
            var kind = Number(noticeKind)
            if (kind === 1) {
                root.statusMsg = qsTr("Printer-local file deletion was not confirmed. The cloud file was kept.")
                root.statusSev = "warn"
                return
            }
            if (kind === 2) {
                root.statusMsg = qsTr("Direct print failed. The printer-local copy was deleted; the cloud file was kept.")
                root.statusSev = "warn"
                return
            }
            if (kind === 3) {
                root.statusMsg = qsTr("Printer-local file deletion failed. The cloud file was kept.")
                root.statusSev = "warn"
                return
            }
            if (kind === 4) {
                root.statusMsg = qsTr("Direct print failed. Cloud and printer-local files were kept.")
                root.statusSev = "warn"
                return
            }
            if (kind === 5) {
                root.statusMsg = qsTr("Direct print finished. Printer-local and cloud files were deleted.")
                root.statusSev = "success"
                return
            }
            if (kind === 6) {
                root.statusMsg = qsTr("Printer-local file deleted, but cloud deletion failed.")
                root.statusSev = "warn"
            }
        }

        function onRemotePrintPreparationReady(result) {
            root.applyRemotePrintPreparation(result)
        }

        function onRemotePrintTrackingReleased(printerId) {
            root.clearPendingRemotePrint(printerId)
        }

        function onRemoteCleanupNotice(noticeKind, printerId) {
            var kind = Number(noticeKind)
            if (kind === 1) {
                root.statusMsg = qsTr("Print finished, but local file deletion failed. Cloud deletion skipped.")
                root.statusSev = "warn"
                return
            }
            if (kind === 2) {
                root.statusMsg = qsTr("Print finished. Local file deleted, but cloud file id is missing.")
                root.statusSev = "warn"
                return
            }
            if (kind === 3) {
                root.statusMsg = qsTr("Print finished. Local file deleted, but cloud deletion failed.")
                root.statusSev = "warn"
                return
            }
            if (kind === 4) {
                root.statusMsg = qsTr("Print finished. File deleted locally and in cloud.")
                root.statusSev = "success"
            }
        }
    }

    Connections {
        target: (typeof mqttBridge !== "undefined" && mqttBridge !== null) ? mqttBridge : null
        ignoreUnknownSignals: true

        function onRealtimeEventTickChanged() {
            if (!root.pageActive) {
                root.mqttRealtimeRefreshPending = true
                return
            }
            if (mqttRealtimeDebounceTimer.running)
                mqttRealtimeDebounceTimer.restart()
            else
                mqttRealtimeDebounceTimer.start()
        }

        function onPrinterFileListReceived(printerId, source, records, state, code, message) {
            root.applyPrinterLocalFilesFromMqtt(printerId, source, records, state, code, message)
        }

        function onConnectedChanged() {
            if (!mqttBridge || mqttBridge.connected !== true)
                return
            var pendingPrinterId = String(root.pendingLocalFilesPrinterId || "").trim()
            if (pendingPrinterId.length <= 0)
                return
            root.pendingLocalFilesPrinterId = ""
            root.requestPrinterLocalFiles(pendingPrinterId)
        }
    }

    PrinterSelectCloudFileDialog {
        id: selectCloudFileDialog
        dialogTitle: qsTr("Select Cloud File")
        dialogSubtitle: qsTr("Compatible files for the selected printer")
        emptyText: root.cloudFilesLoading
                   ? qsTr("Loading compatible cloud files...")
                   : qsTr("No compatible cloud file for this printer.")
        startButtonText: qsTr("Start Printing")
        filesModel: printCloudFilesModel
        selectedFileId: root.selectedCloudFileId
        fileTypeProvider: root.fileType
        onSelectedFileChanged: function(fileId) {
            root.selectedCloudFileId = fileId
        }
        onCloseRequested: selectCloudFileDialog.close()
        onStartRequested: {
            selectCloudFileDialog.close()
            openRemotePrintConfig()
        }
    }

    PrinterSelectCloudFileDialog {
        id: selectLocalPrinterFileDialog
        objectName: "selectLocalPrinterFileDialog"
        dialogTitle: qsTr("Select Printer Local File")
        dialogSubtitle: qsTr("Files currently stored on the selected printer")
        emptyText: root.localFilesLoading
                   ? qsTr("Waiting for printer file list...")
                   : qsTr("No local file available on printer.")
        startButtonText: qsTr("Start Local Print")
        deleteEnabled: true
        deleteButtonText: qsTr("Delete")
        filesModel: printerLocalFilesModel
        selectedFileId: root.selectedPrinterLocalFileName
        fileTypeProvider: root.fileType
        onSelectedFileChanged: function(fileId) {
            root.selectedPrinterLocalFileName = fileId
        }
        onCloseRequested: selectLocalPrinterFileDialog.close()
        onDeleteRequested: function(fileId, fileName) {
            root.deletePrinterLocalFile(fileId.length > 0 ? fileId : fileName)
        }
        onStartRequested: root.startPrintFromPrinterLocalFile()
    }

    PrinterRemotePrintConfigDialog {
        id: remotePrintConfigDialog
        printersModel: printersModel
        compatiblePrintersModel: remoteCompatiblePrintersModel
        remotePrinterId: root.remotePrinterId
        selectedCloudFileId: root.selectedCloudFileId
        directPrintMode: root.remotePrintMode === "direct"
        selectedFileName: selectedCloudFileData() ? String(selectedCloudFileData().fileName || "-") : "-"
        selectedPrinterName: printerDataById(root.remotePrinterId)
                             ? String(printerDataById(root.remotePrinterId).name || "-")
                             : "-"
        selectedPrintTime: selectedCloudFileData() ? String(selectedCloudFileData().printTime || "-") : "-"
        selectedResinUsage: selectedCloudFileData() ? String(selectedCloudFileData().resinUsage || "-") : "-"
        remotePrintPreparing: root.remotePrintPreparing
        remotePrintPrepareMessage: root.remotePrintPrepareMessage
        optionDeleteAfterPrint: root.optionDeleteAfterPrint
        optionLiftCompensation: root.optionLiftCompensation
        optionAutoResinCheck: root.optionAutoResinCheck
        remotePrintAllowed: root.remotePrintAllowed
        remotePrintBlockReason: root.remotePrintBlockReason
        translateLocalizedTextProvider: root.translateLocalizedText
        onRemotePrinterChanged: function(printerId) {
            root.remotePrinterId = printerId
        }
        onOptionDeleteAfterPrintToggled: function(checked) {
            root.optionDeleteAfterPrint = checked
        }
        onOptionLiftCompensationToggled: function(checked) {
            root.optionLiftCompensation = checked
        }
        onOptionAutoResinCheckToggled: function(checked) {
            root.optionAutoResinCheck = checked
        }
        onRefreshGuardRequested: root.refreshRemotePrintGuard()
        onCloseRequested: {
            root.remotePrintPrinterBootstrapPending = false
            if (root.hasPrintWorkflowBridge()
                    && typeof printWorkflowBridge.cancelRemotePrintPreparation === "function")
                printWorkflowBridge.cancelRemotePrintPreparation()
            remotePrintConfigDialog.close()
        }
        onStartRequested: root.startRemotePrint()
    }

    PrinterMainPanel {
        embeddedInTabsContainer: root.embeddedInTabsContainer
        loading: root.loading
        debugUi: root.debugUi
        showDebugLabels: root.showDebugLabels
        localFilePrintEnabled: root.localFilePrintEnabled
        printersModel: printersModel
        selectedPrinterId: root.selectedPrinterId
        tabTitleProvider: root.printerTabTitle
        selectedPrinter: root.selectedPrinterLiveSnapshot
        selectedPrinterDetails: root.selectedPrinterDetails
        selectedLiveJobData: root.liveProjectData
        selectedPrinterDetailsRawJson: root.selectedPrinterDetailsRawJson
        selectedPrinterProjectsRawJson: root.selectedPrinterProjectsRawJson
        feedingOperationActive: root.resinFeedActive
        feedingOperationType: root.resinFeedType
        feedingStopInProgress: root.resinFeedStopSubmitting
        loadingPrinterHistory: root.loadingPrinterHistory
        printerHistoryModel: printerHistoryModel
        printersEndpointPath: root.printersEndpointPath
        printersEndpointRawJson: root.printersEndpointRawJson
        statusChipTextProvider: root.statusChipText
        progressTextProvider: root.progressText
        timeTextProvider: root.timeText
        unixTimeTextProvider: root.unixTimeText
        printStatusTextProvider: root.printStatusText
        prettyJsonProvider: root.prettyJson
        localizedTextProvider: root.translateLocalizedText
        onRefreshRequested: {
            if (!root.hasCloudBridge()) {
                root.loadPrinters()
                return
            }
            if (typeof cloudBridge.refreshPrintersAsync === "function") {
                root.statusMsg = qsTr("Force refresh printers from cloud...")
                root.statusSev = "info"
                cloudBridge.refreshPrintersAsync(true)
            } else {
                root.loadPrinters()
            }
        }
        onDebugToggled: function(checked) {
            root.showDebugLabels = checked
        }
        onPrinterSelected: function(printerId) {
            root.choosePrinter(printerId)
        }
        onCloudFileRequested: function(printerId) {
            root.startCloudFileRemotePrint(printerId)
        }
        onLocalFileRequested: function(printerId) {
            root.openLocalFileDialogForRemotePrint(printerId)
        }
        onResinFeedRequested: function(printerId, feedType) {
            root.startResinFeedOperation(printerId, feedType)
        }
        onResinFeedStopRequested: function(printerId, feedType) {
            root.stopResinFeedOperation(printerId, feedType)
        }
        onPrinterMqttDetailsRequested: function(printerId) {
            root.openPrinterDetailsDialog(printerId)
        }
    }

    AppDialogFrame {
        id: printerDetailsDialog
        objectName: "printerDetailsDialog"
        title: qsTr("Printer details")
        subtitle: qsTr("Current printer characteristics")
        minimumWidth: 980
        maximumWidth: 1280
        minimumHeight: 560
        maximumHeight: 980

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TextArea {
                objectName: "printerDetailsDialogText"
                readOnly: true
                text: root.prettyJson(root.selectedPrinterDetails || ({}))
                wrapMode: TextEdit.NoWrap
                color: Theme.fgPrimary
                font.family: "monospace"
                font.pixelSize: Theme.fontCaptionPx
                selectByMouse: true
                background: Rectangle {
                    color: "transparent"
                }
            }
        }
    }
}
