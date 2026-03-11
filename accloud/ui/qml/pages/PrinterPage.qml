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
    signal statusBroadcast(string message, string severity, string operationId)

    property bool loading: false
    property string statusMsg: qsTr("Ready.")
    property string statusSev: "info" // info | success | warn | error
    property string selectedPrinterId: ""
    property bool debugUi: false
    property bool showDebugLabels: debugUi
    property string printersEndpointPath: "/p/p/workbench/api/work/printer/getPrinters + /p/p/workbench/api/work/project/getProjects?printer_id=<id>&print_status=1"
    property string printersEndpointRawJson: ""

    property string remotePrinterId: ""
    property string selectedCloudFileId: ""
    property bool optionDeleteAfterPrint: false
    property bool optionDryRun: false
    property bool optionHighPriority: false
    property bool optionLiftCompensation: false
    property bool optionAutoResinCheck: true
    property bool remotePrintAllowed: true
    property string remotePrintBlockReason: ""
    property var selectedPrinterDetails: ({})
    property string selectedPrinterDetailsRawJson: ""
    property string selectedPrinterProjectsRawJson: ""
    property var liveProjectData: ({})
    property bool freezeRecentJobsCard: true
    property string recentJobsSnapshotPrinterId: ""
    property bool loadingPrinterDetails: false
    property bool loadingPrinterHistory: false
    property bool reasonCatalogLoaded: false
    property bool reasonCatalogLoading: false
    property var reasonCatalogByCode: ({})
    property bool printerAutoRefreshStarted: false
    property int autoRefreshIntervalMs: 30000
    property int autoRefreshPrintingIntervalMs: 5000
    property int mqttRealtimeDebounceMs: 700

    function emitStatusToShell() {
        var msg = String(statusMsg || "").trim()
        if (msg.length === 0)
            return
        root.statusBroadcast(msg, String(statusSev || "info"), "op_printer_refresh")
    }

    onStatusMsgChanged: root.emitStatusToShell()
    onStatusSevChanged: root.emitStatusToShell()
    onSelectedPrinterIdChanged: root.updatePrintersAutoRefreshInterval()

    ListModel {
        id: printersModel
    }

    ListModel {
        id: printCloudFilesModel
    }

    ListModel {
        id: printerHistoryModel
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
            "失败": qsTr("Failed"),
            "成功": qsTr("Success"),
            "错误": qsTr("Error"),
            "超时": qsTr("Timeout")
        }

        for (var key in replacements) {
            if (Object.prototype.hasOwnProperty.call(replacements, key))
                text = text.split(key).join(replacements[key])
        }

        if (/[\u4e00-\u9fff]/.test(text))
            text = text.replace(/[\u4e00-\u9fff]+/g, qsTr("localized backend message"))

        return text
    }

    function fileType(fileName) {
        var name = String(fileName || "")
        var dot = name.lastIndexOf(".")
        if (dot < 0 || dot + 1 >= name.length)
            return "other"
        return name.slice(dot + 1).toLowerCase()
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
        return Math.max(0, Math.min(100, Math.round(value))) + "%"
    }

    function timeText(seconds) {
        var sec = Number(seconds)
        if (!isFinite(sec) || sec < 0)
            return "-"
        var h = Math.floor(sec / 3600)
        var m = Math.floor((sec % 3600) / 60)
        if (h > 0)
            return qsTr("%1h %2m").arg(h).arg(m)
        return qsTr("%1m").arg(m)
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

        if (!hasCompatibilityByFileIdEndpoint())
            return { "ok": true, "reason": "" }

        var compat = cloudBridge.fetchCompatiblePrintersByFileId(selectedCloudFileId)
        if (compat.ok !== true)
            return {
                "ok": false,
                "reason": qsTr("Compatibility check failed: %1")
                        .arg(String(compat.message || qsTr("unknown error")))
            }

        var list = compat.printers !== undefined ? compat.printers : []
        for (var j = 0; j < list.length; ++j) {
            var item = list[j]
            if (String(item.id) !== String(remotePrinterId))
                continue
            var available = Number(item.available)
            if (isFinite(available) && available > 0)
                return { "ok": true, "reason": "" }
            var reason = normalizedCompatReason(item.reason)
            return {
                "ok": false,
                "reason": reason.length > 0 ? reason : qsTr("Printer is not compatible with this file.")
            }
        }

        return { "ok": false, "reason": qsTr("Selected printer not returned by compatibility check.") }
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

    function selectedCloudFileData() {
        if (selectedCloudFileId.length === 0)
            return null
        for (var i = 0; i < printCloudFilesModel.count; ++i) {
            var f = printCloudFilesModel.get(i)
            if (String(f.fileId) === selectedCloudFileId)
                return f
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
        var active = firstActiveProject(projectsList)
        liveProjectData = active ? active : ({})
    }

    function hasLiveProjectData() {
        return liveProjectData !== null
                && liveProjectData !== undefined
                && Object.keys(liveProjectData).length > 0
    }

    function shouldRefreshRecentJobsCard() {
        if (!freezeRecentJobsCard)
            return true
        return recentJobsSnapshotPrinterId !== selectedPrinterId || printerHistoryModel.count === 0
    }

    function replaceRecentJobsCard(projectsList) {
        printerHistoryModel.clear()
        var list = projectsList !== undefined && projectsList !== null ? projectsList : []
        for (var i = 0; i < list.length; ++i)
            printerHistoryModel.append(list[i])
        recentJobsSnapshotPrinterId = selectedPrinterId
    }

    function selectedPrinterLiveData() {
        var selected = selectedPrinterData()
        if (!selected)
            return null

        var merged = {}
        for (var key in selected)
            merged[key] = selected[key]

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

        var details = selectedPrinterDetails || ({})
        mergeTextField("currentFile", details.currentFile)
        mergeNumberField("progress", details.progress)
        mergeNumberField("elapsedSec", details.elapsedSec)
        mergeNumberField("remainingSec", details.remainingSec)
        mergeNumberField("currentLayer", details.currentLayer)
        mergeNumberField("totalLayers", details.totalLayers)

        var activeProject = hasLiveProjectData() ? liveProjectData : activeProjectFromHistory()
        if (activeProject) {
            mergeTextField("currentFile", String(activeProject.currentFile || activeProject.gcodeName || ""))
            mergeNumberField("progress", activeProject.progress)
            mergeNumberField("elapsedSec", activeProject.elapsedSec)
            mergeNumberField("remainingSec", activeProject.remainingSec)
            mergeNumberField("currentLayer", activeProject.currentLayer)
            mergeNumberField("totalLayers", activeProject.totalLayers)
        }

        return merged
    }

    function choosePrinter(printerId) {
        selectedPrinterId = String(printerId || "")
        updatePrintersAutoRefreshInterval()
        loadSelectedPrinterInsights()
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
    }

    function ensureReasonCatalogLoaded() {
        if (reasonCatalogLoaded || reasonCatalogLoading)
            return
        if (!hasCloudBridge() || typeof cloudBridge.fetchReasonCatalog !== "function")
            return

        reasonCatalogLoading = true
        var r = cloudBridge.fetchReasonCatalog()
        reasonCatalogLoading = false
        if (r.ok !== true) {
            statusMsg = qsTr("Reason catalog unavailable: ") + String(r.message || "")
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

    function selectedPrinterReasonText() {
        var selected = selectedPrinterData()
        if (!selected)
            return ""
        return String(selected.reason || "")
    }

    function selectedPrinterHelpUrlText() {
        return reasonHelpUrl(selectedPrinterReasonText())
    }

    function displayReason(reasonText) {
        var text = String(reasonText || "").trim()
        if (text.length === 0)
            return ""

        var entry = reasonEntryFromText(text)
        if (!entry)
            return text

        var desc = String(entry.desc || "").trim()
        if (desc.length === 0)
            return text
        return desc + " (" + text + ")"
    }

    function reasonHelpUrl(reasonText) {
        var entry = reasonEntryFromText(reasonText)
        if (!entry)
            return ""
        return String(entry.helpUrl || "").trim()
    }

    function loadSelectedPrinterInsights() {
        selectedPrinterDetails = ({})
        selectedPrinterDetailsRawJson = ""
        selectedPrinterProjectsRawJson = ""
        liveProjectData = ({})

        if (selectedPrinterId.length === 0)
            return

        var refreshRecentJobsCard = shouldRefreshRecentJobsCard()
        if (refreshRecentJobsCard)
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
                if (refreshRecentJobsCard)
                    replaceRecentJobsCard(cachedProjectsList)
            }
        } else if (selected && selected.projects !== undefined) {
            var inlineProjects = selected.projects
            setLiveProjectFromList(inlineProjects)
            if (refreshRecentJobsCard)
                replaceRecentJobsCard(inlineProjects)
        }

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
            loadingPrinterHistory = refreshRecentJobsCard
            var projectsRes = cloudBridge.fetchPrinterProjects(selectedPrinterId, 1, 20)
            loadingPrinterHistory = false
            if (projectsRes.ok === true) {
                var cloudProjects = projectsRes.projects !== undefined ? projectsRes.projects : []
                setLiveProjectFromList(cloudProjects)
                if (refreshRecentJobsCard)
                    replaceRecentJobsCard(cloudProjects)
            }
            if (projectsRes.rawJson !== undefined)
                selectedPrinterProjectsRawJson = String(projectsRes.rawJson || selectedPrinterProjectsRawJson)
        }

        updatePrintersAutoRefreshInterval()
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
        replacePrintersModel(printers)

        loadSelectedPrinterInsights()
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

    function replacePrintersModel(printers) {
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

        loadSelectedPrinterInsights()
        updatePrintersAutoRefreshInterval()
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
        replacePrintersModel(list)
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

    function compatibilityEntryAllows(item) {
        var available = Number(item && item.available !== undefined ? item.available : 1)
        return isFinite(available) ? available > 0 : true
    }

    function compatibilityFailureReason(compatResult, fallbackMessage) {
        if (compatResult === null || compatResult === undefined || compatResult.ok !== true)
            return fallbackMessage
        var list = compatResult.printers !== undefined ? compatResult.printers : []
        if (list.length <= 0)
            return fallbackMessage
        for (var i = 0; i < list.length; ++i) {
            var item = list[i]
            if (compatibilityEntryAllows(item))
                continue
            var reason = normalizedCompatReason(item.reason)
            if (reason.length > 0)
                return reason
        }
        return fallbackMessage
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

    function setSingleRemotePrintFile(fileId, fileName) {
        var normalizedFileId = String(fileId || "").trim()
        if (normalizedFileId.length === 0)
            return

        var normalizedFileName = String(fileName || "").trim()
        var existing = null
        for (var i = 0; i < printCloudFilesModel.count; ++i) {
            var item = printCloudFilesModel.get(i)
            if (String(item.fileId) === normalizedFileId) {
                existing = item
                break
            }
        }

        var entry = existing ? existing : ({})
        entry.fileId = normalizedFileId
        entry.fileName = normalizedFileName.length > 0
                ? normalizedFileName
                : String(entry.fileName || qsTr("Cloud file"))
        entry.sizeText = String(entry.sizeText || "-")
        entry.status = String(entry.status || "READY")
        entry.printTime = String(entry.printTime || "-")
        entry.resinUsage = String(entry.resinUsage || "-")

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

        if (printersModel.count <= 0)
            loadPrinters()
        if (printersModel.count <= 0) {
            statusMsg = qsTr("No printer available for remote print.")
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
                var byFilePrinterId = preferredCompatiblePrinterId(fileCompat, preferredPrinterId)
                if (byFilePrinterId.length > 0)
                    targetPrinterId = byFilePrinterId
                else {
                    statusMsg = qsTr("Print blocked: %1")
                            .arg(compatibilityFailureReason(fileCompat,
                                                            qsTr("No compatible printer available for this file.")))
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
                    var byExtPrinterId = preferredCompatiblePrinterId(extCompat, preferredPrinterId)
                    if (byExtPrinterId.length > 0)
                        targetPrinterId = byExtPrinterId
                    else {
                        statusMsg = qsTr("Print blocked: %1")
                                .arg(compatibilityFailureReason(extCompat,
                                                                qsTr("No compatible printer available for this file type.")))
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
            statusMsg = qsTr("No printer available for remote print.")
            statusSev = "warn"
            return
        }

        choosePrinter(targetPrinterId)
        remotePrinterId = targetPrinterId
        setSingleRemotePrintFile(normalizedFileId, normalizedFileName)
        openRemotePrintConfig()

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
        printCloudFilesModel.clear()
        selectedCloudFileId = ""

        var files = []
        if (hasCloudBridge()) {
            var listing = cloudBridge.fetchFiles(1, 100)
            if (listing.ok === true) {
                files = listing.files !== undefined ? listing.files : []
            } else {
                statusMsg = qsTr("Cannot load cloud files for print: ") + String(listing.message)
                statusSev = "error"
                return
            }
        } else {
            files = [
                {
                    "fileId": "demo-001",
                    "fileName": "rook_plate_v12.pwmb",
                    "sizeText": "42.6 MB",
                    "status": "READY",
                    "printTime": "02h 15m",
                    "resinUsage": "67 ml"
                },
                {
                    "fileId": "demo-002",
                    "fileName": "calibration_tower.pws",
                    "sizeText": "11.8 MB",
                    "status": "READY",
                    "printTime": "00h 48m",
                    "resinUsage": "14 ml"
                }
            ]
        }

        var compatCache = {}
        var compatFailed = false

        for (var i = 0; i < files.length; ++i) {
            var file = files[i]
            var ext = fileType(file.fileName)
            var compat = null

            if (hasCompatibilityEndpoint()) {
                if (compatCache[ext] === undefined) {
                    compatCache[ext] = cloudBridge.fetchCompatiblePrintersByExt(ext)
                }
                compat = compatCache[ext]
                if (compat.ok !== true)
                    compatFailed = true
            }

            if (compatibilityAllowsPrinter(compat, printerId))
                printCloudFilesModel.append(file)
        }

        if (compatFailed) {
            statusMsg = qsTr("Compatibility endpoint partial failure. Showing best-effort list.")
            statusSev = "warn"
        }

        if (printCloudFilesModel.count > 0)
            selectedCloudFileId = String(printCloudFilesModel.get(0).fileId)
    }

    function openSelectCloudFileDialog(printerId) {
        remotePrinterId = String(printerId || selectedPrinterId)
        loadCloudFilesForRemotePrint(remotePrinterId)
        selectCloudFileDialog.open()
    }

    function openRemotePrintConfig() {
        if (selectedCloudFileId.length === 0)
            return

        optionDeleteAfterPrint = false
        optionDryRun = false
        optionHighPriority = false
        remotePrintAllowed = true
        remotePrintBlockReason = ""
        refreshRemotePrintGuard()
        remotePrintConfigDialog.open()
    }

    function startRemotePrint() {
        if (remotePrinterId.length === 0) {
            statusMsg = qsTr("Select a printer first.")
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
            statusMsg = qsTr("Demo: remote print payload prepared for ") + String(fileData.fileName)
            statusSev = "warn"
            remotePrintConfigDialog.close()
            return
        }

        var r = cloudBridge.sendPrintOrder(remotePrinterId,
                                           String(fileData.fileId),
                                           optionDeleteAfterPrint,
                                           optionDryRun)
        if (r.ok === true) {
            var taskId = String(r.taskId || "")
            statusMsg = taskId.length > 0
                    ? (qsTr("Print order sent (task_id=%1)").arg(taskId))
                    : qsTr("Print order sent.")
            statusSev = optionDryRun ? "warn" : "success"
            remotePrintConfigDialog.close()
            loadPrinters()
        } else {
            statusMsg = qsTr("Print order failed: ") + String(r.message)
            statusSev = "error"
        }
    }

    Component.onCompleted: {
        ensureReasonCatalogLoaded()
        loadPrinters()
    }

    Timer {
        id: printersAutoRefreshTimer
        objectName: "printersAutoRefreshTimer"
        interval: root.autoRefreshIntervalMs
        repeat: true
        running: false
        triggeredOnStart: false
        onTriggered: {
            if (hasCloudBridge() && typeof cloudBridge.refreshPrintersAsync === "function")
                cloudBridge.refreshPrintersAsync(true)
        }
    }

    Timer {
        id: mqttRealtimeDebounceTimer
        interval: root.mqttRealtimeDebounceMs
        repeat: false
        running: false
        onTriggered: root.refreshPrintersFromCacheOnly()
    }

    Connections {
        target: root.hasQObjectCloudBridge() ? cloudBridge : null
        ignoreUnknownSignals: true

        function onPrintersUpdatedFromCloud(printers, message) {
            var list = printers !== undefined ? printers : []
            replacePrintersModel(list)
            statusMsg = qsTr("%1 printer(s) refreshed from cloud.").arg(String(list.length))
            statusSev = "success"
            if (!root.printerAutoRefreshStarted) {
                root.printerAutoRefreshStarted = true
                printersAutoRefreshTimer.start()
            }
        }

        function onSyncFailed(scope, message) {
            if (String(scope) !== "printers")
                return
            statusMsg = qsTr("Background sync failed (printers): ") + String(message)
            statusSev = "warn"
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
    }

    PrinterSelectCloudFileDialog {
        id: selectCloudFileDialog
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

    PrinterRemotePrintConfigDialog {
        id: remotePrintConfigDialog
        printersModel: printersModel
        remotePrinterId: root.remotePrinterId
        selectedCloudFileId: root.selectedCloudFileId
        selectedFileName: selectedCloudFileData() ? String(selectedCloudFileData().fileName || "-") : "-"
        selectedPrinterName: selectedPrinterData() ? String(selectedPrinterData().name || "-") : "-"
        selectedPrintTime: selectedCloudFileData() ? String(selectedCloudFileData().printTime || "-") : "-"
        selectedResinUsage: selectedCloudFileData() ? String(selectedCloudFileData().resinUsage || "-") : "-"
        optionHighPriority: root.optionHighPriority
        optionDeleteAfterPrint: root.optionDeleteAfterPrint
        optionDryRun: root.optionDryRun
        remotePrintAllowed: root.remotePrintAllowed
        remotePrintBlockReason: root.remotePrintBlockReason
        translateLocalizedTextProvider: root.translateLocalizedText
        onRemotePrinterChanged: function(printerId) {
            root.remotePrinterId = printerId
        }
        onOptionHighPriorityToggled: function(checked) {
            root.optionHighPriority = checked
        }
        onOptionDeleteAfterPrintToggled: function(checked) {
            root.optionDeleteAfterPrint = checked
        }
        onOptionDryRunToggled: function(checked) {
            root.optionDryRun = checked
        }
        onRefreshGuardRequested: root.refreshRemotePrintGuard()
        onChangePrinterRequested: {
            remotePrintConfigDialog.close()
            root.openSelectCloudFileDialog(root.remotePrinterId)
        }
        onMoreRequested: printConfigDialog.open()
        onCloseRequested: remotePrintConfigDialog.close()
        onStartRequested: root.startRemotePrint()
    }

    PrinterAdvancedPrintConfigDialog {
        id: printConfigDialog
        liftCompensation: root.optionLiftCompensation
        autoResinCheck: root.optionAutoResinCheck
        onLiftCompensationToggled: function(checked) {
            root.optionLiftCompensation = checked
        }
        onAutoResinCheckToggled: function(checked) {
            root.optionAutoResinCheck = checked
        }
        onCloseRequested: printConfigDialog.close()
    }

    PrinterMainPanel {
        embeddedInTabsContainer: root.embeddedInTabsContainer
        loading: root.loading
        showDebugLabels: root.showDebugLabels
        printersModel: printersModel
        selectedPrinterId: root.selectedPrinterId
        tabTitleProvider: root.printerTabTitle
        selectedPrinter: root.selectedPrinterLiveData()
        selectedPrinterDetails: root.selectedPrinterDetails
        selectedLiveJobData: root.liveProjectData
        selectedPrinterDetailsRawJson: root.selectedPrinterDetailsRawJson
        selectedPrinterProjectsRawJson: root.selectedPrinterProjectsRawJson
        loadingPrinterHistory: root.loadingPrinterHistory
        printerHistoryModel: printerHistoryModel
        printersEndpointPath: root.printersEndpointPath
        printersEndpointRawJson: root.printersEndpointRawJson
        selectedPrinterHelpUrlText: root.selectedPrinterHelpUrlText()
        statusChipTextProvider: root.statusChipText
        progressTextProvider: root.progressText
        timeTextProvider: root.timeText
        unixTimeTextProvider: root.unixTimeText
        printStatusTextProvider: root.printStatusText
        prettyJsonProvider: root.prettyJson
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
            root.openSelectCloudFileDialog(printerId)
        }
        onLocalFileRequested: {
            root.statusMsg = qsTr("Local file remote print entrypoint is not implemented yet.")
            root.statusSev = "warn"
        }
    }
}
