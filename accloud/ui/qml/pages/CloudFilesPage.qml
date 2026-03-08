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
    readonly property bool buildDebugEnabled: (typeof accloudBuildDebugEnabled !== "undefined")
                                             && accloudBuildDebugEnabled === true
    property alias filesModel: cloudFilesModel
    signal statusBroadcast(string message, string severity, string operationId)

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
    readonly property int tableViewportWidth: Math.max(0, filesList.width - tableRowHorizontalMargin * 2 - (filesVBar.visible ? filesVBar.width : 0))
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
        return "#2f6ecb"
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
        root.statusMsg = qsTr("Open Printers > Details to start remote print for ") + String(fileName || fileId)
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

        if (useCacheFlow)
            cloudBridge.refreshFilesAsync(1, 20, true)
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

    AppDialogFrame {
        id: fileDetailsDialog
        property var fileData: ({})
        title: root.fileNameWithoutExtension(fileDetailsDialog.fileData.fileName)
        subtitle: qsTr("ID: %1 | status_code: %2 | gcode_id: %3")
                      .arg(String(fileDetailsDialog.fileData.fileId || "-"))
                      .arg(String(fileDetailsDialog.fileData.statusCode || "-"))
                      .arg(String(fileDetailsDialog.fileData.gcodeId || "-"))
        minimumWidth: 860
        maximumWidth: 1020
        minimumHeight: 620
        maximumHeight: 920

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapRow

            Rectangle {
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                radius: Theme.radiusControl
                color: Theme.accentSoft
                border.width: Theme.borderWidth
                border.color: Theme.borderDefault

                Text {
                    anchors.centerIn: parent
                    text: root.fileTypeLabel(fileDetailsDialog.fileData.fileName || "")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontCaptionPx
                    font.bold: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: String(fileDetailsDialog.fileData.fileName || "-")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontTitlePx
                    font.bold: true
                    elide: Text.ElideRight
                }

            }

            AppButton {
                text: qsTr("Rename")
                variant: "secondary"
                onClicked: root.requestRename(fileDetailsDialog.fileData.fileId,
                                              fileDetailsDialog.fileData.fileName)
            }
        }

        AppTabBar {
            id: detailsTabBar
            Layout.fillWidth: true

            AppTabButton { text: qsTr("Basic Information") }
            AppTabButton { text: qsTr("Slice Settings") }
            AppTabButton { text: qsTr("Cloud Metadata"); visible: root.buildDebugEnabled }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: detailsTabBar.currentIndex

            Flickable {
                clip: true
                contentWidth: width
                contentHeight: basicInfoColumn.implicitHeight

                ColumnLayout {
                    id: basicInfoColumn
                    width: parent.width
                    spacing: 8

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 20
                        rowSpacing: 8

                        Text { text: qsTr("File name"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text {
                            text: String(fileDetailsDialog.fileData.fileName || "-")
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.fontBodyPx
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text { text: qsTr("Type"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: root.fileTypeLabel(fileDetailsDialog.fileData.fileName || ""); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Size"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.sizeText || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Date"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: root.displayDate(fileDetailsDialog.fileData.uploadTime); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("status_code"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.statusCode || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("gcode_id"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.gcodeId || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }
                    }
                }
            }

            Flickable {
                clip: true
                contentWidth: width
                contentHeight: sliceColumn.implicitHeight

                ColumnLayout {
                    id: sliceColumn
                    width: parent.width
                    spacing: 8

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 20
                        rowSpacing: 8

                        Text { text: qsTr("Machine"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.machine || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Material"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.material || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Print time"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.printTime || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Layer thickness"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.layerThickness || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Layers"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.layers || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Resin usage"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.resinUsage || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Dimensions"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.dimensions || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Bottom layers"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.bottomLayers || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Exposure time"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.exposureTime || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Off time"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.offTime || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Printers"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text {
                            text: String(fileDetailsDialog.fileData.printers || "-")
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.fontBodyPx
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Text { text: qsTr("Slice md5"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text {
                            text: String(fileDetailsDialog.fileData.md5 || "-")
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.fontBodyPx
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Flickable {
                visible: root.buildDebugEnabled
                clip: true
                contentWidth: width
                contentHeight: cloudColumn.implicitHeight

                ColumnLayout {
                    id: cloudColumn
                    width: parent.width
                    spacing: 8

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 20
                        rowSpacing: 8

                        Text { text: qsTr("Uploaded"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: root.displayDate(fileDetailsDialog.fileData.uploadTime); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Created"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: root.displayDate(fileDetailsDialog.fileData.createTime); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Updated"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: root.displayDate(fileDetailsDialog.fileData.updateTime); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Thumbnail URL"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text {
                            text: String(fileDetailsDialog.fileData.thumbnailUrl || "-")
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.fontBodyPx
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }

                        Text { text: qsTr("Download URL"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text {
                            text: String(fileDetailsDialog.fileData.downloadUrl || "-")
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.fontBodyPx
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }

                        Text { text: qsTr("Region"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.region || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Bucket"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text { text: String(fileDetailsDialog.fileData.bucket || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                        Text { text: qsTr("Path"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text {
                            text: String(fileDetailsDialog.fileData.path || "-")
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.fontBodyPx
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        footerLeadingData: [
            AppButton {
                text: qsTr("Delete")
                variant: "danger"
                onClicked: {
                    var fileId = String(fileDetailsDialog.fileData.fileId || "")
                    var fileName = String(fileDetailsDialog.fileData.fileName || fileId)
                    fileDetailsDialog.close()
                    root.requestDelete(fileId, fileName)
                }
            }
        ]

        footerTrailingData: [
            AppButton {
                text: qsTr("Download")
                variant: "secondary"
                onClicked: root.requestDownload(fileDetailsDialog.fileData.fileId,
                                                fileDetailsDialog.fileData.fileName)
            },
            AppButton {
                text: qsTr("Print")
                variant: "primary"
                onClicked: root.requestPrint(fileDetailsDialog.fileData.fileId,
                                             fileDetailsDialog.fileData.fileName)
            },
            AppButton {
                text: qsTr("Close")
                variant: "secondary"
                onClicked: fileDetailsDialog.close()
            }
        ]
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            AppButton {
                id: refreshFilesButton
                objectName: "refreshFilesButton"
                text: root.loading ? qsTr("Loading...") : qsTr("Refresh")
                variant: "secondary"
                enabled: !root.loading
                onClicked: {
                    if (!hasCloudBridge()) {
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
            }

            AppButton {
                id: uploadPwmbButton
                objectName: "uploadPwmbButton"
                text: qsTr("Upload")
                variant: "primary"
                onClicked: {
                    root.statusMsg = qsTr("Upload workflow will be connected in next lot.")
                    root.statusSev = "warn"
                }
            }

            Item { Layout.fillWidth: true }

            RowLayout {
                spacing: 8

                Text {
                    text: qsTr("Type")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontBodyPx
                }

                AppComboBox {
                    id: typeFilterCombo
                    objectName: "filesTypeFilter"
                    Layout.preferredWidth: 130
                    textRole: "label"
                    model: root.typeFilterOptions
                    currentIndex: root.typeFilterIndex()
                    onActivated: {
                        if (currentIndex >= 0 && currentIndex < model.length) {
                            root.typeFilterValue = String(model[currentIndex].code)
                            root.currentPage = 0
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            radius: Theme.radiusControl
            color: Theme.bgSurface
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    Text {
                        text: root.quotaUsedText()
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontBodyPx
                        font.bold: true
                    }

                    Text {
                        text: root.quotaFreeText()
                        color: Theme.fgSecondary
                        font.pixelSize: Theme.fontCaptionPx
                    }

                    Text {
                        text: qsTr("Files ") + root.visibleFileCount()
                        color: Theme.fgSecondary
                        font.pixelSize: Theme.fontCaptionPx
                    }

                    Item { Layout.fillWidth: true }
                }

                Item {
                    objectName: "quotaFreeSpaceBar"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 12
                    readonly property real freeRatio: root.quotaFreeRatio()
                    readonly property real usedRatio: root.quotaRatio()

                    Rectangle {
                        anchors.fill: parent
                        radius: height / 2
                        color: root.quotaBackgroundColor()
                        border.width: Theme.borderWidth
                        border.color: Theme.borderDefault
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * parent.usedRatio
                        height: parent.height
                        radius: height / 2
                        color: root.quotaBarColor()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusControl
            color: Theme.bgSurface
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    color: Theme.bgSurface
                    border.width: 0

                    Item {
                        id: fileTableHeaderRow
                        objectName: "fileTableHeaderRow"
                        anchors.fill: parent
                        anchors.leftMargin: root.tableRowHorizontalMargin
                        anchors.rightMargin: root.tableRowHorizontalMargin + (filesVBar.visible ? filesVBar.width : 0)
                        clip: true

                        Text {
                            objectName: "fileHeaderThumb"
                            x: root.colXThumb
                            width: root.colThumbWidth
                            height: parent.height
                            text: qsTr("Thumb")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text {
                            objectName: "fileHeaderName"
                            x: root.colXName
                            width: root.colNameWidth
                            height: parent.height
                            text: qsTr("File name")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text {
                            objectName: "fileHeaderType"
                            x: root.colXType
                            width: root.colTypeWidth
                            height: parent.height
                            text: qsTr("Type")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text {
                            objectName: "fileHeaderSize"
                            x: root.colXSize
                            width: root.colSizeWidth
                            height: parent.height
                            text: qsTr("Size")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text {
                            objectName: "fileHeaderDate"
                            x: root.colXDate
                            width: root.colDateWidth
                            height: parent.height
                            text: qsTr("Date")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text {
                            objectName: "fileHeaderActions"
                            x: root.colXActions
                            width: root.colActionsWidth
                            height: parent.height
                            text: qsTr("Actions")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.borderWidth
                    color: Theme.borderSubtle
                }

                ListView {
                    id: filesList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 0
                    model: cloudFilesModel

                    ScrollBar.vertical: ScrollBar {
                        id: filesVBar
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        property bool rowVisible: root.isIndexOnCurrentPage(index, model.fileName || "")
                        readonly property bool rowSelected: root.selectedFileId === String(model.fileId)
                        readonly property int rowVerticalPadding: 6
                        readonly property int selectedBleedY: 3
                        width: filesList.width
                        height: rowVisible ? 112 : 0
                        visible: rowVisible
                        color: Theme.bgSurface
                        border.width: 0

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectedFileId = String(model.fileId || "")
                        }

                        Rectangle {
                            visible: parent.rowSelected
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.topMargin: parent.rowVerticalPadding - parent.selectedBleedY
                            anchors.bottomMargin: parent.rowVerticalPadding - parent.selectedBleedY
                            color: Theme.selectionBg
                            border.width: 0
                        }

                        Item {
                            objectName: "fileTableDataRow"
                            anchors.left: parent.left
                            anchors.leftMargin: root.tableRowHorizontalMargin
                            anchors.top: parent.top
                            anchors.topMargin: parent.rowVerticalPadding
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: parent.rowVerticalPadding
                            width: root.tableViewportWidth
                            clip: true

                            Rectangle {
                                objectName: "fileRowThumb"
                                x: root.colXThumb
                                width: root.colThumbWidth
                                height: root.colThumbWidth
                                radius: 6
                                color: Theme.accentSoft
                                border.width: Theme.borderWidth
                                border.color: Theme.borderDefault
                                clip: true

                                Image {
                                    id: thumbnailImage
                                    anchors.fill: parent
                                    source: String(model.thumbnailUrl || "")
                                    fillMode: Image.PreserveAspectFit
                                    visible: String(source).length > 0
                                    asynchronous: true
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: !(thumbnailImage.visible && thumbnailImage.status === Image.Ready)
                                    text: qsTr("100x100")
                                    color: Theme.fgPrimary
                                    font.pixelSize: Theme.fontCaptionPx
                                    font.bold: true
                                }
                            }

                            Item {
                                objectName: "fileRowNameCell"
                                x: root.colXName
                                width: root.colNameWidth
                                height: parent.height

                                Column {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2

                                    Text {
                                        objectName: "fileRowName"
                                        width: parent.width
                                        text: String(model.fileName || "-")
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontBodyPx
                                        elide: Text.ElideRight
                                        horizontalAlignment: Text.AlignLeft
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    Text {
                                        objectName: "fileRowThumbnailPath"
                                        width: parent.width
                                        text: String(model.thumbnailUrl || "-")
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                        elide: Text.ElideMiddle
                                        horizontalAlignment: Text.AlignLeft
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    Text {
                                        objectName: "fileRowThumbnailStatus"
                                        width: parent.width
                                        text: (thumbnailImage.status === Image.Ready
                                               ? "thumb=ready"
                                               : (thumbnailImage.status === Image.Loading
                                                  ? "thumb=loading"
                                                  : (thumbnailImage.status === Image.Error
                                                     ? "thumb=error"
                                                     : "thumb=null")))
                                              + " vis=" + (thumbnailImage.visible ? "1" : "0")
                                              + " s=" + thumbnailImage.status
                                        color: thumbnailImage.status === Image.Error
                                               ? Theme.danger
                                               : Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                        horizontalAlignment: Text.AlignLeft
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }

                            Text {
                                objectName: "fileRowType"
                                x: root.colXType
                                width: root.colTypeWidth
                                height: parent.height
                                text: root.fileTypeLabel(model.fileName || "")
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.fontBodyPx
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            Text {
                                objectName: "fileRowSize"
                                x: root.colXSize
                                width: root.colSizeWidth
                                height: parent.height
                                text: String(model.sizeText || "-")
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.fontBodyPx
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            Text {
                                objectName: "fileRowDate"
                                x: root.colXDate
                                width: root.colDateWidth
                                height: parent.height
                                text: root.displayDate(model.uploadTime)
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.fontBodyPx
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            Rectangle {
                                objectName: "fileRowActions"
                                x: root.colXActions
                                width: root.colActionsWidth
                                height: parent.height
                                color: "transparent"

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 6

                                    AppButton {
                                        text: qsTr("Details")
                                        variant: "secondary"
                                        width: root.actionDetailsWidth
                                        onClicked: root.openFileDetails(model.fileId)
                                    }

                                    AppButton {
                                        text: qsTr("Download")
                                        variant: "secondary"
                                        width: root.actionDownloadWidth
                                        onClicked: root.requestDownload(model.fileId, model.fileName)
                                    }

                                    AppButton {
                                        text: qsTr("Print")
                                        variant: "primary"
                                        width: root.actionPrintWidth
                                        onClicked: root.requestPrint(model.fileId, model.fileName)
                                    }

                                    AppButton {
                                        text: qsTr("...")
                                        variant: "secondary"
                                        compact: true
                                        width: root.actionMenuWidth
                                        onClicked: rowMenu.open()
                                    }

                                    Menu {
                                        id: rowMenu

                                        MenuItem {
                                            text: qsTr("Rename")
                                            onTriggered: root.requestRename(model.fileId, model.fileName)
                                        }

                                        MenuItem {
                                            text: qsTr("Delete")
                                            onTriggered: root.requestDelete(model.fileId, model.fileName)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: !root.loading && root.visibleFileCount() === 0
                    text: qsTr("No file matches current type filter.")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontBodyPx
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    padding: 10
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.borderWidth
                    color: Theme.borderSubtle
                    visible: root.visibleFileCount() > 0
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    radius: Theme.radiusControl
                    color: Theme.bgWindow
                    border.width: Theme.borderWidth
                    border.color: Theme.borderSubtle
                    visible: root.visibleFileCount() > 0

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Text {
                            text: qsTr("Page %1 / %2").arg(root.currentPage + 1).arg(root.totalPages())
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }

                        Text {
                            text: qsTr("Rows: %1").arg(root.visibleFileCount())
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }

                        Text {
                            text: qsTr("Rows/page")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }

                        AppComboBox {
                            id: filesRowsPerPage
                            objectName: "filesRowsPerPage"
                            Layout.preferredWidth: 88
                            textRole: "label"
                            model: [
                                { "value": 10, "label": "10" },
                                { "value": 20, "label": "20" },
                                { "value": 50, "label": "50" },
                                { "value": 100, "label": "100" }
                            ]
                            currentIndex: root.pageSize === 20 ? 1 : (root.pageSize === 50 ? 2 : (root.pageSize === 100 ? 3 : 0))
                            onActivated: {
                                if (currentIndex >= 0 && currentIndex < model.length) {
                                    root.pageSize = Number(model[currentIndex].value)
                                    root.currentPage = 0
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        AppButton {
                            text: qsTr("Previous")
                            variant: "secondary"
                            enabled: root.currentPage > 0
                            onClicked: root.currentPage = Math.max(0, root.currentPage - 1)
                        }

                        AppButton {
                            text: qsTr("Next")
                            variant: "secondary"
                            enabled: root.currentPage < root.totalPages() - 1
                            onClicked: root.currentPage = Math.min(root.totalPages() - 1, root.currentPage + 1)
                        }
                    }
                }
            }

            BusyIndicator {
                anchors.centerIn: parent
                visible: root.loading
                running: visible
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
