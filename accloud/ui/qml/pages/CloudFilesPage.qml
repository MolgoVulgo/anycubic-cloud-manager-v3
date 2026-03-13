import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs
import QtQuick.Layouts 1.15
import QtCore
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

    // UI state
    property bool loading: false
    property string statusMsg: qsTr("Ready.")
    property string statusSev: "info" // info | success | warn | error
    property var quotaData: null
    property string typeFilterValue: "all"
    property var typeFilterOptions: [
        { "code": "all", "label": qsTr("All") }
    ]
    property string selectedFileId: ""
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
    property int colThumbWidth: 100
    property int colTypeWidth: 72
    property int colSizeWidth: 86
    property int colDateWidth: 92
    property int colActionsWidth: 392
    readonly property int tableRowHorizontalMargin: 8
    readonly property int tableColumnSpacing: 8
    readonly property int actionDetailsWidth: 92
    readonly property int actionDownloadWidth: 112
    readonly property int actionPrintWidth: 96
    readonly property int actionMenuWidth: 42
    readonly property int tableFixedColumnsWidth: colThumbWidth + colTypeWidth + colSizeWidth + colDateWidth + colActionsWidth + tableColumnSpacing * 5
    readonly property int colNameWidth: Math.max(220, tableViewportWidth - tableFixedColumnsWidth)
    property int pageSize: 10
    property int currentPage: 0
    readonly property int tableViewportWidth: Math.max(0, pageFrame.width - (Theme.paddingPage * 2) - (tableRowHorizontalMargin * 2) - 24)
    readonly property int colXThumb: 0
    readonly property int colXName: colXThumb + colThumbWidth + tableColumnSpacing
    readonly property int colXType: colXName + colNameWidth + tableColumnSpacing
    readonly property int colXSize: colXType + colTypeWidth + tableColumnSpacing
    readonly property int colXDate: colXSize + colSizeWidth + tableColumnSpacing
    readonly property int colXActions: colXDate + colDateWidth + tableColumnSpacing

    ListModel {
        id: cloudFilesModel
    }

    function hasCloudBridge() {
        return (typeof cloudBridge !== "undefined")
                && cloudBridge !== null
                && typeof cloudBridge.fetchFiles === "function"
                && typeof cloudBridge.fetchQuota === "function"
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
        var present = {}
        for (var i = 0; i < cloudFilesModel.count; ++i) {
            var entry = cloudFilesModel.get(i)
            var ext = fileExtension(entry.fileName)
            if (ext.length > 0 && isSupportedExtension(ext))
                present[ext] = true
        }

        var sortedExt = []
        for (var key in present)
            sortedExt.push(key)
        sortedExt.sort()

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
        var families = {
            "pm3": "Photon Mono 3, Mono 3 Ultra",
            "pm3m": "Photon Mono 3 Max",
            "pm3r": "Photon Mono 3 series",
            "pm3n": "Photon Mono 3 series",
            "pm5": "Photon Mono M5",
            "pm5s": "Photon Mono M5s",
            "m5sp": "Photon Mono M5s Pro",
            "px6s": "Photon Mono X 6Ks",
            "pwmb": "Modern Photon resin printers",
            "pws": "Legacy Photon resin printers",
            "pwsz": "Legacy Photon resin printers",
            "photons": "Legacy Photon resin printers",
            "photon": "Legacy Photon resin printers",
            "pw0": "Legacy Photon resin printers",
            "pwx": "Legacy Photon resin printers",
            "pwmo": "Legacy Photon resin printers",
            "pwma": "Legacy Photon resin printers",
            "pwms": "Legacy Photon resin printers",
            "pwmx": "Legacy Photon resin printers",
            "pmx2": "Photon Mono X series",
            "pmsq": "Anycubic resin printers",
            "dlp": "Anycubic DLP printers",
            "dl2p": "Anycubic DLP printers"
        }
        if (families[ext] !== undefined)
            return qsTr(families[ext])
        if (ext.length === 0)
            return qsTr("Unknown")
        return qsTr("Anycubic resin printers")
    }

    function compatiblePrintersTooltip(fileName) {
        return qsTr("Compatible printers: %1").arg(compatiblePrintersLabel(fileName))
    }

    function visibleFileCount() {
        var count = 0
        for (var i = 0; i < cloudFilesModel.count; ++i) {
            var entry = cloudFilesModel.get(i)
            if (fileMatchesFilter(entry.fileName))
                count += 1
        }
        return count
    }

    function totalPages() {
        var total = visibleFileCount()
        if (total <= 0)
            return 1
        return Math.max(1, Math.ceil(total / pageSize))
    }

    function clampCurrentPage() {
        currentPage = Math.max(0, Math.min(currentPage, totalPages() - 1))
    }

    function visibleRankForIndex(modelIndex) {
        var rank = 0
        for (var i = 0; i < modelIndex; ++i) {
            var entry = cloudFilesModel.get(i)
            if (fileMatchesFilter(entry.fileName))
                rank += 1
        }
        return rank
    }

    function isIndexOnCurrentPage(modelIndex, fileName) {
        if (!fileMatchesFilter(fileName))
            return false
        var rank = visibleRankForIndex(modelIndex)
        var start = currentPage * pageSize
        var end = start + pageSize
        return rank >= start && rank < end
    }

    onPageSizeChanged: clampCurrentPage()
    onTypeFilterValueChanged: clampCurrentPage()

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
        for (var i = 0; i < cloudFilesModel.count; ++i) {
            var entry = cloudFilesModel.get(i)
            if (String(entry.fileId) === String(fileId))
                return entry
        }
        return null
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

        var r = cloudBridge.getDownloadUrl(String(fileId))
        if (r.ok !== true) {
            root.statusMsg = qsTr("Cannot get download URL: ") + String(r.message)
            root.statusSev = "error"
            return
        }

        saveDialog.pendingUrl = r.url
        saveDialog.suggestName = String(fileName || qsTr("file"))
        saveDialog.defaultSuffix = String(fileExtension(fileName) || "file")
        saveDialog.open()
    }

    function requestDelete(fileId, fileName) {
        deleteConfirmDialog.pendingId = String(fileId)
        deleteConfirmDialog.pendingName = String(fileName || fileId)
        deleteConfirmDialog.open()
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
        var r = cloudBridge.deleteFile(String(fileId))
        root.loading = false

        if (r.ok === true) {
            root.statusMsg = qsTr("File deleted.")
            root.statusSev = "success"
            loadFiles()
        } else {
            root.statusMsg = qsTr("Delete failed: ") + String(r.message)
            root.statusSev = "error"
        }
    }

    function requestRename(fileId, fileName) {
        root.statusMsg = qsTr("Rename not implemented yet for ") + String(fileName || fileId)
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

        root.statusMsg = qsTr("Opening Printers workflow for %1...")
                .arg(normalizedFileName.length > 0 ? normalizedFileName : normalizedFileId)
        root.statusSev = "info"
        root.printIntentRequested(normalizedFileId, normalizedFileName)
    }

    function pickUploadFile() {
        if (uploadOverlay.visible)
            return
        uploadFileDialog.open()
    }

    function uploadSelectedLocalFile(fileUrl) {
        var selectedPath = decodeURIComponent(String(fileUrl || "").replace(/^file:\/\//, ""))
        if (selectedPath.length === 0) {
            root.statusMsg = qsTr("No file selected.")
            root.statusSev = "warn"
            return
        }

        var fileName = selectedPath.split("/").pop()
        if (!hasCloudBridge()) {
            root.statusMsg = qsTr("Selected for upload: %1").arg(fileName)
            root.statusSev = "info"
            return
        }

        if (typeof cloudBridge.startUploadLocalFile === "function") {
            uploadOverlay.fileName = fileName
            uploadOverlay.phase = qsTr("Starting upload...")
            uploadOverlay.progress = 0
            uploadOverlay.visible = true
            root.statusMsg = qsTr("Uploading %1...").arg(fileName)
            root.statusSev = "info"
            cloudBridge.startUploadLocalFile(selectedPath)
            return
        }

        if (typeof cloudBridge.uploadLocalFile === "function") {
            root.loading = true
            root.statusMsg = qsTr("Uploading %1...").arg(fileName)
            root.statusSev = "info"
            var r = cloudBridge.uploadLocalFile(selectedPath)
            root.loading = false

            if (r.ok === true) {
                var backendMessage = String(r.message || "").trim()
                root.statusMsg = backendMessage.length > 0
                        ? backendMessage
                        : qsTr("Uploaded: %1").arg(fileName)
                root.statusSev = (r.unlockOk === false) ? "warn" : "success"
                loadFiles()
            } else {
                root.statusMsg = qsTr("Upload failed: ") + String(r.message)
                root.statusSev = "error"
            }
            return
        }

        root.statusMsg = qsTr("Upload unavailable without backend.")
        root.statusSev = "warn"
    }

    function loadMockFiles() {
        root.currentPage = 0
        cloudFilesModel.clear()
        cloudFilesModel.append({
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
        })
        cloudFilesModel.append({
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
        })
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

        var q = useCacheFlow ? cloudBridge.loadCachedQuota() : cloudBridge.fetchQuota()
        if (q.ok === true && Number(q.totalBytes || 0) > 0)
            root.quotaData = q

        var r = useCacheFlow ? cloudBridge.loadCachedFiles(1, 20) : cloudBridge.fetchFiles(1, 20)
        root.loading = false

        cloudFilesModel.clear()
        var files = r.files !== undefined ? r.files : []
        for (var i = 0; i < files.length; ++i)
            cloudFilesModel.append(files[i])
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
        }
    }

    Component.onCompleted: loadFiles()

    Connections {
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
                root.statusMsg = qsTr("Downloaded: ") + savedPath
                root.statusSev = "success"
            } else {
                root.statusMsg = qsTr("Download error: ") + message
                root.statusSev = "error"
            }
        }

        function onUploadProgressChanged(progress, phase) {
            uploadOverlay.progress = Math.max(0, Math.min(1, Number(progress)))
            uploadOverlay.phase = String(phase || "")
            if (!uploadOverlay.visible)
                uploadOverlay.visible = true
        }

        function onUploadFinished(ok, message, fileId, gcodeId, uploadStatus, unlockOk) {
            uploadOverlay.visible = false
            var backendMessage = String(message || "").trim()
            if (ok) {
                root.statusMsg = backendMessage.length > 0
                        ? backendMessage
                        : qsTr("Upload completed.")
                root.statusSev = unlockOk === false ? "warn" : "success"
                root.loadFiles()
            } else {
                root.statusMsg = qsTr("Upload failed: ") + backendMessage
                root.statusSev = "error"
            }
        }

        function onFilesUpdatedFromCloud(files, message) {
            cloudFilesModel.clear()
            var list = files !== undefined ? files : []
            for (var i = 0; i < list.length; ++i)
                cloudFilesModel.append(list[i])
            refreshTypeFilterOptions()
            root.currentPage = Math.min(root.currentPage, root.totalPages() - 1)
            root.statusMsg = qsTr("%1 file(s) refreshed from cloud.").arg(String(list.length))
            root.statusSev = "success"
        }

        function onQuotaUpdatedFromCloud(quota, message) {
            if (quota !== undefined)
                root.quotaData = quota
        }

        function onSyncFailed(scope, message) {
            if (String(scope) !== "files" && String(scope) !== "quota")
                return
            root.statusMsg = qsTr("Background sync failed (%1): %2")
                    .arg(String(scope))
                    .arg(String(message))
            root.statusSev = "warn"
        }
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

    CloudFileDetailsDialog {
        id: fileDetailsDialog
        fileData: ({})
        buildDebugEnabled: root.buildDebugEnabled
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

    FileDialog {
        id: uploadFileDialog
        title: qsTr("Select file to upload")
        fileMode: FileDialog.OpenFile
        options: FileDialog.DontUseNativeDialog
        nameFilters: [
            qsTr("Slice files (*.photon *.pws *.pwsz *.photons *.pw0 *.pwx *.pwmo *.pwma *.pwms *.pwmx *.pmx2 *.pmsq *.dlp *.dl2p *.pwmb *.pm3 *.pm3m *.pm3r *.pm3n *.px6s *.pm5 *.pm5s *.m5sp)"),
            qsTr("All files (*)")
        ]
        currentFolder: StandardPaths.writableLocation(StandardPaths.HomeLocation)

        onAccepted: {
            root.uploadSelectedLocalFile(selectedFile)
        }
    }

    FileDialog {
        id: saveDialog
        property string pendingUrl: ""
        property string suggestName: "file"
        readonly property var downloadFolders: StandardPaths.standardLocations(StandardPaths.DownloadLocation)
        title: qsTr("Save As")
        fileMode: FileDialog.SaveFile
        nameFilters: ["All files (*)"]
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

        CloudFilesToolbar {
            loading: root.loading
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
            onTypeFilterSelected: function(index, code) {
                root.typeFilterValue = code
                root.currentPage = 0
            }
        }

        CloudQuotaCard {
            usedText: root.quotaUsedText()
            freeText: root.quotaFreeText()
            filesCount: root.visibleFileCount()
            usedRatio: root.quotaRatio()
            backgroundColor: root.quotaBackgroundColor()
            barColor: root.quotaBarColor()
        }

        CloudFilesTablePanel {
            loading: root.loading
            filesModel: cloudFilesModel
            selectedFileId: root.selectedFileId
            tableRowHorizontalMargin: root.tableRowHorizontalMargin
            tableViewportWidth: root.tableViewportWidth
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
            totalPages: root.totalPages()
            visibleCount: root.visibleFileCount()
            pageSize: root.pageSize
            isIndexOnCurrentPageProvider: root.isIndexOnCurrentPage
            fileTypeLabelProvider: root.fileTypeLabel
            displayDateProvider: root.displayDate
            onSelectedFileChanged: function(fileId) { root.selectedFileId = fileId }
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
