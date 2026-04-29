import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Item {
    id: root
    objectName: "printerPage"
    Layout.fillWidth: true
    Layout.fillHeight: true
    property bool embeddedInTabsContainer: false
    property bool deferStartupInitialization: false
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
    property bool localFilesLoading: false
    property bool cloudFilesLoading: false
    property int localFilesPrepareOrderId: 1231
    property int localFilesListOrderId: 103
    property int localUdiskListOrderId: 101
    property int localFileStartPrintOrderId: 999
    readonly property bool localFilePrintEnabled: localFileStartPrintOrderId !== 999
    property bool optionDeleteAfterPrint: false
    property bool optionLiftCompensation: false
    property bool optionAutoResinCheck: true
    property bool remotePrintAllowed: true
    property string remotePrintBlockReason: ""
    property bool remotePrintPreparing: false
    property string remotePrintPrepareMessage: ""
    property var remotePrintCompatibilityResult: null
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
    property string lastJobsRefreshReason: ""
    property string mqttDetailsTitle: ""
    property string mqttDetailsText: ""
    property int autoRefreshIntervalMs: 30000
    property int autoRefreshPrintingIntervalMs: 5000
    property int mqttRealtimeDebounceMs: 700
    readonly property var cloudFileKnownExtensions: [
        "photon", "pws", "pwsz", "photons", "pw0", "pwx", "pwmo", "pwma", "pwms",
        "pwmx", "pmx2", "pmsq", "dlp", "dl2p", "pwmb", "pm3", "pm3m",
        "pm3r", "pm3n", "px6s", "pm5", "pm5s", "m5sp"
    ]
    readonly property var cloudFileCompatibilityStopTokens: ({
        "anycubic": true,
        "photon": true,
        "mono": true,
        "printer": true,
        "printers": true,
        "series": true,
        "resin": true,
        "lcd": true
    })

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

    ListModel {
        id: printersModel
    }

    ListModel {
        id: remoteCompatiblePrintersModel
    }

    ListModel {
        id: printCloudFilesModel
    }

    ListModel {
        id: printerLocalFilesModel
    }

    ListModel {
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

    function hasCompatibilityEndpoint() {
        return hasCloudBridge() && typeof cloudBridge.fetchCompatiblePrintersByExt === "function"
    }

    function hasCompatibilityByFileIdEndpoint() {
        return hasCloudBridge() && typeof cloudBridge.fetchCompatiblePrintersByFileId === "function"
    }

    function hasPrinterOrderEndpoint() {
        return hasCloudBridge() && typeof cloudBridge.sendPrinterOrder === "function"
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

    function normalizedCompatText(value) {
        var text = String(value || "").toLowerCase().trim()
        if (text.length === 0)
            return ""
        text = text.replace(/[_\-./]+/g, " ")
        text = text.replace(/\s+/g, " ").trim()
        return text
    }

    function compatTokens(value) {
        var text = normalizedCompatText(value)
        if (text.length === 0)
            return []

        var rawTokens = text.split(" ")
        var out = []
        var seen = {}
        for (var i = 0; i < rawTokens.length; ++i) {
            var token = String(rawTokens[i] || "").trim()
            if (token.length <= 1)
                continue
            if (cloudFileCompatibilityStopTokens[token] !== undefined)
                continue
            if (seen[token] === true)
                continue
            seen[token] = true
            out.push(token)
        }
        return out
    }

    function compatTokenOverlapCount(tokensA, tokensB) {
        var lookup = {}
        for (var i = 0; i < tokensA.length; ++i)
            lookup[String(tokensA[i])] = true

        var count = 0
        for (var j = 0; j < tokensB.length; ++j) {
            var token = String(tokensB[j] || "")
            if (lookup[token] === true)
                count += 1
        }
        return count
    }

    function compatTextContains(haystack, needle) {
        var h = normalizedCompatText(haystack)
        var n = normalizedCompatText(needle)
        if (h.length === 0 || n.length === 0)
            return false
        return h.indexOf(n) !== -1
    }

    function isKnownCloudSliceExtension(ext) {
        var normalizedExt = String(ext || "").toLowerCase()
        if (normalizedExt.length === 0 || normalizedExt === "other")
            return false
        for (var i = 0; i < cloudFileKnownExtensions.length; ++i) {
            if (String(cloudFileKnownExtensions[i]) === normalizedExt)
                return true
        }
        return false
    }

    function fileHasLocalCompatibilityMetadata(fileEntry) {
        if (!fileEntry)
            return false

        var machineText = normalizedCompatText(fileEntry.machine)
        var printersText = normalizedCompatText(fileEntry.printers)
        var fileMachineType = normalizedCompatText(fileEntry.machineType)
        if (fileMachineType.length === 0)
            fileMachineType = normalizedCompatText(fileEntry.machineTypeId)

        return machineText.length > 0 || printersText.length > 0 || fileMachineType.length > 0
    }

    function localCompatibilityForPrinterFile(fileEntry, printerId) {
        var printer = printerDataById(printerId)
        if (!printer)
            return { "ok": false, "score": 0, "reason": qsTr("Select a printer first.") }
        if (!fileEntry)
            return { "ok": false, "score": 0, "reason": qsTr("Select a cloud file first.") }

        var ext = fileType(fileEntry.fileName)
        if (!isKnownCloudSliceExtension(ext))
            return { "ok": false, "score": 0, "reason": qsTr("Unsupported file format.") }

        if (hasLocalCompatibilityEvaluator()) {
            var bridgeResult = cloudBridge.evaluateLocalPrinterFileCompatibility(printer, fileEntry)
            if (bridgeResult && bridgeResult.ok !== undefined) {
                return {
                    "ok": bridgeResult.ok === true,
                    "score": Number(bridgeResult.score || 0),
                    "reason": String(bridgeResult.reason || "")
                }
            }
        }

        var printerMachineType = normalizedCompatText(printer.machineType)
        var fileMachineType = normalizedCompatText(fileEntry.machineType)
        if (fileMachineType.length === 0)
            fileMachineType = normalizedCompatText(fileEntry.machineTypeId)

        if (fileMachineType.length > 0 && printerMachineType.length > 0) {
            if (fileMachineType === printerMachineType)
                return { "ok": true, "score": 500, "reason": "" }
            return { "ok": false, "score": 0, "reason": qsTr("Slice file does not match machine type.") }
        }

        var machineText = normalizedCompatText(fileEntry.machine)
        var printersText = normalizedCompatText(fileEntry.printers)
        var metadataText = (machineText + " " + printersText).trim()
        if (metadataText.length <= 0)
            return { "ok": false, "score": 0, "reason": qsTr("Missing local compatibility metadata.") }

        var printerModel = normalizedCompatText(printer.model)
        var printerName = normalizedCompatText(printer.name)

        if (machineText.length > 0 && (machineText === printerModel || machineText === printerName))
            return { "ok": true, "score": 420, "reason": "" }
        if (compatTextContains(machineText, printerModel)
                || compatTextContains(printerModel, machineText)
                || compatTextContains(machineText, printerName)
                || compatTextContains(printerName, machineText)) {
            return { "ok": true, "score": 360, "reason": "" }
        }
        if (compatTextContains(printersText, printerModel)
                || compatTextContains(printersText, printerName)
                || compatTextContains(printerModel, printersText)
                || compatTextContains(printerName, printersText)) {
            return { "ok": true, "score": 330, "reason": "" }
        }

        var metadataTokens = compatTokens(metadataText)
        var printerTokens = compatTokens(printerModel + " " + printerName + " " + printerMachineType)
        var overlapCount = compatTokenOverlapCount(metadataTokens, printerTokens)
        if (overlapCount > 0)
            return { "ok": true, "score": 280 + overlapCount, "reason": "" }

        return { "ok": false, "score": 0, "reason": qsTr("Slice file does not match selected printer model.") }
    }

    function uploadInputToDisplayName(fileInput) {
        var raw = String(fileInput || "").trim()
        if (raw.length === 0)
            return ""

        var stripped = raw.replace(/^file:\/\/localhost/i, "file://")
        if (stripped.indexOf("file://") === 0)
            stripped = stripped.replace(/^file:\/\//i, "")

        var tail = stripped.split("/").pop()
        if (tail.length === 0)
            tail = stripped
        tail = tail.replace(/[?#].*$/, "")
        try {
            return decodeURIComponent(tail)
        } catch (err) {
            return tail
        }
    }

    function uploadIsReady(uploadStatus, gcodeId) {
        return Number(uploadStatus) === 1 || String(gcodeId || "").trim().length > 0
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

    function progressRatio(progress) {
        var value = Number(progress)
        if (!isFinite(value) || value < 0)
            return 0
        return Math.max(0, Math.min(100, value)) / 100.0
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

    function printerSecondaryText(printer) {
        var parts = []
        var modelText = String(printer && printer.model !== undefined ? printer.model : "").trim()
        var typeText = String(printer && printer.type !== undefined ? printer.type : "").trim()
        var lastSeenText = String(printer && printer.lastSeen !== undefined ? printer.lastSeen : "").trim()

        if (modelText.length > 0)
            parts.push(modelText)
        if (typeText.length > 0)
            parts.push(typeText)
        if (lastSeenText.length > 0)
            parts.push(qsTr("Last seen: %1").arg(lastSeenText))

        return parts.length > 0 ? parts.join(" \u00b7 ") : "-"
    }

    function printerTabTitle(printer) {
        var name = String(printer && printer.name !== undefined ? printer.name : "-")
        var status = statusChipText(printer ? printer.state : "READY")
        return name + " | " + status
    }

    function selectedPrinterIndex() {
        if (selectedPrinterId.length === 0)
            return -1
        for (var i = 0; i < printersModel.count; ++i) {
            if (String(printersModel.get(i).id) === selectedPrinterId)
                return i
        }
        return -1
    }

    function normalizedCompatReason(reasonText) {
        var text = String(reasonText || "").trim()
        if (text.length === 0)
            return ""
        var prefix = qsTr("unavailable reason:")
        if (text.toLowerCase().indexOf(prefix) === 0)
            text = text.slice(prefix.length).trim()
        return text
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

    function evaluateRemotePrintGuard() {
        var printer = null
        for (var i = 0; i < printersModel.count; ++i) {
            var candidate = printersModel.get(i)
            if (String(candidate.id) === String(remotePrinterId)) {
                printer = candidate
                break
            }
        }

        var stateCheck = canStartFromPrinterState(printer)
        if (stateCheck.ok !== true)
            return stateCheck

        if (selectedCloudFileId.length === 0)
            return { "ok": false, "reason": qsTr("Select a cloud file first.") }

        var fileData = selectedCloudFileData()
        if (!fileData)
            return { "ok": true, "reason": "" }
        if (!fileHasLocalCompatibilityMetadata(fileData))
            return { "ok": true, "reason": "" }

        var localCompat = localCompatibilityForPrinterFile(fileData, remotePrinterId)
        if (localCompat.ok === true)
            return { "ok": true, "reason": "" }
        return {
            "ok": false,
            "reason": String(localCompat.reason || qsTr("Printer is not compatible with this file."))
        }
    }

    function refreshRemotePrintGuard() {
        var result = evaluateRemotePrintGuard()
        remotePrintAllowed = (result.ok === true)
        remotePrintBlockReason = translateLocalizedText(String(result.reason || ""))
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

    function findCloudFileInListById(fileList, fileId) {
        var normalizedFileId = String(fileId || "").trim()
        var list = fileList !== undefined && fileList !== null ? fileList : []
        for (var i = 0; i < list.length; ++i) {
            var candidate = list[i]
            if (cloudFileIdValue(candidate) === normalizedFileId)
                return candidate
        }
        return null
    }

    function loadCloudFileDetailsById(fileId) {
        var normalizedFileId = String(fileId || "").trim()
        if (normalizedFileId.length === 0 || !hasCloudBridge())
            return null

        if (typeof cloudBridge.loadCachedFiles === "function") {
            var cached = cloudBridge.loadCachedFiles(1, 200)
            if (cached.ok === true) {
                var cachedList = cached.files !== undefined ? cached.files : []
                var cachedMatch = findCloudFileInListById(cachedList, normalizedFileId)
                if (cachedMatch)
                    return cachedMatch
            }
        }

        if (typeof cloudBridge.fetchFiles === "function") {
            var listing = cloudBridge.fetchFiles(1, 200)
            if (listing.ok === true) {
                var files = listing.files !== undefined ? listing.files : []
                var fetchedMatch = findCloudFileInListById(files, normalizedFileId)
                if (fetchedMatch)
                    return fetchedMatch
            }
        }

        return null
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

    function buildPrinterMqttDetails(printerId) {
        var printer = printerDataById(printerId)
        var printerName = printer && String(printer.name || "").length > 0
                ? String(printer.name)
                : String(printerId || "-")
        mqttDetailsTitle = qsTr("MQTT details: %1").arg(printerName)

        if (typeof mqttBridge === "undefined" || mqttBridge === null) {
            mqttDetailsText = qsTr("MQTT bridge unavailable.")
            return
        }

        var key = printer && String(printer.printerKey || "").trim().length > 0
                ? String(printer.printerKey).trim()
                : String(printerId || "").trim()
        if (key.length === 0) {
            mqttDetailsText = qsTr("Missing printer identifier.")
            return
        }

        var topics = mqttBridge.receivedTopics || []
        var matched = []
        for (var i = 0; i < topics.length; ++i) {
            var topic = String(topics[i] || "")
            if (topic.indexOf("/" + key + "/") !== -1 || topic.indexOf("/" + String(printerId || "") + "/") !== -1)
                matched.push(topic)
        }

        if (matched.length === 0) {
            mqttDetailsText = qsTr("No MQTT message captured yet for this printer.")
            return
        }

        var blocks = []
        for (var j = 0; j < matched.length; ++j) {
            var t = matched[j]
            var body = String(mqttBridge.messagesForTopic(t) || "").trim()
            if (body.length > 0)
                blocks.push("### " + t + "\n" + body)
        }
        mqttDetailsText = blocks.length > 0
                ? blocks.join("\n\n")
                : qsTr("No MQTT payload available for matched topics.")
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
        var active = firstActiveProject(projectsList)
        if (active) {
            clearPendingRemotePrint(String(active.printerId || selectedPrinterId || ""))
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

    function replaceRecentJobsCard(projectsList) {
        printerHistoryModel.clear()
        var list = projectsList !== undefined && projectsList !== null ? projectsList : []
        for (var i = 0; i < list.length; ++i)
            printerHistoryModel.append(list[i])
    }

    function mergeRecentJobsCard(projectsList) {
        var byTaskId = {}
        var merged = []
        var order = 0

        function copyJob(source) {
            var out = {}
            for (var key in source)
                out[key] = source[key]
            return out
        }

        function pushOrUpdate(job) {
            var item = copyJob(job)
            var taskId = String(item.taskId || "").trim()
            if (taskId.length > 0 && byTaskId[taskId] !== undefined) {
                var existing = byTaskId[taskId]
                var previousOrder = existing.__mergeOrder
                for (var key in item)
                    existing[key] = item[key]
                existing.__mergeOrder = previousOrder
                return
            }

            item.__mergeOrder = order++
            merged.push(item)
            if (taskId.length > 0)
                byTaskId[taskId] = item
        }

        for (var i = 0; i < printerHistoryModel.count; ++i)
            pushOrUpdate(printerHistoryModel.get(i))

        var list = projectsList !== undefined && projectsList !== null ? projectsList : []
        for (var j = 0; j < list.length; ++j)
            pushOrUpdate(list[j])

        merged.sort(function(a, b) {
            var at = Number(a.createTime)
            var bt = Number(b.createTime)
            var aValid = isFinite(at) && at > 0
            var bValid = isFinite(bt) && bt > 0
            if (aValid && bValid && at !== bt)
                return bt - at
            if (aValid !== bValid)
                return aValid ? -1 : 1
            return Number(a.__mergeOrder) - Number(b.__mergeOrder)
        })

        printerHistoryModel.clear()
        for (var k = 0; k < merged.length; ++k) {
            delete merged[k].__mergeOrder
            printerHistoryModel.append(merged[k])
        }
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
            "img": ""
        }
        pendingRemotePrintByPrinterId = next
        liveProjectData = next[key]
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
        printersAutoRefreshTimer.running = startupInitialized && printersModel.count > 0
    }

    function refreshSelectedPrinterLiveSnapshot() {
        selectedPrinterLiveSnapshot = selectedPrinterLiveData()
    }

    function syncSelectedPrinterDetailsFromModel() {
        var selected = selectedPrinterData()
        if (!selected)
            return
        if (selected.details !== undefined)
            selectedPrinterDetails = selected.details
        if (selected.detailsRawJson !== undefined)
            selectedPrinterDetailsRawJson = String(selected.detailsRawJson || "")
        if (selected.projectsRawJson !== undefined)
            selectedPrinterProjectsRawJson = String(selected.projectsRawJson || "")
    }

    function refreshPrintersFromTimer() {
        if (!startupInitialized)
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
                selectedPrinterDetails = selected.details
            if (selected.detailsRawJson !== undefined)
                selectedPrinterDetailsRawJson = String(selected.detailsRawJson || "")
            if (selected.projectsRawJson !== undefined)
                selectedPrinterProjectsRawJson = String(selected.projectsRawJson || "")
        }

        if (!hasCloudBridge())
            return

        if (typeof cloudBridge.loadCachedPrinterProjects === "function") {
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
                        selectedPrinterDetails = fetchedDetails
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
        printersModel.clear()
        printersModel.append({
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
        })
        printersModel.append({
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
        })
        printersModel.append({
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
        })

        if (selectedPrinterId.length === 0 && printersModel.count > 0)
            selectedPrinterId = String(printersModel.get(0).id)

        rebuildRemoteCompatiblePrintersModel()

        statusMsg = qsTr("Demo mode (backend unavailable).")
        statusSev = "warn"
        loading = false
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
        var r = useCacheFlow ? cloudBridge.loadCachedPrinters() : cloudBridge.fetchPrinters()
        loading = false
        printersEndpointPath = String(r.endpoint || printersEndpointPath)
        printersEndpointRawJson = String(r.rawJson || "")

        var printers = r.printers !== undefined ? r.printers : []
        replacePrintersModel(printers, false)
        if (!startupJobsRefreshed) {
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

    function ensureStartupInitialized() {
        if (startupInitialized)
            return
        startupInitialized = true
        ensureReasonCatalogLoaded()
        loadPrinters()
        updatePrintersAutoRefreshInterval()
    }

    function replacePrintersModel(printers, refreshInsights) {
        printersModel.clear()
        var list = printers !== undefined ? printers : []
        for (var i = 0; i < list.length; ++i)
            printersModel.append(list[i])

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

        detectPrintCompletionTransitions()
        var shouldRefreshInsights = refreshInsights === true
        if (shouldRefreshInsights)
            refreshSelectedPrinterJobs("explicit", true, true)
        else
            syncSelectedPrinterDetailsFromModel()
        rebuildRemoteCompatiblePrintersModel()
        refreshSelectedPrinterLiveSnapshot()
        updatePrintersAutoRefreshInterval()
    }

    function detectPrintCompletionTransitions() {
        var nextState = {}
        var finishedSelectedPrinter = false
        for (var i = 0; i < printersModel.count; ++i) {
            var printer = printersModel.get(i)
            var printerId = String(printer.id || "")
            if (printerId.length === 0)
                continue
            var active = printerHasActiveJob(printer)
            var hadActive = printerHadActiveJobById[printerId] === true
            nextState[printerId] = active
            if (hadActive && !active && printerId === selectedPrinterId)
                finishedSelectedPrinter = true
        }
        printerHadActiveJobById = nextState
        if (finishedSelectedPrinter)
            refreshSelectedPrinterJobs("print_finished", true, false)
    }

    function refreshPrintersFromCacheOnly() {
        if (!hasCloudBridge() || typeof cloudBridge.loadCachedPrinters !== "function")
            return
        var r = cloudBridge.loadCachedPrinters()
        if (r.ok !== true)
            return
        printersEndpointPath = String(r.endpoint || printersEndpointPath)
        printersEndpointRawJson = String(r.rawJson || "")
        var list = r.printers !== undefined ? r.printers : []
        replacePrintersModel(list, false)
    }

    function compatibilityAllowsPrinter(compatResult, printerId) {
        if (compatResult === null || compatResult === undefined)
            return true
        if (compatResult.ok !== true)
            return true

        var list = compatResult.printers !== undefined ? compatResult.printers : []
        for (var i = 0; i < list.length; ++i) {
            var item = list[i]
            if (String(item.id) !== String(printerId))
                continue
            var available = Number(item.available)
            return isFinite(available) ? available > 0 : true
        }
        return false
    }

    function printerExists(printerId) {
        var normalizedId = String(printerId || "")
        if (normalizedId.length === 0)
            return false
        for (var i = 0; i < printersModel.count; ++i) {
            if (String(printersModel.get(i).id) === normalizedId)
                return true
        }
        return false
    }

    function firstPrinterId() {
        if (printersModel.count <= 0)
            return ""
        return String(printersModel.get(0).id || "")
    }

    function appendRemoteCompatiblePrinter(printer) {
        if (!printer)
            return
        var printerId = String(printer.id || "").trim()
        if (printerId.length === 0)
            return
        remoteCompatiblePrintersModel.append({
            "id": printerId,
            "name": String(printer.name || printerId),
            "model": String(printer.model || ""),
            "state": String(printer.state || "READY"),
            "machineType": String(printer.machineType || "")
        })
    }

    function compatibilityEntryAllows(item) {
        var available = Number(item && item.available !== undefined ? item.available : 1)
        return isFinite(available) ? available > 0 : true
    }

    function compatibilityFailureReason(compatResult, fallbackMessage) {
        if (compatResult === null || compatResult === undefined || compatResult.ok !== true)
            return translateLocalizedText(fallbackMessage)
        var list = compatResult.printers !== undefined ? compatResult.printers : []
        if (list.length <= 0)
            return translateLocalizedText(fallbackMessage)
        for (var i = 0; i < list.length; ++i) {
            var item = list[i]
            if (compatibilityEntryAllows(item))
                continue
            var reason = normalizedCompatReason(item.reason)
            if (reason.length > 0)
                return translateLocalizedText(reason)
        }
        return translateLocalizedText(fallbackMessage)
    }

    function preferredCompatiblePrinterId(compatResult, preferredPrinterId) {
        if (compatResult === null || compatResult === undefined || compatResult.ok !== true)
            return ""

        var preferredId = String(preferredPrinterId || "")
        var fallbackId = ""
        var list = compatResult.printers !== undefined ? compatResult.printers : []
        for (var i = 0; i < list.length; ++i) {
            var item = list[i]
            var candidateId = String(item.id || "")
            if (candidateId.length === 0 || !printerExists(candidateId))
                continue
            if (!compatibilityEntryAllows(item))
                continue
            if (preferredId.length > 0 && candidateId === preferredId)
                return candidateId
            if (fallbackId.length === 0)
                fallbackId = candidateId
        }
        return fallbackId
    }

    function rebuildRemoteCompatiblePrintersModel() {
        remoteCompatiblePrintersModel.clear()

        var fileData = selectedCloudFileData()
        var hasFileData = fileData !== null && fileData !== undefined
        var hasLocalCompatibilityMetadata = fileHasLocalCompatibilityMetadata(fileData)
        var serverCompatByPrinterId = {}
        var hasServerCompatibility = false

        if (hasFileData && remotePrintCompatibilityResult !== null && remotePrintCompatibilityResult !== undefined) {
            if (remotePrintCompatibilityResult.ok === true) {
                var cachedCompatPrinters = remotePrintCompatibilityResult.printers !== undefined ? remotePrintCompatibilityResult.printers : []
                for (var cachedCp = 0; cachedCp < cachedCompatPrinters.length; ++cachedCp) {
                    var cachedCompatEntry = cachedCompatPrinters[cachedCp]
                    var cachedCompatPrinterId = String(cachedCompatEntry && cachedCompatEntry.id !== undefined ? cachedCompatEntry.id : "")
                    if (cachedCompatPrinterId.length <= 0)
                        continue
                    if (!compatibilityEntryAllows(cachedCompatEntry))
                        continue
                    serverCompatByPrinterId[cachedCompatPrinterId] = true
                }
                hasServerCompatibility = true
            }
        } else if (hasFileData && hasCompatibilityByFileIdEndpoint()) {
            var fileId = String(fileData.fileId || "").trim()
            if (fileId.length > 0) {
                var compatByFile = cloudBridge.fetchCompatiblePrintersByFileId(fileId)
                if (compatByFile.ok === true) {
                    var compatPrinters = compatByFile.printers !== undefined ? compatByFile.printers : []
                    for (var cp = 0; cp < compatPrinters.length; ++cp) {
                        var compatEntry = compatPrinters[cp]
                        var compatPrinterId = String(compatEntry && compatEntry.id !== undefined ? compatEntry.id : "")
                        if (compatPrinterId.length <= 0)
                            continue
                        if (!compatibilityEntryAllows(compatEntry))
                            continue
                        serverCompatByPrinterId[compatPrinterId] = true
                    }
                    hasServerCompatibility = true
                }
            }
        }

        var shouldFilterByCompatibility = hasFileData
                && (hasServerCompatibility || hasLocalCompatibilityMetadata)

        for (var i = 0; i < printersModel.count; ++i) {
            var printer = printersModel.get(i)
            var printerId = String(printer && printer.id !== undefined ? printer.id : "")
            if (printerId.length === 0)
                continue
            if (shouldFilterByCompatibility) {
                if (hasServerCompatibility) {
                    if (serverCompatByPrinterId[printerId] !== true)
                        continue
                } else {
                    var localCompat = localCompatibilityForPrinterFile(fileData, printerId)
                    if (localCompat.ok !== true)
                        continue
                }
            }
            appendRemoteCompatiblePrinter(printer)
        }

        if (remoteCompatiblePrintersModel.count <= 0) {
            if (shouldFilterByCompatibility)
                remotePrinterId = ""
            return
        }

        var foundCurrent = false
        for (var j = 0; j < remoteCompatiblePrintersModel.count; ++j) {
            if (String(remoteCompatiblePrintersModel.get(j).id) === remotePrinterId) {
                foundCurrent = true
                break
            }
        }

        if (!foundCurrent)
            remotePrinterId = String(remoteCompatiblePrintersModel.get(0).id || "")
    }

    function setSingleRemotePrintFile(fileId, fileName) {
        var normalizedFileId = String(fileId || "").trim()
        if (normalizedFileId.length === 0)
            return

        var normalizedFileName = String(fileName || "").trim()
        var existing = cloudFileDataById(normalizedFileId)
        if (!existing)
            existing = loadCloudFileDetailsById(normalizedFileId)

        var entry = existing ? existing : ({})
        entry.fileId = normalizedFileId
        entry.fileName = normalizedFileName.length > 0
                ? normalizedFileName
                : String(entry.fileName || qsTr("Cloud file"))
        entry.sizeText = String(entry.sizeText || "-")
        entry.status = String(entry.status || "READY")
        entry.printTime = cloudFilePrintTimeText(entry)
        entry.resinUsage = cloudFileResinUsageText(entry)

        printCloudFilesModel.clear()
        printCloudFilesModel.append(entry)
        selectedCloudFileId = normalizedFileId
    }

    function openRemotePrintFromFile(fileId, fileName) {
        var normalizedFileId = String(fileId || "").trim()
        var normalizedFileName = String(fileName || "").trim()
        if (normalizedFileId.length === 0) {
            statusMsg = qsTr("Cannot start remote print: missing file id.")
            statusSev = "warn"
            return
        }

        setSingleRemotePrintFile(normalizedFileId, normalizedFileName)
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

    function prepareRemotePrintDialog(fileId, fileName) {
        var normalizedFileId = String(fileId || "").trim()
        var normalizedFileName = String(fileName || "").trim()

        if (printersModel.count <= 0)
            loadPrinters()
        if (printersModel.count <= 0) {
            remotePrintPreparing = false
            remotePrintAllowed = false
            remotePrintPrepareMessage = ""
            remotePrintBlockReason = qsTr("No printer available for remote print.")
            statusMsg = remotePrintBlockReason
            statusSev = "warn"
            return
        }

        var preferredPrinterId = selectedPrinterId.length > 0 ? selectedPrinterId : firstPrinterId()
        var targetPrinterId = preferredPrinterId
        var compatResult = null
        var compatibilityChecked = false
        var compatibilityFailed = false

        if (hasCompatibilityByFileIdEndpoint()) {
            compatibilityChecked = true
            var fileCompat = cloudBridge.fetchCompatiblePrintersByFileId(normalizedFileId)
            if (fileCompat.ok === true) {
                compatResult = fileCompat
                remotePrintCompatibilityResult = fileCompat
                var byFilePrinterId = preferredCompatiblePrinterId(fileCompat, preferredPrinterId)
                if (byFilePrinterId.length > 0)
                    targetPrinterId = byFilePrinterId
                else {
                    remotePrintPreparing = false
                    remotePrintAllowed = false
                    remotePrintPrepareMessage = ""
                    remotePrintBlockReason = compatibilityFailureReason(fileCompat,
                                                                        qsTr("No compatible printer available for this file."))
                    statusMsg = qsTr("Print blocked: %1").arg(remotePrintBlockReason)
                    statusSev = "warn"
                    return
                }
            } else {
                compatibilityFailed = true
            }
        }

        if ((compatResult === null || compatResult === undefined) && hasCompatibilityEndpoint()) {
            var ext = fileType(normalizedFileName)
            if (ext !== "other" && ext.length > 0) {
                compatibilityChecked = true
                var extCompat = cloudBridge.fetchCompatiblePrintersByExt(ext)
                if (extCompat.ok === true) {
                    compatResult = extCompat
                    remotePrintCompatibilityResult = extCompat
                    var byExtPrinterId = preferredCompatiblePrinterId(extCompat, preferredPrinterId)
                    if (byExtPrinterId.length > 0)
                        targetPrinterId = byExtPrinterId
                    else {
                        remotePrintPreparing = false
                        remotePrintAllowed = false
                        remotePrintPrepareMessage = ""
                        remotePrintBlockReason = compatibilityFailureReason(extCompat,
                                                                            qsTr("No compatible printer available for this file type."))
                        statusMsg = qsTr("Print blocked: %1").arg(remotePrintBlockReason)
                        statusSev = "warn"
                        return
                    }
                } else {
                    compatibilityFailed = true
                }
            }
        }

        if (targetPrinterId.length === 0)
            targetPrinterId = firstPrinterId()
        if (targetPrinterId.length === 0) {
            remotePrintPreparing = false
            remotePrintAllowed = false
            remotePrintPrepareMessage = ""
            remotePrintBlockReason = qsTr("No printer available for remote print.")
            statusMsg = remotePrintBlockReason
            statusSev = "warn"
            return
        }

        choosePrinter(targetPrinterId)
        remotePrinterId = targetPrinterId
        rebuildRemoteCompatiblePrintersModel()
        refreshRemotePrintGuard()
        remotePrintPreparing = false
        remotePrintPrepareMessage = ""

        if (compatibilityChecked && compatibilityFailed && (compatResult === null || compatResult === undefined)) {
            statusMsg = qsTr("Compatibility check failed. Continuing with best-effort printer selection.")
            statusSev = "warn"
            return
        }

        statusMsg = qsTr("Remote print prepared for %1")
                .arg(normalizedFileName.length > 0 ? normalizedFileName : normalizedFileId)
        statusSev = "info"
    }

    function loadCloudFilesForRemotePrint(printerId) {
        var targetPrinterId = String(printerId || "").trim()
        cloudFilesLoading = true
        printCloudFilesModel.clear()
        selectedCloudFileId = ""

        if (targetPrinterId.length === 0) {
            statusMsg = qsTr("No printer selected for cloud file filtering.")
            statusSev = "warn"
            cloudFilesLoading = false
            return
        }

        var files = []
        var loadedFromLocalCache = false
        if (hasCloudBridge()) {
            if (typeof cloudBridge.loadCachedFiles === "function") {
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

        var compatibleRows = []
        var hiddenIncompatibleCount = 0
        var hiddenMissingMetadataCount = 0

        for (var i = 0; i < files.length; ++i) {
            var file = files[i]
            var localCompat = localCompatibilityForPrinterFile(file, targetPrinterId)
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

        for (var rowIndex = 0; rowIndex < compatibleRows.length; ++rowIndex) {
            var row = compatibleRows[rowIndex]
            if (row.file !== undefined)
                printCloudFilesModel.append(row.file)
        }

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
            return
        }

        if (files.length <= 0) {
            statusMsg = loadedFromLocalCache
                    ? qsTr("No local cloud cache yet.")
                    : qsTr("No cloud file available.")
            statusSev = "warn"
            cloudFilesLoading = false
            return
        }
        if (hiddenMissingMetadataCount > 0 && hiddenMissingMetadataCount === files.length) {
            statusMsg = qsTr("No compatible cloud file: local metadata missing for all files.")
            statusSev = "warn"
            cloudFilesLoading = false
            return
        }
        statusMsg = qsTr("No compatible cloud file available for this printer.")
        statusSev = "warn"
        cloudFilesLoading = false
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
        if (!localFilePrintEnabled) {
            statusMsg = qsTr("Printer local file printing is disabled until the start order id is confirmed.")
            statusSev = "warn"
            return
        }
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
            localFilesLoading = false
            statusMsg = qsTr("MQTT must be connected to receive printer local file list.")
            statusSev = "warn"
            return
        }

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

        statusMsg = qsTr("Loading local files from printer...")
        statusSev = "info"
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
        printerLocalFilesModel.clear()
        selectedPrinterLocalFileName = ""

        var list = records !== undefined ? records : []
        for (var i = 0; i < list.length; ++i) {
            var record = list[i]
            var fileName = String(record.filename || "").trim()
            if (fileName.length === 0)
                continue
            var isDir = record.isDir === true || Number(record.isDir) > 0
            if (isDir)
                continue

            var sizeValue = Number(record.size)
            if (!isFinite(sizeValue) || sizeValue < 0)
                sizeValue = 0
            var timestampValue = Number(record.timestamp)
            if (!isFinite(timestampValue) || timestampValue < 0)
                timestampValue = 0

            printerLocalFilesModel.append({
                "fileId": fileName,
                "fileName": fileName,
                "sizeText": bytesText(sizeValue),
                "status": qsTr("On printer"),
                "printTime": timestampValue > 0 ? unixTimeText(timestampValue) : "-"
            })
        }

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
        if (!localFilePrintEnabled) {
            statusMsg = qsTr("Printer local file printing is disabled until the start order id is confirmed.")
            statusSev = "warn"
            return
        }
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
        statusMsg = qsTr("Sending local print command for %1...")
                .arg(selectedFileName)
        statusSev = "info"

        var uploadRes = cloudBridge.sendPrinterOrder(targetPrinterId,
                                                     localFileStartPrintOrderId,
                                                     {
                                                         "filename": selectedFileName,
                                                         "path": "/"
                                                     },
                                                     targetPrinterId)
        loading = false
        if (uploadRes.ok !== true) {
            statusMsg = qsTr("Local print command failed: %1")
                    .arg(backendStatusDetail(uploadRes.message, qsTr("Command rejected.")))
            statusSev = "error"
            return
        }

        var msgId = String(uploadRes.msgId || "").trim()
        if (localFileStartPrintOrderId === 999) {
            statusMsg = qsTr("Local print command sent with placeholder order_id=999 (msgid=%1).")
                    .arg(msgId.length > 0 ? msgId : "-")
            statusSev = "warn"
        } else {
            statusMsg = qsTr("Local print command sent (order_id=%1, msgid=%2).")
                    .arg(String(localFileStartPrintOrderId))
                    .arg(msgId.length > 0 ? msgId : "-")
            statusSev = "success"
        }
        selectLocalPrinterFileDialog.close()
        if (targetPrinterId === selectedPrinterId)
            refreshSelectedPrinterJobs("print_started", true, false)
        loadPrinters()
    }

    function openRemotePrintConfig() {
        if (!ensureSelectedCloudFile())
            return

        optionDeleteAfterPrint = false
        if (!remotePrintPreparing) {
            rebuildRemoteCompatiblePrintersModel()
            remotePrintAllowed = true
            remotePrintBlockReason = ""
            refreshRemotePrintGuard()
        }
        remotePrintConfigDialog.open()
    }

    function startRemotePrint() {
        if (remotePrintPreparing) {
            statusMsg = qsTr("Remote print setup is still loading.")
            statusSev = "warn"
            return
        }

        if (remotePrinterId.length === 0) {
            statusMsg = qsTr("Select a printer first.")
            statusSev = "warn"
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

        refreshRemotePrintGuard()
        if (!remotePrintAllowed) {
            statusMsg = remotePrintBlockReason.length > 0
                    ? (qsTr("Print blocked: %1").arg(remotePrintBlockReason))
                    : qsTr("Print blocked by compatibility checks.")
            statusSev = "warn"
            return
        }

        if (!hasCloudBridge()) {
            statusMsg = qsTr("Demo: remote print payload prepared for %1")
                    .arg(String(fileData.fileName || ""))
            statusSev = "warn"
            remotePrintConfigDialog.close()
            return
        }

        var r = cloudBridge.sendPrintOrder(remotePrinterId,
                                           cloudFileIdValue(fileData),
                                           optionDeleteAfterPrint,
                                           false)
        if (r.ok === true) {
            var taskId = String(r.taskId || "")
            var successMessage = taskId.length > 0
                    ? (qsTr("Print order sent (task_id=%1)").arg(taskId))
                    : qsTr("Print order sent.")
            markRemotePrintAccepted(remotePrinterId, fileData, taskId)
            remotePrintConfigDialog.close()
            if (remotePrinterId !== selectedPrinterId)
                choosePrinter(remotePrinterId)
            else
                refreshSelectedPrinterLiveSnapshot()
            refreshSelectedPrinterJobs("print_started", true, false)
            loadPrinters()
            statusMsg = successMessage
            statusSev = "success"
            root.remotePrintAccepted(remotePrinterId, taskId)
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
            replacePrintersModel(list, false)
            statusMsg = qsTr("%1 printer(s) refreshed from cloud.").arg(String(list.length))
            statusSev = "success"
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
                root.selectedPrinterDetails = resolvedDetails

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
            }
        }
    }

    Connections {
        target: (typeof mqttBridge !== "undefined" && mqttBridge !== null) ? mqttBridge : null
        ignoreUnknownSignals: true

        function onRealtimeEventTickChanged() {
            if (mqttRealtimeDebounceTimer.running)
                mqttRealtimeDebounceTimer.restart()
            else
                mqttRealtimeDebounceTimer.start()
        }

        function onPrinterFileListReceived(printerId, source, records, state, code, message) {
            root.applyPrinterLocalFilesFromMqtt(printerId, source, records, state, code, message)
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
        dialogTitle: qsTr("Select Printer Local File")
        dialogSubtitle: qsTr("Files currently stored on the selected printer")
        emptyText: root.localFilesLoading
                   ? qsTr("Waiting for printer file list...")
                   : qsTr("No local file available on printer.")
        startButtonText: qsTr("Start Local Print")
        filesModel: printerLocalFilesModel
        selectedFileId: root.selectedPrinterLocalFileName
        fileTypeProvider: root.fileType
        onSelectedFileChanged: function(fileId) {
            root.selectedPrinterLocalFileName = fileId
        }
        onCloseRequested: selectLocalPrinterFileDialog.close()
        onStartRequested: root.startPrintFromPrinterLocalFile()
    }

    PrinterRemotePrintConfigDialog {
        id: remotePrintConfigDialog
        printersModel: printersModel
        compatiblePrintersModel: remoteCompatiblePrintersModel
        remotePrinterId: root.remotePrinterId
        selectedCloudFileId: root.selectedCloudFileId
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
        onChangePrinterRequested: {
            remotePrintConfigDialog.close()
            root.openSelectCloudFileDialog(root.remotePrinterId)
        }
        onCloseRequested: remotePrintConfigDialog.close()
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
        onPrinterMqttDetailsRequested: function(printerId) {
            root.buildPrinterMqttDetails(printerId)
            mqttDetailsDialog.open()
        }
    }

    AppDialogFrame {
        id: mqttDetailsDialog
        title: root.mqttDetailsTitle.length > 0 ? root.mqttDetailsTitle : qsTr("MQTT details")
        subtitle: qsTr("All captured broker messages for this printer")
        minimumWidth: 980
        maximumWidth: 1280
        minimumHeight: 560
        maximumHeight: 980

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TextArea {
                readOnly: true
                text: root.mqttDetailsText
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
