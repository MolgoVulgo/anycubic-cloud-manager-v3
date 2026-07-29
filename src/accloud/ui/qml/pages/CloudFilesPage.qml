import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs
import QtQuick.Layouts 1.15
import QtCore
import Accloud.Models 1.0
import "../components/Theme.js" as Theme
import "../components"

Item {
    id: root
    objectName: "cloudFilesPage"
    Layout.fillWidth: true
    Layout.fillHeight: true
    property bool embeddedInTabsContainer: false
    readonly property bool buildDebugEnabled: (typeof accloudBuildDebugEnabled !== "undefined")
                                             && accloudBuildDebugEnabled === true
    property alias filesModel: cloudFilesModel
    signal statusBroadcast(string message, string severity, string operationId)
    signal printIntentRequested(string fileId, string fileName)
    signal pwszUploadSettingsChanged()

    // UI state
    property bool loading: false
    property bool showAdvancedDetails: false
    property string statusMsg: qsTr("Ready.")
    property string statusSev: "info" // info | success | warn | error
    property var quotaData: null
    property string typeFilterValue: "all"
    property var typeFilterOptions: [
        { "code": "all", "label": qsTr("All") }
    ]
    property string selectedFileId: ""
    property var selectedFiles: []
    readonly property int selectedFilesCount: selectedFiles.length
    property bool batchDeleteRunning: false
    property var batchDeleteQueue: []
    property int batchDeleteCompleted: 0
    property int batchDeleteTotal: 0
    property int batchDeleteSucceeded: 0
    property var batchDeleteFailures: []
    property string batchDeleteCurrentId: ""
    property string batchDeleteSummaryMessage: ""
    property string batchDeleteSummarySeverity: "success"
    property bool batchDeleteSummaryPending: false
    property bool batchDeleteAwaitingCloudRefresh: false
    readonly property int visibleFilesCount: cloudFilesModel.visibleCount
    readonly property int pageTotal: cloudFilesModel.totalPages
    readonly property string uploadLastFolderSettingsKey: "ui.cloudFiles.upload.lastFolderPath"
    readonly property string pwszPreviewCompletionSettingsKey: "cloud.upload.completePwszPreview2"
    readonly property string pwszPreviewConfirmationSettingsKey: "cloud.upload.confirmPwszPreview2"
    property string uploadLastFolderPath: StandardPaths.writableLocation(StandardPaths.HomeLocation)
    property var pwszCloudUpdateCandidates: []
    property real pwszCloudUpdateBytes: 0
    property bool pwszCloudUpdateRunning: false
    property bool pwszCloudUpdateCancelRequested: false
    property int pwszCloudUpdateCurrent: 0
    property int pwszCloudUpdateTotal: 0
    property string pwszCloudUpdateFileName: ""
    property string pwszCloudUpdatePhase: ""
    property var pwszCloudUpdateSummary: ({})
    property bool pwszCloudUpdateProposalPending: false
    property bool pwszCloudUpdateProposalOpened: false
    readonly property bool pwszCloudUpdateProposalVisible: pwszCloudUpdateProposalPending
                                                           || pwszCloudUpdateConfirmDialog.visible
    readonly property bool pwszCloudUpdateResultVisible: pwszCloudUpdateResultDialog.visible
    readonly property var supportedExtensions: [
        "photon", "pws", "pwsz", "photons", "pw0", "pwx", "pwmo", "pwma", "pwms",
        "pwmx", "pmx2", "pmsq", "dlp", "dl2p", "pwmb", "pm3", "pm3m",
        "pm3r", "pm3n", "px6s", "pm5", "pm5s", "m5sp"
    ]

    function emitStatusToShell() {
        var msg = String(statusMsg || "").trim()
        if (msg.length === 0)
            return
        root.statusBroadcast(msg, String(statusSev || "info"), "op_files_refresh")
    }

    onStatusMsgChanged: root.emitStatusToShell()
    onStatusSevChanged: root.emitStatusToShell()

    // Table column widths
    property int colSelectWidth: 30
    property int colThumbWidth: 76
    property int colTypeWidth: 72
    property int colSizeWidth: 86
    property int colDateWidth: 92
    property int colActionsWidth: 326
    readonly property int tableRowHorizontalMargin: 12
    readonly property int tableScrollbarReserve: 12
    readonly property int tableColumnSpacing: 8
    readonly property int actionDetailsWidth: 78
    readonly property int actionDownloadWidth: 104
    readonly property int actionPrintWidth: 82
    readonly property int actionMenuWidth: 36
    readonly property int tableFixedColumnsWidth: colSelectWidth + colThumbWidth + colTypeWidth + colSizeWidth + colDateWidth + colActionsWidth + tableColumnSpacing * 6
    readonly property int colNameWidth: Math.max(220, tableViewportWidth - tableFixedColumnsWidth)
    readonly property int pageContentPadding: 12
    property int pageSize: 10
    property int currentPage: 0
    readonly property int tableViewportWidth: Math.max(0,
                                                        pageFrame.width
                                                        - (pageContentPadding * 2)
                                                        - (Theme.borderWidth * 2)
                                                        - (tableRowHorizontalMargin * 2)
                                                        - tableScrollbarReserve)
    readonly property int colXSelect: 0
    readonly property int colXThumb: colXSelect + colSelectWidth + tableColumnSpacing
    readonly property int colXName: colXThumb + colThumbWidth + tableColumnSpacing
    readonly property int colXType: colXName + colNameWidth + tableColumnSpacing
    readonly property int colXSize: colXType + colTypeWidth + tableColumnSpacing
    readonly property int colXDate: colXSize + colSizeWidth + tableColumnSpacing
    readonly property int colXActions: colXDate + colDateWidth + tableColumnSpacing

    CloudFilesModel {
        id: cloudFilesModel
        currentPage: root.currentPage
        pageSize: root.pageSize
        typeFilter: root.typeFilterValue
    }

    Connections {
        target: cloudFilesModel
        function onCurrentPageChanged() {
            if (root.currentPage !== cloudFilesModel.currentPage)
                root.currentPage = cloudFilesModel.currentPage
        }
    }

    function hasCloudBridge() {
        return (typeof cloudBridge !== "undefined")
                && cloudBridge !== null
                && typeof cloudBridge.fetchFiles === "function"
                && typeof cloudBridge.fetchQuota === "function"
    }

    function hasUiSettingsBridge() {
        return (typeof uiSettingsBridge !== "undefined")
                && uiSettingsBridge !== null
                && typeof uiSettingsBridge.getString === "function"
                && typeof uiSettingsBridge.setString === "function"
    }

    function requestPwszCloudUpdateCancellation() {
        if (!root.pwszCloudUpdateRunning || root.pwszCloudUpdateCancelRequested)
            return false
        root.pwszCloudUpdateCancelRequested = true
        if (root.hasCloudBridge()
                && typeof cloudBridge.cancelPwszCloudPreviewUpdate === "function") {
            cloudBridge.cancelPwszCloudPreviewUpdate()
        }
        return true
    }

    function pwszCloudUpdatePhaseText(phaseKey) {
        var key = String(phaseKey || "")
        switch (key) {
        case "pwsz.update.download":
            return qsTr("Downloading cloud file")
        case "pwsz.update.prepare":
            return qsTr("Preparing the PWSZ archive")
        case "pwsz.update.upload":
            return qsTr("Uploading the modified version")
        case "pwsz.update.cloud_processing":
            return qsTr("Waiting for cloud processing")
        case "pwsz.update.validate_thumbnail":
            return qsTr("Validating the new thumbnail")
        case "pwsz.update.delete_original":
            return qsTr("Deleting the original version")
        default:
            return root.sanitizedBackendMessage(key)
        }
    }

    function pwszCloudUpdateStatusLabel(status) {
        switch (String(status || "")) {
        case "failed":
            return qsTr("Failed")
        case "partial":
            return qsTr("Partial")
        case "cancelled":
            return qsTr("Cancelled")
        case "modified":
            return qsTr("Modified")
        case "skipped":
            return qsTr("Already compliant")
        default:
            return qsTr("Unknown")
        }
    }

    function pwszCloudUpdateResultTitle(summary) {
        var value = summary || ({})
        if (value.cancelled === true)
            return qsTr("Update cancelled")
        if (value.ok === true)
            return qsTr("Update completed")
        return qsTr("Update completed with issues")
    }

    function pwszCloudUpdateIssueDetails(summary) {
        var value = summary || ({})
        var items = value.items || []
        var lines = []
        for (var i = 0; i < items.length; ++i) {
            var item = items[i] || ({})
            var status = String(item.status || "")
            if (status !== "failed" && status !== "partial" && status !== "cancelled")
                continue
            var name = String(item.fileName || item.originalFileId || qsTr("Unknown file"))
            var detail = root.backendStatusDetail(item.message, qsTr("No additional detail."))
            var ids = qsTr("original id: %1").arg(String(item.originalFileId || "-"))
            if (String(item.newFileId || "").length > 0)
                ids += qsTr(", new id: %1").arg(String(item.newFileId))
            lines.push(qsTr("%1 — %2 — %3 (%4)")
                       .arg(name)
                       .arg(root.pwszCloudUpdateStatusLabel(status))
                       .arg(detail)
                       .arg(ids))
        }
        return lines.join("\n")
    }


    function booleanSetting(key, defaultValue) {
        if (!hasUiSettingsBridge())
            return defaultValue === true
        var fallback = defaultValue === true ? "true" : "false"
        return String(uiSettingsBridge.getString(key, fallback)).toLowerCase() !== "false"
    }

    function persistBooleanSetting(key, value) {
        if (!hasUiSettingsBridge())
            return
        uiSettingsBridge.setString(key, value === true ? "true" : "false")
        if (typeof uiSettingsBridge.sync === "function")
            uiSettingsBridge.sync()
        root.pwszUploadSettingsChanged()
    }

    function sanitizedBackendMessage(rawMessage) {
        var text = String(rawMessage || "").trim()
        if (text.length === 0)
            return ""
        if (/[\u4e00-\u9fff]/.test(text))
            text = text.replace(/[\u4e00-\u9fff]+/g, qsTr("localized backend message"))
        return text
    }

    function backendStatusDetail(rawMessage, fallbackMessage) {
        var detail = sanitizedBackendMessage(rawMessage)
        return detail.length > 0 ? detail : String(fallbackMessage || qsTr("unknown error"))
    }

    function formatBytes(bytes) {
        var value = Number(bytes)
        if (!isFinite(value) || value < 0)
            return "-"
        if (value >= 1073741824)
            return qsTr("%1 GB").arg((value / 1073741824).toFixed(1))
        if (value >= 1048576)
            return qsTr("%1 MB").arg((value / 1048576).toFixed(1))
        if (value >= 1024)
            return qsTr("%1 KB").arg((value / 1024).toFixed(1))
        return qsTr("%1 B").arg(Math.round(value))
    }

    function quotaRatio() {
        if (!quotaData || !quotaData.totalBytes || quotaData.totalBytes <= 0)
            return 0
        var ratio = Number(quotaData.usedBytes) / Number(quotaData.totalBytes)
        if (!isFinite(ratio))
            return 0
        return Math.max(0, Math.min(1, ratio))
    }

    function quotaUsedText() {
        if (!quotaData)
            return qsTr("Used / Total: -")
        return qsTr("Used %1 / %2")
                .arg(String(quotaData.usedDisplay || "-"))
                .arg(String(quotaData.totalDisplay || "-"))
    }

    function quotaFreeText() {
        if (!quotaData)
            return qsTr("Free -")
        var free = Number(quotaData.totalBytes) - Number(quotaData.usedBytes)
        return qsTr("Free %1").arg(formatBytes(free))
    }

    function quotaFreeRatio() {
        return Math.max(0, 1 - quotaRatio())
    }

    function colorFromHex(hexColor) {
        var raw = String(hexColor || "")
        if (raw.length !== 7 || raw.charAt(0) !== "#")
            return Qt.rgba(0, 0, 0, 1)
        var r = parseInt(raw.slice(1, 3), 16)
        var g = parseInt(raw.slice(3, 5), 16)
        var b = parseInt(raw.slice(5, 7), 16)
        if (!isFinite(r) || !isFinite(g) || !isFinite(b))
            return Qt.rgba(0, 0, 0, 1)
        return Qt.rgba(r / 255, g / 255, b / 255, 1)
    }

    function mixColors(fromColor, toColor, ratio) {
        var t = Math.max(0, Math.min(1, Number(ratio)))
        var from = colorFromHex(fromColor)
        var to = colorFromHex(toColor)
        return Qt.rgba(from.r + (to.r - from.r) * t,
                       from.g + (to.g - from.g) * t,
                       from.b + (to.b - from.b) * t,
                       1)
    }

    function quotaBarColor() {
        return Theme.accent
    }

    function quotaBackgroundColor() {
        return mixColors(Theme.danger, Theme.success, quotaFreeRatio())
    }

    function fileExtension(fileName) {
        var name = String(fileName || "")
        var dot = name.lastIndexOf(".")
        if (dot < 0 || dot + 1 >= name.length)
            return ""
        return name.slice(dot + 1).toLowerCase()
    }

    function isSupportedExtension(ext) {
        var value = String(ext || "").toLowerCase()
        for (var i = 0; i < supportedExtensions.length; ++i) {
            if (supportedExtensions[i] === value)
                return true
        }
        return false
    }

    function localPathFromInput(pathInput) {
        var raw = String(pathInput || "").trim()
        if (raw.length === 0)
            return ""

        var normalized = raw.replace(/^file:\/\/localhost/i, "file://")
        if (normalized.indexOf("file://") === 0)
            normalized = normalized.replace(/^file:\/\//i, "")
        normalized = normalized.replace(/[?#].*$/, "")
        try {
            normalized = decodeURIComponent(normalized)
        } catch (err) {}
        normalized = normalized.replace(/\\/g, "/")
        if (normalized.length > 1)
            normalized = normalized.replace(/\/+$/, "")
        return normalized
    }

    function pathToFileUrl(pathInput) {
        var path = localPathFromInput(pathInput)
        if (path.length === 0)
            path = StandardPaths.writableLocation(StandardPaths.HomeLocation)
        if (path.charAt(0) === "/")
            return "file://" + path
        return "file:///" + path
    }

    function parentFolderPath(pathInput) {
        var path = localPathFromInput(pathInput)
        var slash = path.lastIndexOf("/")
        if (slash <= 0)
            return path
        return path.slice(0, slash)
    }

    function persistUploadLastFolder(pathInput) {
        if (!hasUiSettingsBridge())
            return
        var normalized = localPathFromInput(pathInput)
        if (normalized.length <= 0)
            return
        uiSettingsBridge.setString(uploadLastFolderSettingsKey, normalized)
        if (typeof uiSettingsBridge.sync === "function")
            uiSettingsBridge.sync()
    }

    function restoreUploadLastFolderFromSettings() {
        if (!hasUiSettingsBridge())
            return
        var fallback = localPathFromInput(StandardPaths.writableLocation(StandardPaths.HomeLocation))
        var persisted = localPathFromInput(uiSettingsBridge.getString(uploadLastFolderSettingsKey, fallback))
        if (persisted.length <= 0)
            persisted = fallback
        uploadLastFolderPath = persisted
    }

    function fileType(fileName) {
        var ext = fileExtension(fileName)
        if (ext.length === 0)
            return "other"
        return isSupportedExtension(ext) ? ext : "other"
    }

    function fileTypeLabel(fileName) {
        var ext = fileExtension(fileName)
        if (ext.length === 0)
            return "-"
        return ext.toUpperCase()
    }

    function fileNameWithoutExtension(fileName) {
        var name = String(fileName || "").trim()
        if (name.length === 0)
            return qsTr("File Details")
        var dot = name.lastIndexOf(".")
        if (dot > 0)
            return name.slice(0, dot)
        return name
    }

    function fileMatchesFilter(fileName) {
        if (typeFilterValue === "all")
            return true
        return fileExtension(fileName) === String(typeFilterValue)
    }

    function refreshTypeFilterOptions() {
        var sortedExt = cloudFilesModel.availableFileTypes(supportedExtensions)

        var options = [ { "code": "all", "label": qsTr("All") } ]
        for (var j = 0; j < sortedExt.length; ++j) {
            options.push({
                "code": sortedExt[j],
                "label": sortedExt[j].toUpperCase()
            })
        }
        typeFilterOptions = options

        var hasCurrent = false
        for (var k = 0; k < typeFilterOptions.length; ++k) {
            if (String(typeFilterOptions[k].code) === String(typeFilterValue)) {
                hasCurrent = true
                break
            }
        }
        if (!hasCurrent) {
            typeFilterValue = "all"
            currentPage = 0
        }
    }

    function typeFilterIndex() {
        for (var i = 0; i < typeFilterOptions.length; ++i) {
            if (String(typeFilterOptions[i].code) === String(typeFilterValue))
                return i
        }
        return 0
    }

    function compatiblePrintersLabel(fileName) {
        var ext = fileExtension(fileName)
        if (ext === "pm3")
            return qsTr("Photon Mono 3, Mono 3 Ultra")
        if (ext === "pm3m")
            return qsTr("Photon Mono 3 Max")
        if (ext === "pm3r" || ext === "pm3n")
            return qsTr("Photon Mono 3 series")
        if (ext === "pm5")
            return qsTr("Photon Mono M5")
        if (ext === "pm5s")
            return qsTr("Photon Mono M5s")
        if (ext === "m5sp")
            return qsTr("Photon Mono M5s Pro")
        if (ext === "px6s")
            return qsTr("Photon Mono X 6Ks")
        if (ext === "pwmb")
            return qsTr("Modern Photon resin printers")
        if (ext === "pws" || ext === "pwsz" || ext === "photons" || ext === "photon"
                || ext === "pw0" || ext === "pwx" || ext === "pwmo" || ext === "pwma"
                || ext === "pwms" || ext === "pwmx")
            return qsTr("Legacy Photon resin printers")
        if (ext === "pmx2")
            return qsTr("Photon Mono X series")
        if (ext === "pmsq")
            return qsTr("Anycubic resin printers")
        if (ext === "dlp" || ext === "dl2p")
            return qsTr("Anycubic DLP printers")
        if (ext.length === 0)
            return qsTr("Unknown")
        return qsTr("Anycubic resin printers")
    }

    function compatiblePrintersTooltip(fileName) {
        return qsTr("Compatible printers: %1").arg(compatiblePrintersLabel(fileName))
    }

    function visibleFileCount() {
        return cloudFilesModel.visibleCount
    }

    function totalPages() {
        return cloudFilesModel.totalPages
    }

    function clampCurrentPage() {
        currentPage = Math.max(0, Math.min(currentPage, cloudFilesModel.totalPages - 1))
    }

    onTypeFilterValueChanged: {
        currentPage = 0
        clearFileSelection()
    }

    function displayDate(uploadTime) {
        var value = String(uploadTime || "").trim()
        return value.length > 0 ? value : "-"
    }

    function displayStatus(status) {
        var raw = String(status || "UNKNOWN").toUpperCase()
        if (raw === "READY")
            return qsTr("Ready")
        if (raw === "PROCESSING")
            return qsTr("Processing")
        return qsTr("Unknown")
    }

    function statusColor(status) {
        var raw = String(status || "UNKNOWN").toUpperCase()
        if (raw === "READY")
            return Theme.success
        if (raw === "PROCESSING")
            return Theme.warning
        return Theme.fgSecondary
    }

    function fileDataById(fileId) {
        var entry = cloudFilesModel.fileDataById(String(fileId || ""))
        return entry && Object.keys(entry).length > 0 ? entry : null
    }

    function selectedFileIndex(fileId) {
        var normalizedId = String(fileId || "").trim()
        for (var i = 0; i < selectedFiles.length; ++i) {
            if (String(selectedFiles[i].fileId || "") === normalizedId)
                return i
        }
        return -1
    }

    function isFileSelected(fileId) {
        return selectedFileIndex(fileId) >= 0
    }

    function setFileSelected(fileId, fileName, checked) {
        var normalizedId = String(fileId || "").trim()
        if (normalizedId.length === 0 || batchDeleteRunning)
            return

        var next = selectedFiles.slice(0)
        var index = selectedFileIndex(normalizedId)
        if (checked === true && index < 0) {
            next.push({
                "fileId": normalizedId,
                "fileName": String(fileName || normalizedId)
            })
        } else if (checked !== true && index >= 0) {
            next.splice(index, 1)
        }
        selectedFiles = next
    }

    function removeFileSelection(fileId) {
        var normalizedId = String(fileId || "").trim()
        var next = []
        for (var i = 0; i < selectedFiles.length; ++i) {
            if (String(selectedFiles[i].fileId || "") !== normalizedId)
                next.push(selectedFiles[i])
        }
        selectedFiles = next
    }

    function clearFileSelection() {
        if (!batchDeleteRunning)
            selectedFiles = []
    }

    function pruneFileSelection() {
        var kept = []
        for (var i = 0; i < selectedFiles.length; ++i) {
            var item = selectedFiles[i]
            if (fileDataById(item.fileId) !== null)
                kept.push(item)
        }
        selectedFiles = kept
    }

    function openFileDetails(fileId) {
        var entry = fileDataById(fileId)
        if (!entry)
            return
        fileDetailsDialog.fileData = entry
        selectedFileId = String(fileId)
        fileDetailsDialog.open()
    }

    function requestDownload(fileId, fileName) {
        if (!hasCloudBridge() || typeof cloudBridge.getDownloadUrl !== "function") {
            root.statusMsg = qsTr("Download unavailable without backend.")
            root.statusSev = "warn"
            return
        }

        saveDialog.suggestName = String(fileName || qsTr("file"))
        saveDialog.defaultSuffix = String(fileExtension(fileName) || "file")
        if (typeof cloudBridge.getDownloadUrlAsync === "function") {
            root.statusMsg = qsTr("Preparing download...")
            root.statusSev = "info"
            cloudBridge.getDownloadUrlAsync(String(fileId))
            return
        }

        var r = cloudBridge.getDownloadUrl(String(fileId))
        applyDownloadUrlResult(String(fileId), r)
    }

    function applyDownloadUrlResult(fileId, r) {
        if (r.ok !== true) {
            root.statusMsg = qsTr("Cannot get download URL: %1")
                    .arg(backendStatusDetail(r.message, qsTr("Download URL unavailable.")))
            root.statusSev = "error"
            return
        }

        saveDialog.pendingUrl = r.url
        saveDialog.open()
    }

    function requestDelete(fileId, fileName) {
        deleteConfirmDialog.pendingId = String(fileId)
        deleteConfirmDialog.pendingName = String(fileName || fileId)
        deleteConfirmDialog.open()
    }

    function requestDeleteSelected() {
        if (selectedFilesCount <= 0 || batchDeleteRunning)
            return
        batchDeleteConfirmDialog.open()
    }

    function usesBackgroundFilesRefresh() {
        if (!hasCloudBridge())
            return false
        var synchronousCacheFlow = typeof cloudBridge.loadCachedFiles === "function"
                && typeof cloudBridge.loadCachedQuota === "function"
                && typeof cloudBridge.refreshFilesAsync === "function"
        var asynchronousCacheFlow = typeof cloudBridge.loadCachedFilesAsync === "function"
                && typeof cloudBridge.loadCachedQuotaAsync === "function"
                && typeof cloudBridge.refreshFilesAsync === "function"
        return synchronousCacheFlow || asynchronousCacheFlow
    }

    function startBatchDelete() {
        if (selectedFilesCount <= 0 || batchDeleteRunning)
            return
        if (!hasCloudBridge() || typeof cloudBridge.deleteFile !== "function") {
            root.statusMsg = qsTr("Delete unavailable without backend.")
            root.statusSev = "warn"
            return
        }

        batchDeleteQueue = selectedFiles.slice(0)
        batchDeleteCompleted = 0
        batchDeleteTotal = batchDeleteQueue.length
        batchDeleteSucceeded = 0
        batchDeleteFailures = []
        batchDeleteCurrentId = ""
        batchDeleteSummaryPending = false
        batchDeleteAwaitingCloudRefresh = false
        batchDeleteRunning = true
        root.loading = true
        runNextBatchDelete()
    }

    function runNextBatchDelete() {
        if (!batchDeleteRunning)
            return
        if (batchDeleteQueue.length <= 0) {
            finishBatchDelete()
            return
        }

        var nextItem = batchDeleteQueue[0]
        batchDeleteQueue = batchDeleteQueue.slice(1)
        batchDeleteCurrentId = String(nextItem.fileId || "")
        root.statusMsg = qsTr("Deleting file %1 of %2...")
                .arg(String(batchDeleteCompleted + 1))
                .arg(String(batchDeleteTotal))
        root.statusSev = "info"

        if (typeof cloudBridge.deleteFileAsync === "function") {
            cloudBridge.deleteFileAsync(batchDeleteCurrentId)
            return
        }

        var result = cloudBridge.deleteFile(batchDeleteCurrentId)
        handleBatchDeleteResult(batchDeleteCurrentId, result)
    }

    function handleBatchDeleteResult(fileId, result) {
        if (!batchDeleteRunning || String(fileId) !== batchDeleteCurrentId)
            return

        batchDeleteCompleted += 1
        if (result.ok === true) {
            batchDeleteSucceeded += 1
            removeFileSelection(fileId)
        } else {
            var failures = batchDeleteFailures.slice(0)
            failures.push({
                "fileId": String(fileId),
                "message": backendStatusDetail(result.message, qsTr("Operation rejected by backend."))
            })
            batchDeleteFailures = failures
        }
        batchDeleteCurrentId = ""
        Qt.callLater(root.runNextBatchDelete)
    }

    function finishBatchDelete() {
        var failedCount = batchDeleteFailures.length
        batchDeleteRunning = false
        root.loading = false
        batchDeleteCurrentId = ""

        if (batchDeleteSucceeded === batchDeleteTotal) {
            batchDeleteSummaryMessage = qsTr("Deleted %1 file(s).")
                    .arg(String(batchDeleteSucceeded))
            batchDeleteSummarySeverity = "success"
        } else if (batchDeleteSucceeded > 0) {
            batchDeleteSummaryMessage = qsTr("Deleted %1 of %2 file(s). %3 failed.")
                    .arg(String(batchDeleteSucceeded))
                    .arg(String(batchDeleteTotal))
                    .arg(String(failedCount))
            batchDeleteSummarySeverity = "warn"
        } else {
            batchDeleteSummaryMessage = qsTr("Unable to delete %1 selected file(s).")
                    .arg(String(batchDeleteTotal))
            batchDeleteSummarySeverity = "error"
        }

        if (batchDeleteSucceeded <= 0) {
            root.statusMsg = batchDeleteSummaryMessage
            root.statusSev = batchDeleteSummarySeverity
            return
        }

        batchDeleteSummaryPending = true
        batchDeleteAwaitingCloudRefresh = usesBackgroundFilesRefresh()
        loadFiles()
    }

    function applyBatchDeleteSummary() {
        if (!batchDeleteSummaryPending)
            return
        root.statusMsg = batchDeleteSummaryMessage
        root.statusSev = batchDeleteSummarySeverity
        batchDeleteSummaryPending = false
        batchDeleteAwaitingCloudRefresh = false
    }

    function runDelete(fileId) {
        if (!hasCloudBridge() || typeof cloudBridge.deleteFile !== "function") {
            root.statusMsg = qsTr("Delete unavailable without backend.")
            root.statusSev = "warn"
            return
        }

        root.loading = true
        root.statusMsg = qsTr("Deleting file...")
        root.statusSev = "info"
        if (typeof cloudBridge.deleteFileAsync === "function") {
            cloudBridge.deleteFileAsync(String(fileId))
            return
        }

        var r = cloudBridge.deleteFile(String(fileId))
        root.loading = false
        applyDeleteResult(String(fileId), r)
    }

    function applyDeleteResult(fileId, r) {
        if (r.ok === true) {
            removeFileSelection(fileId)
            root.statusMsg = qsTr("File deleted.")
            root.statusSev = "success"
            loadFiles()
        } else {
            root.statusMsg = qsTr("Delete failed: %1")
                    .arg(backendStatusDetail(r.message, qsTr("Operation rejected by backend.")))
            root.statusSev = "error"
        }
    }

    function requestRename(fileId, fileName) {
        root.statusMsg = qsTr("Rename not implemented yet for %1")
                .arg(String(fileName || fileId))
        root.statusSev = "warn"
    }

    function requestPrint(fileId, fileName) {
        var normalizedFileId = String(fileId || "").trim()
        var normalizedFileName = String(fileName || "").trim()
        if (normalizedFileId.length === 0) {
            root.statusMsg = qsTr("Cannot start remote print: missing file id.")
            root.statusSev = "warn"
            return
        }

        root.statusMsg = qsTr("Preparing print setup for %1...")
                .arg(normalizedFileName.length > 0 ? normalizedFileName : normalizedFileId)
        root.statusSev = "info"
        root.printIntentRequested(normalizedFileId, normalizedFileName)
    }

    function pickUploadFile() {
        if (uploadOverlay.visible)
            return
        uploadFileDialog.currentFolder = pathToFileUrl(uploadLastFolderPath)
        uploadFileDialog.open()
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
        var normalizedGcodeId = String(gcodeId || "").trim()
        return Number(uploadStatus) === 1
                || (normalizedGcodeId.length > 0 && normalizedGcodeId !== "0")
    }

    function beginUpload(selectedInput, completePwszPreview2) {
        var fileName = uploadInputToDisplayName(selectedInput)
        if (typeof cloudBridge.startUploadLocalFile === "function") {
            uploadOverlay.fileName = fileName
            uploadOverlay.phase = qsTr("Starting upload...")
            uploadOverlay.progress = 0
            uploadOverlay.visible = true
            root.statusMsg = qsTr("Uploading %1...").arg(fileName)
            root.statusSev = "info"
            cloudBridge.startUploadLocalFile(selectedInput, completePwszPreview2 === true)
            return
        }

        if (typeof cloudBridge.uploadLocalFile === "function") {
            root.loading = true
            root.statusMsg = qsTr("Uploading %1...").arg(fileName)
            root.statusSev = "info"
            var r = cloudBridge.uploadLocalFile(selectedInput, completePwszPreview2 === true)
            root.loading = false

            if (r.ok === true) {
                var backendMessage = sanitizedBackendMessage(r.message)
                var ready = uploadIsReady(r.uploadStatus, r.gcodeId)
                root.statusMsg = backendMessage.length > 0
                        ? backendMessage
                        : (ready ? qsTr("Uploaded: %1").arg(fileName)
                                 : qsTr("Upload transferred. Cloud processing in progress."))
                root.statusSev = (!ready || r.unlockOk === false || r.localFileSynchronized === false)
                        ? "warn" : "success"
                loadFiles()
            } else {
                root.statusMsg = qsTr("Upload failed: %1")
                        .arg(backendStatusDetail(r.message, qsTr("Transfer failed.")))
                root.statusSev = "error"
            }
            return
        }

        root.statusMsg = qsTr("Upload unavailable without backend.")
        root.statusSev = "warn"
    }

    function uploadSelectedLocalFile(fileUrl) {
        var selectedInput = String(fileUrl || "").trim()
        if (selectedInput.length === 0) {
            root.statusMsg = qsTr("No file selected.")
            root.statusSev = "warn"
            return
        }

        var sourcePath = localPathFromInput(selectedInput)
        var sourceFolder = parentFolderPath(sourcePath)
        if (sourceFolder.length > 0) {
            uploadLastFolderPath = sourceFolder
            persistUploadLastFolder(sourceFolder)
        }

        var fileName = uploadInputToDisplayName(selectedInput)
        if (!hasCloudBridge()) {
            root.statusMsg = qsTr("Selected for upload: %1").arg(fileName)
            root.statusSev = "info"
            return
        }

        var completionEnabled = booleanSetting(pwszPreviewCompletionSettingsKey, true)
        if (fileExtension(fileName) !== "pwsz" || completionEnabled !== true
                || typeof cloudBridge.inspectPwszPreview !== "function") {
            beginUpload(selectedInput, false)
            return
        }

        var inspection = cloudBridge.inspectPwszPreview(selectedInput)
        if (inspection.ok !== true) {
            root.statusMsg = qsTr("PWSZ inspection failed: %1")
                    .arg(backendStatusDetail(inspection.message, qsTr("Invalid PWSZ archive.")))
            root.statusSev = "error"
            return
        }
        if (inspection.needsCompletion !== true) {
            beginUpload(selectedInput, false)
            return
        }

        if (booleanSetting(pwszPreviewConfirmationSettingsKey, true)) {
            pwszPreviewConfirmDialog.pendingPath = selectedInput
            pwszPreviewConfirmDialog.pendingName = fileName
            pwszPreviewConfirmDialog.doNotAskAgain = false
            pwszPreviewConfirmDialog.open()
            return
        }
        beginUpload(selectedInput, true)
    }

    function loadMockFiles() {
        root.currentPage = 0
        cloudFilesModel.replaceFiles([
        {
            "fileId": "demo-001",
            "fileName": "rook_plate_v12.pwmb",
            "status": "READY",
            "statusCode": 1,
            "sizeText": "42.6 MB",
            "machine": "Photon Mono M7",
            "printers": "Photon Mono M7, Photon Mono M7 Pro",
            "material": "Eco Resin Gray",
            "uploadTime": "2026-03-05",
            "createTime": "2026-03-05",
            "updateTime": "2026-03-05",
            "printTime": "02h 15m",
            "layerThickness": "0.05 mm",
            "layers": 1850,
            "isPwmb": true,
            "resinUsage": "67 ml",
            "dimensions": "102x68x120",
            "bottomLayers": "4",
            "exposureTime": "1.5 s",
            "offTime": "0.5 s",
            "md5": "b574212e123ff9ef2db4ab9bb880a6b0",
            "downloadUrl": "https://cdn.cloud-universe.anycubic.com/file/demo/rook_plate_v12.pwmb",
            "region": "us-east-2",
            "bucket": "workbentch",
            "path": "file/demo/rook_plate_v12.pwmb",
            "thumbnailUrl": "",
            "gcodeId": "demo-gcode-001"
        },
        {
            "fileId": "demo-002",
            "fileName": "calibration_tower.pws",
            "status": "READY",
            "statusCode": 1,
            "sizeText": "11.8 MB",
            "machine": "Photon Mono M5s",
            "printers": "Photon Mono M5s",
            "material": "ABS-Like Resin",
            "uploadTime": "2026-03-05",
            "createTime": "2026-03-05",
            "updateTime": "2026-03-05",
            "printTime": "00h 48m",
            "layerThickness": "0.05 mm",
            "layers": 620,
            "isPwmb": false,
            "resinUsage": "14 ml",
            "dimensions": "35x35x80",
            "bottomLayers": "5",
            "exposureTime": "1.8 s",
            "offTime": "0.5 s",
            "md5": "ff08f1feb055fb7711bafcbe0ec55843",
            "downloadUrl": "https://cdn.cloud-universe.anycubic.com/file/demo/calibration_tower.pws",
            "region": "us-east-2",
            "bucket": "workbentch",
            "path": "file/demo/calibration_tower.pws",
            "thumbnailUrl": "",
            "gcodeId": "demo-gcode-002"
        }
        ])
        pruneFileSelection()
        root.quotaData = {
            "totalDisplay": "2.0 GB",
            "usedDisplay": "1.1 GB",
            "totalBytes": 2147483648,
            "usedBytes": 1181116006
        }
        refreshTypeFilterOptions()
        root.statusMsg = qsTr("Demo mode (backend unavailable).")
        root.statusSev = "warn"
        root.loading = false
    }

    function applyCachedQuotaResult(q) {
        if (q.ok === true && Number(q.totalBytes || 0) > 0)
            root.quotaData = q
    }

    function applyCachedFilesResult(r, useCacheFlow) {
        root.loading = false

        var files = r.files !== undefined ? r.files : []
        cloudFilesModel.replaceFiles(files)
        pruneFileSelection()
        refreshTypeFilterOptions()

        if (files.length > 0) {
            if (useCacheFlow) {
                root.statusMsg = qsTr("%1 file(s) loaded from local cache. Syncing cloud...").arg(String(files.length))
                root.statusSev = "info"
            } else {
                root.statusMsg = qsTr("%1 file(s) loaded").arg(String(files.length))
                root.statusSev = "success"
            }
        } else {
            if (useCacheFlow) {
                root.statusMsg = qsTr("No local cache yet. Syncing cloud...")
                root.statusSev = "warn"
            } else {
                root.statusMsg = qsTr("No file found.")
                root.statusSev = "warn"
            }
        }

        if (useCacheFlow) {
            Qt.callLater(function() {
                if (hasCloudBridge()
                        && typeof cloudBridge.refreshFilesAsync === "function") {
                    cloudBridge.refreshFilesAsync(1, 20, true)
                }
            })
        } else if (batchDeleteSummaryPending && !batchDeleteAwaitingCloudRefresh) {
            applyBatchDeleteSummary()
        }
    }

    function loadFiles() {
        if (root.loading)
            return

        root.currentPage = 0
        root.loading = true
        root.statusMsg = qsTr("Loading files from local cache...")
        root.statusSev = "info"

        if (!hasCloudBridge()) {
            loadMockFiles()
            return
        }

        var useCacheFlow = typeof cloudBridge.loadCachedFiles === "function"
                && typeof cloudBridge.loadCachedQuota === "function"
                && typeof cloudBridge.refreshFilesAsync === "function"
        var useAsyncCacheFlow = typeof cloudBridge.loadCachedFilesAsync === "function"
                && typeof cloudBridge.loadCachedQuotaAsync === "function"
                && typeof cloudBridge.refreshFilesAsync === "function"

        if (useAsyncCacheFlow) {
            cloudBridge.loadCachedQuotaAsync()
            cloudBridge.loadCachedFilesAsync(1, 20)
            return
        }

        var q = useCacheFlow ? cloudBridge.loadCachedQuota() : cloudBridge.fetchQuota()
        applyCachedQuotaResult(q)

        var r = useCacheFlow ? cloudBridge.loadCachedFiles(1, 20) : cloudBridge.fetchFiles(1, 20)
        applyCachedFilesResult(r, useCacheFlow)
    }

    Component.onCompleted: {
        restoreUploadLastFolderFromSettings()
        loadFiles()
    }

    Connections {
        id: cloudBridgeConnections
        property Item owner: root
        target: (typeof cloudBridge !== "undefined"
                 && cloudBridge !== null) ? cloudBridge : null
        ignoreUnknownSignals: true

        function onDownloadProgress(received, total) {
            downloadOverlay.received = received
            downloadOverlay.total = total
            if (total > 0)
                downloadOverlay.progress = received / total
            else
                downloadOverlay.progress = 0
        }

        function onDownloadFinished(ok, message, savedPath) {
            downloadOverlay.visible = false
            if (ok) {
                root.statusMsg = qsTr("Downloaded: %1").arg(String(savedPath || ""))
                root.statusSev = "success"
            } else {
                root.statusMsg = qsTr("Download error: %1")
                        .arg(root.backendStatusDetail(message, qsTr("Download failed.")))
                root.statusSev = "error"
            }
        }

        function onDownloadUrlReady(fileId, result) {
            root.applyDownloadUrlResult(fileId, result)
        }

        function onDeleteFileFinished(fileId, result) {
            if (root.batchDeleteRunning && String(fileId) === root.batchDeleteCurrentId) {
                root.handleBatchDeleteResult(fileId, result)
                return
            }
            root.loading = false
            root.applyDeleteResult(fileId, result)
        }

        function onUploadProgressChanged(progress, phase) {
            uploadOverlay.progress = Math.max(0, Math.min(1, Number(progress)))
            uploadOverlay.phase = String(phase || "")
            if (!uploadOverlay.visible)
                uploadOverlay.visible = true
        }

        function onUploadFinished(ok, message, fileId, gcodeId, uploadStatus, unlockOk, localFileSynchronized) {
            uploadOverlay.visible = false
            var pageRoot = cloudBridgeConnections.owner
            if (pageRoot === null)
                return
            var backendMessage = pageRoot.sanitizedBackendMessage(message)
            if (ok) {
                var ready = pageRoot.uploadIsReady(uploadStatus, gcodeId)
                var uploadStatusMessage = backendMessage.length > 0
                        ? backendMessage
                        : (ready ? qsTr("Upload completed.")
                                 : qsTr("Upload transferred. Cloud processing in progress."))
                var uploadStatusSeverity = (!ready || unlockOk === false || localFileSynchronized === false)
                        ? "warn" : "success"
                pageRoot.loadFiles()
                pageRoot.statusMsg = uploadStatusMessage
                pageRoot.statusSev = uploadStatusSeverity
            } else {
                pageRoot.statusMsg = qsTr("Upload failed: %1")
                        .arg(pageRoot.backendStatusDetail(message, qsTr("Transfer failed.")))
                pageRoot.statusSev = "error"
            }
        }

        function onCachedQuotaLoaded(result) {
            root.applyCachedQuotaResult(result)
        }

        function onCachedFilesLoaded(result) {
            root.applyCachedFilesResult(result, true)
        }

        function onFilesUpdatedFromCloud(files, message) {
            var list = files !== undefined ? files : []
            cloudFilesModel.replaceFiles(list)
            root.pruneFileSelection()
            root.refreshTypeFilterOptions()
            if (root.batchDeleteSummaryPending) {
                root.applyBatchDeleteSummary()
            } else {
                root.statusMsg = qsTr("%1 file(s) refreshed from cloud.").arg(String(list.length))
                root.statusSev = "success"
            }
        }

        function onPwszCloudPreviewUpdateSuggested(files, totalBytes) {
            if (root.pwszCloudUpdateRunning)
                return
            root.pwszCloudUpdateCandidates = files !== undefined ? files : []
            root.pwszCloudUpdateBytes = Number(totalBytes || 0)
            root.pwszCloudUpdateProposalPending = root.pwszCloudUpdateCandidates.length > 0
            if (root.pwszCloudUpdateProposalPending)
                pwszCloudUpdateConfirmDialog.open()
        }

        function onPwszCloudPreviewUpdateProgress(current, total, fileName, phase) {
            root.pwszCloudUpdateRunning = true
            root.pwszCloudUpdateCurrent = Number(current || 0)
            root.pwszCloudUpdateTotal = Number(total || 0)
            root.pwszCloudUpdateFileName = String(fileName || "")
            root.pwszCloudUpdatePhase = root.pwszCloudUpdatePhaseText(phase)
            pwszCloudUpdateProgressDialog.open()
        }

        function onPwszCloudPreviewUpdateFinished(summary) {
            root.pwszCloudUpdateRunning = false
            root.pwszCloudUpdateCancelRequested = false
            root.pwszCloudUpdateSummary = summary !== undefined ? summary : ({})
            pwszCloudUpdateProgressDialog.close()
            pwszCloudUpdateResultDialog.open()
        }

        function onQuotaUpdatedFromCloud(quota, message) {
            if (quota !== undefined)
                root.quotaData = quota
        }

        function onSyncFailed(scope, message) {
            var normalizedScope = String(scope)
            if (normalizedScope === "pwsz_preview_candidates") {
                root.statusMsg = qsTr("Cloud inventory incomplete; the PWSZ batch update proposal was skipped. Retry the refresh.")
                root.statusSev = "warn"
                return
            }
            if (normalizedScope !== "files" && normalizedScope !== "quota")
                return
            if (normalizedScope === "files" && root.batchDeleteSummaryPending) {
                root.statusMsg = qsTr("%1 Cloud refresh failed: %2")
                        .arg(root.batchDeleteSummaryMessage)
                        .arg(root.backendStatusDetail(message, qsTr("Retry later.")))
                root.statusSev = "warn"
                root.batchDeleteSummaryPending = false
                root.batchDeleteAwaitingCloudRefresh = false
                return
            }
            root.statusMsg = qsTr("Background sync failed (%1): %2")
                    .arg(normalizedScope)
                    .arg(root.backendStatusDetail(message, qsTr("Retry later.")))
            root.statusSev = "warn"
        }
    }


    AppDialogFrame {
        id: pwszPreviewConfirmDialog
        property string pendingPath: ""
        property string pendingName: ""
        property bool doNotAskAgain: false
        title: qsTr("Missing PWSZ cloud preview")
        subtitle: qsTr("The local file will be modified only after the cloud upload succeeds.")
        allowScrimClose: false
        minimumWidth: 620
        maximumWidth: 720

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.gapRow

            Text {
                Layout.fillWidth: true
                text: qsTr("%1 does not contain preview_images/preview_2.png. ACM will copy preview_1.png to preview_2.png without changing its bytes, upload that version, then replace the local PWSZ atomically after a successful cloud upload.").arg(pwszPreviewConfirmDialog.pendingName)
                color: Theme.fgPrimary
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontBodyPx
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("If preparation or upload fails, the original local file remains unchanged.")
                color: Theme.fgSecondary
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontCaptionPx
            }

            AppCheckBox {
                text: qsTr("Do not ask again for future PWSZ files")
                checked: pwszPreviewConfirmDialog.doNotAskAgain
                onToggled: pwszPreviewConfirmDialog.doNotAskAgain = checked
            }
        }

        footerTrailingData: [
            AppButton {
                text: qsTr("Cancel")
                variant: "secondary"
                onClicked: pwszPreviewConfirmDialog.close()
            },
            AppButton {
                text: qsTr("Modify and upload")
                variant: "primary"
                onClicked: {
                    var path = pwszPreviewConfirmDialog.pendingPath
                    if (pwszPreviewConfirmDialog.doNotAskAgain)
                        root.persistBooleanSetting(root.pwszPreviewConfirmationSettingsKey, false)
                    pwszPreviewConfirmDialog.close()
                    root.beginUpload(path, true)
                }
            }
        ]
    }

    AppDialogFrame {
        id: pwszCloudUpdateConfirmDialog
        title: qsTr("Missing PWSZ thumbnails")
        onOpened: {
            root.pwszCloudUpdateProposalOpened = true
            root.pwszCloudUpdateProposalPending = false
        }
        onClosed: {
            if (root.pwszCloudUpdateProposalOpened)
                root.pwszCloudUpdateProposalPending = false
            root.pwszCloudUpdateProposalOpened = false
        }
        subtitle: qsTr("Update the affected cloud files")
        allowScrimClose: false
        minimumWidth: 650
        maximumWidth: 780

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.gapRow

            Text {
                Layout.fillWidth: true
                text: qsTr("ACM detected %1 PWSZ file(s) whose Anycubic thumbnail is empty.")
                        .arg(String(root.pwszCloudUpdateCandidates.length))
                color: Theme.fgPrimary
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontBodyPx
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("To update them, ACM will download every affected file (%1), add preview_images/preview_2.png by copying preview_1.png, upload a new normal cloud version, validate its thumbnail, then delete the old cloud version.")
                        .arg(root.formatBytes(root.pwszCloudUpdateBytes))
                color: Theme.fgSecondary
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontCaptionPx
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("The new version is registered directly with the original display name. The old version is deleted only after the new thumbnail is valid. If deletion fails, both versions are kept and the modification is reported as partial.")
                color: Theme.warning
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontCaptionPx
            }
        }

        footerTrailingData: [
            AppButton {
                text: qsTr("Cancel")
                variant: "secondary"
                onClicked: {
                    root.pwszCloudUpdateProposalPending = false
                    pwszCloudUpdateConfirmDialog.close()
                }
            },
            AppButton {
                text: qsTr("Update %1 file(s)").arg(String(root.pwszCloudUpdateCandidates.length))
                variant: "primary"
                onClicked: {
                    var files = root.pwszCloudUpdateCandidates
                    root.pwszCloudUpdateProposalPending = false
                    pwszCloudUpdateConfirmDialog.close()
                    root.pwszCloudUpdateRunning = true
                    root.pwszCloudUpdateCancelRequested = false
                    root.pwszCloudUpdateCurrent = 0
                    root.pwszCloudUpdateTotal = files.length
                    root.pwszCloudUpdatePhase = qsTr("Starting modification")
                    pwszCloudUpdateProgressDialog.open()
                    if (root.hasCloudBridge()
                            && typeof cloudBridge.startPwszCloudPreviewUpdate === "function") {
                        cloudBridge.startPwszCloudPreviewUpdate(files)
                    }
                }
            }
        ]
    }

    AppDialogFrame {
        id: pwszCloudUpdateProgressDialog
        title: qsTr("Modification in progress")
        subtitle: root.pwszCloudUpdateTotal > 0
                  ? qsTr("File %1 of %2").arg(String(root.pwszCloudUpdateCurrent)).arg(String(root.pwszCloudUpdateTotal))
                  : qsTr("Preparing files")
        allowScrimClose: false
        minimumWidth: 560
        maximumWidth: 680

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.gapRow
            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: root.pwszCloudUpdateRunning
            }
            Text {
                Layout.fillWidth: true
                text: root.pwszCloudUpdateFileName
                color: Theme.fgPrimary
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideMiddle
                font.pixelSize: Theme.fontBodyPx
            }
            Text {
                Layout.fillWidth: true
                text: root.pwszCloudUpdatePhase
                color: Theme.fgSecondary
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontCaptionPx
            }
        }

        footerTrailingData: [
            AppButton {
                objectName: "pwszCloudUpdateCancelButton"
                text: qsTr("Cancel")
                variant: "secondary"
                enabled: root.pwszCloudUpdateRunning && !root.pwszCloudUpdateCancelRequested
                onClicked: root.requestPwszCloudUpdateCancellation()
            }
        ]
    }

    AppDialogFrame {
        id: pwszCloudUpdateResultDialog
        title: root.pwszCloudUpdateResultTitle(root.pwszCloudUpdateSummary)
        subtitle: qsTr("PWSZ cloud modification summary")
        allowScrimClose: true
        minimumWidth: 560
        maximumWidth: 680

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.gapRow
            Text {
                Layout.fillWidth: true
                text: qsTr("Files modified: %1\nAlready compliant: %2\nFailures: %3\nPartial modifications: %4\nCancelled: %5")
                        .arg(String(root.pwszCloudUpdateSummary.modified || 0))
                        .arg(String(root.pwszCloudUpdateSummary.skipped || 0))
                        .arg(String(root.pwszCloudUpdateSummary.failed || 0))
                        .arg(String(root.pwszCloudUpdateSummary.partial || 0))
                        .arg(String(root.pwszCloudUpdateSummary.cancelledItems || 0))
                color: Theme.fgPrimary
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontBodyPx
            }
            TextArea {
                id: pwszCloudUpdateResultDetails
                objectName: "pwszCloudUpdateResultDetails"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(240, Math.max(80, contentHeight + 20))
                visible: text.length > 0
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                text: root.pwszCloudUpdateIssueDetails(root.pwszCloudUpdateSummary)
                font.pixelSize: Theme.fontCaptionPx
            }
        }

        footerTrailingData: [
            AppButton {
                text: qsTr("Close")
                variant: "primary"
                onClicked: pwszCloudUpdateResultDialog.close()
            }
        ]
    }

    AppDialogFrame {
        id: deleteConfirmDialog
        property string pendingId: ""
        property string pendingName: ""
        title: qsTr("Delete File")
        subtitle: qsTr("This action is irreversible.")
        allowScrimClose: false
        minimumWidth: 520
        maximumWidth: 620

        Text {
            Layout.fillWidth: true
            text: qsTr("Delete permanently \"%1\"?").arg(deleteConfirmDialog.pendingName)
            color: Theme.fgPrimary
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontBodyPx
        }

        footerTrailingData: [
            AppButton {
                text: qsTr("Cancel")
                variant: "secondary"
                onClicked: deleteConfirmDialog.close()
            },
            AppButton {
                text: qsTr("Delete")
                variant: "danger"
                onClicked: {
                    var fileId = deleteConfirmDialog.pendingId
                    deleteConfirmDialog.close()
                    root.runDelete(fileId)
                }
            }
        ]
    }

    AppDialogFrame {
        id: batchDeleteConfirmDialog
        title: qsTr("Delete selected files")
        subtitle: qsTr("This action is irreversible.")
        allowScrimClose: false
        minimumWidth: 520
        maximumWidth: 620

        Text {
            Layout.fillWidth: true
            text: qsTr("Delete permanently %1 selected file(s)?")
                    .arg(String(root.selectedFilesCount))
            color: Theme.fgPrimary
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontBodyPx
        }

        footerTrailingData: [
            AppButton {
                text: qsTr("Cancel")
                variant: "secondary"
                onClicked: batchDeleteConfirmDialog.close()
            },
            AppButton {
                objectName: "confirmDeleteSelectedFilesButton"
                text: qsTr("Delete")
                variant: "danger"
                enabled: root.selectedFilesCount > 0 && !root.batchDeleteRunning
                onClicked: {
                    batchDeleteConfirmDialog.close()
                    root.startBatchDelete()
                }
            }
        ]
    }

    CloudFileDetailsDialog {
        id: fileDetailsDialog
        fileData: ({})
        buildDebugEnabled: root.buildDebugEnabled
        showAdvancedDetails: root.showAdvancedDetails
        fileTypeLabelProvider: root.fileTypeLabel
        fileNameWithoutExtensionProvider: root.fileNameWithoutExtension
        displayDateProvider: root.displayDate
        onRenameRequested: function(fileId, fileName) {
            root.requestRename(fileId, fileName)
        }
        onDeleteRequested: function(fileId, fileName) {
            fileDetailsDialog.close()
            root.requestDelete(fileId, fileName)
        }
        onDownloadRequested: function(fileId, fileName) {
            root.requestDownload(fileId, fileName)
        }
        onPrintRequested: function(fileId, fileName) {
            root.requestPrint(fileId, fileName)
        }
        onCloseRequested: fileDetailsDialog.close()
    }

    UploadFileDialog {
        id: uploadFileDialog
        currentFolder: root.pathToFileUrl(root.uploadLastFolderPath)
        nameFilters: [
            qsTr("Slice files (*.photon *.pws *.pwsz *.photons *.pw0 *.pwx *.pwmo *.pwma *.pwms *.pwmx *.pmx2 *.pmsq *.dlp *.dl2p *.pwmb *.pm3 *.pm3m *.pm3r *.pm3n *.px6s *.pm5 *.pm5s *.m5sp)"),
            qsTr("All files (*)")
        ]
        onFileChosen: function(file) {
            root.uploadSelectedLocalFile(file)
        }
        onCancelled: {}
    }

    Connections {
        target: uploadFileDialog
        ignoreUnknownSignals: true

        function onCurrentFolderChanged() {
            var folder = root.localPathFromInput(uploadFileDialog.currentFolder)
            if (folder.length <= 0)
                return
            root.uploadLastFolderPath = folder
            root.persistUploadLastFolder(folder)
        }
    }

    FileDialog {
        id: saveDialog
        property string pendingUrl: ""
        property string suggestName: "file"
        readonly property var downloadFolders: StandardPaths.standardLocations(StandardPaths.DownloadLocation)
        title: qsTr("Save As")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("All files (*)")]
        currentFolder: downloadFolders.length > 0
                     ? downloadFolders[0]
                     : StandardPaths.writableLocation(StandardPaths.HomeLocation)

        onAccepted: {
            if (!hasCloudBridge() || typeof cloudBridge.startDownload !== "function") {
                root.statusMsg = qsTr("Download unavailable without backend.")
                root.statusSev = "warn"
                return
            }
            var dest = String(selectedFile).replace(/^file:\/\//, "")
            downloadOverlay.visible = true
            downloadOverlay.progress = 0
            downloadOverlay.received = 0
            downloadOverlay.total = 0
            cloudBridge.startDownload(pendingUrl, dest)
        }
    }

    Rectangle {
        id: uploadOverlay
        anchors.fill: parent
        color: Theme.overlayScrim
        visible: false
        z: 21

        property real progress: 0
        property string phase: ""
        property string fileName: ""

        Rectangle {
            anchors.centerIn: parent
            width: 420
            height: 180
            radius: Theme.radiusDialog
            color: Theme.bgDialog
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingDialog
                spacing: Theme.gapRow

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Uploading file...")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontSectionPx
                    font.bold: true
                }

                Text {
                    Layout.fillWidth: true
                    text: uploadOverlay.fileName
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontCaptionPx
                    elide: Text.ElideMiddle
                }

                ProgressBar {
                    from: 0
                    to: 1
                    value: uploadOverlay.progress
                    Layout.fillWidth: true
                }

                Text {
                    Layout.fillWidth: true
                    text: uploadOverlay.phase.length > 0
                          ? uploadOverlay.phase
                          : qsTr("Preparing upload...")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontCaptionPx
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Rectangle {
        id: downloadOverlay
        anchors.fill: parent
        color: Theme.overlayScrim
        visible: false
        z: 20

        property real progress: 0
        property real received: 0
        property real total: 0

        Rectangle {
            anchors.centerIn: parent
            width: 380
            height: 158
            radius: Theme.radiusDialog
            color: Theme.bgDialog
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingDialog
                spacing: Theme.gapRow

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Downloading file...")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontSectionPx
                    font.bold: true
                }

                ProgressBar {
                    id: dlBar
                    from: 0
                    to: 1
                    value: downloadOverlay.total > 0 ? downloadOverlay.progress : 0
                    indeterminate: downloadOverlay.total <= 0
                    Layout.fillWidth: true
                }

                Text {
                    text: downloadOverlay.total > 0
                          ? (Math.round(downloadOverlay.received / 1048576)
                              + qsTr(" MB / ")
                             + Math.round(downloadOverlay.total / 1048576)
                              + qsTr(" MB"))
                          : qsTr("Connecting...")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontCaptionPx
                }

                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    AppButton {
                        text: qsTr("Cancel")
                        variant: "danger"
                        onClicked: {
                            if (hasCloudBridge() && typeof cloudBridge.cancelDownload === "function")
                                cloudBridge.cancelDownload()
                            else
                                downloadOverlay.visible = false
                        }
                    }
                }
            }
        }
    }

    AppPageFrame {
        id: pageFrame
        anchors.fill: parent
        embeddedInTabsContainer: root.embeddedInTabsContainer
        contentPadding: root.pageContentPadding

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            CloudFilesToolbar {
                Layout.fillWidth: false
                Layout.preferredWidth: 520
                loading: root.loading
                selectedFilesCount: root.selectedFilesCount
                batchDeleteRunning: root.batchDeleteRunning
                batchDeleteCompleted: root.batchDeleteCompleted
                batchDeleteTotal: root.batchDeleteTotal
                typeFilterOptions: root.typeFilterOptions
                typeFilterCurrentIndex: root.typeFilterIndex()
                onRefreshRequested: {
                    if (!root.hasCloudBridge()) {
                        root.loadFiles()
                        return
                    }
                    if (typeof cloudBridge.refreshFilesAsync === "function") {
                        root.statusMsg = qsTr("Force refresh from cloud...")
                        root.statusSev = "info"
                        cloudBridge.refreshFilesAsync(1, 20, true)
                    } else {
                        root.loadFiles()
                    }
                }
                onUploadRequested: {
                    root.pickUploadFile()
                }
                onDeleteSelectedRequested: root.requestDeleteSelected()
                onTypeFilterSelected: function(index, code) {
                    root.typeFilterValue = code
                    root.currentPage = 0
                }
            }

            CloudQuotaCard {
                Layout.fillWidth: true
                usedText: root.quotaUsedText()
                freeText: root.quotaFreeText()
                filesCount: root.visibleFileCount()
                usedRatio: root.quotaRatio()
                backgroundColor: root.quotaBackgroundColor()
                barColor: root.quotaBarColor()
            }
        }

        CloudFilesTablePanel {
            id: filesTablePanel
            loading: root.loading
            filesModel: cloudFilesModel
            selectedFileId: root.selectedFileId
            selectedFiles: root.selectedFiles
            batchDeleteRunning: root.batchDeleteRunning
            tableRowHorizontalMargin: root.tableRowHorizontalMargin
            scrollbarReserve: root.tableScrollbarReserve
            tableViewportWidth: root.tableViewportWidth
            colXSelect: root.colXSelect
            colSelectWidth: root.colSelectWidth
            colXThumb: root.colXThumb
            colThumbWidth: root.colThumbWidth
            colXName: root.colXName
            colNameWidth: root.colNameWidth
            colXType: root.colXType
            colTypeWidth: root.colTypeWidth
            colXSize: root.colXSize
            colSizeWidth: root.colSizeWidth
            colXDate: root.colXDate
            colDateWidth: root.colDateWidth
            colXActions: root.colXActions
            colActionsWidth: root.colActionsWidth
            actionDetailsWidth: root.actionDetailsWidth
            actionDownloadWidth: root.actionDownloadWidth
            actionPrintWidth: root.actionPrintWidth
            actionMenuWidth: root.actionMenuWidth
            currentPage: root.currentPage
            totalPages: root.pageTotal
            visibleCount: root.visibleFilesCount
            pageSize: root.pageSize
            onSelectedFileChanged: function(fileId) { root.selectedFileId = fileId }
            onFileSelectionToggled: function(fileId, fileName, checked) {
                root.setFileSelected(fileId, fileName, checked)
            }
            onDetailsRequested: function(fileId) { root.openFileDetails(fileId) }
            onDownloadRequested: function(fileId, fileName) { root.requestDownload(fileId, fileName) }
            onPrintRequested: function(fileId, fileName) { root.requestPrint(fileId, fileName) }
            onRenameRequested: function(fileId, fileName) { root.requestRename(fileId, fileName) }
            onDeleteRequested: function(fileId, fileName) { root.requestDelete(fileId, fileName) }
            onPageSizeSelected: function(value) {
                root.pageSize = value
                root.currentPage = 0
            }
            onPreviousPageRequested: {
                root.currentPage = Math.max(0, root.currentPage - 1)
            }
            onNextPageRequested: {
                root.currentPage = Math.min(root.totalPages() - 1, root.currentPage + 1)
            }
        }
    }

    // Hidden ListView kept for test compatibility.
    ListView {
        id: hiddenListForTests
        objectName: "filesListView"
        visible: false
        model: cloudFilesModel
    }
}
