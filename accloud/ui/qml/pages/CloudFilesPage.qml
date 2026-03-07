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
    property alias filesModel: cloudFilesModel
    signal statusBroadcast(string message, string severity, string operationId)

    // UI state
    property bool loading: false
    property string statusMsg: qsTr("Ready.")
    property string statusSev: "info" // info | success | warn | error
    property var quotaData: null
    property string typeFilterValue: "all"
    property string selectedFileId: ""

    function emitStatusToShell() {
        var msg = String(statusMsg || "").trim()
        if (msg.length === 0)
            return
        root.statusBroadcast(msg, String(statusSev || "info"), "op_files_refresh")
    }

    onStatusMsgChanged: root.emitStatusToShell()
    onStatusSevChanged: root.emitStatusToShell()

    // Dense table column widths
    property int colCheckWidth: 32
    property int colThumbWidth: 56
    property int colTypeWidth: 72
    property int colSizeWidth: 86
    property int colDateWidth: 92
    property int colStatusWidth: 86
    property int colActionsWidth: 278

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

    function fileType(fileName) {
        var name = String(fileName || "")
        var dot = name.lastIndexOf(".")
        if (dot < 0 || dot + 1 >= name.length)
            return "other"
        var ext = name.slice(dot + 1).toLowerCase()
        if (ext === "pwmb" || ext === "pws" || ext === "pw0" || ext === "phz" || ext === "photons")
            return ext
        return "other"
    }

    function fileTypeLabel(fileName) {
        var ext = fileType(fileName)
        if (ext === "other")
            return qsTr("other")
        return ext.toUpperCase()
    }

    function fileMatchesFilter(fileName) {
        if (typeFilterValue === "all")
            return true
        return fileType(fileName) === String(typeFilterValue)
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
        saveDialog.defaultSuffix = String(fileType(fileName))
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
        cloudFilesModel.clear()
        cloudFilesModel.append({
            "fileId": "demo-001",
            "fileName": "rook_plate_v12.pwmb",
            "status": "READY",
            "sizeText": "42.6 MB",
            "machine": "Photon Mono M7",
            "material": "Eco Resin Gray",
            "uploadTime": "2026-03-05",
            "printTime": "02h 15m",
            "layerThickness": "0.05 mm",
            "layers": 1850,
            "isPwmb": true,
            "resinUsage": "67 ml",
            "dimensions": "102x68x120",
            "thumbnailUrl": "",
            "gcodeId": "demo-gcode-001"
        })
        cloudFilesModel.append({
            "fileId": "demo-002",
            "fileName": "calibration_tower.pws",
            "status": "READY",
            "sizeText": "11.8 MB",
            "machine": "Photon Mono M5s",
            "material": "ABS-Like Resin",
            "uploadTime": "2026-03-05",
            "printTime": "00h 48m",
            "layerThickness": "0.05 mm",
            "layers": 620,
            "isPwmb": false,
            "resinUsage": "14 ml",
            "dimensions": "35x35x80",
            "thumbnailUrl": "",
            "gcodeId": "demo-gcode-002"
        })
        root.quotaData = {
            "totalDisplay": "2.0 GB",
            "usedDisplay": "1.1 GB",
            "totalBytes": 2147483648,
            "usedBytes": 1181116006
        }
        root.statusMsg = qsTr("Demo mode (backend unavailable).")
        root.statusSev = "warn"
        root.loading = false
    }

    function loadFiles() {
        if (root.loading)
            return

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
        title: qsTr("File Details")
        subtitle: qsTr("Metadata and slice settings")
        minimumWidth: 860
        maximumWidth: 1020

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

                Text {
                    text: qsTr("ID: ") + String(fileDetailsDialog.fileData.fileId || "-")
                    color: Theme.fgSecondary
                    opacity: 0.9
                    font.pixelSize: Theme.fontCaptionPx
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

                        Text { text: qsTr("Status"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                        Text {
                            text: root.displayStatus(fileDetailsDialog.fileData.status)
                            color: root.statusColor(fileDetailsDialog.fileData.status)
                            font.pixelSize: Theme.fontBodyPx
                        }

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
                    model: [
                        { "code": "all", "label": qsTr("All") },
                        { "code": "pwmb", "label": "pwmb" },
                        { "code": "pws", "label": "pws" },
                        { "code": "pw0", "label": "pw0" },
                        { "code": "phz", "label": "phz" },
                        { "code": "photons", "label": "photons" },
                        { "code": "other", "label": qsTr("other") }
                    ]
                    onActivated: {
                        if (currentIndex >= 0 && currentIndex < model.length)
                            root.typeFilterValue = String(model[currentIndex].code)
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

                ProgressBar {
                    Layout.fillWidth: true
                    from: 0
                    to: 1
                    value: root.quotaRatio()
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
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    color: Theme.bgSurface
                    border.width: 0

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        Text { Layout.preferredWidth: root.colCheckWidth; text: qsTr(""); color: Theme.fgSecondary }
                        Text {
                            Layout.preferredWidth: root.colThumbWidth
                            text: qsTr("Thumb")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("File name")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }
                        Text {
                            Layout.preferredWidth: root.colTypeWidth
                            text: qsTr("Type")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }
                        Text {
                            Layout.preferredWidth: root.colSizeWidth
                            text: qsTr("Size")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }
                        Text {
                            Layout.preferredWidth: root.colDateWidth
                            text: qsTr("Date")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }
                        Text {
                            Layout.preferredWidth: root.colStatusWidth
                            text: qsTr("Status")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }
                        Text {
                            Layout.preferredWidth: root.colActionsWidth
                            text: qsTr("Actions")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
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
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        property bool rowVisible: root.fileMatchesFilter(model.fileName || "")
                        width: filesList.width
                        height: rowVisible ? 60 : 0
                        visible: rowVisible
                        color: root.selectedFileId === String(model.fileId) ? Theme.selectionBg : Theme.bgSurface
                        border.width: 0

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8

                            AppCheckBox {
                                Layout.preferredWidth: root.colCheckWidth
                                checked: root.selectedFileId === String(model.fileId)
                                onClicked: {
                                    if (root.selectedFileId === String(model.fileId))
                                        root.selectedFileId = ""
                                    else
                                        root.selectedFileId = String(model.fileId)
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 48
                                Layout.alignment: Qt.AlignVCenter
                                radius: 6
                                color: Theme.accentSoft
                                border.width: Theme.borderWidth
                                border.color: Theme.borderDefault

                                Text {
                                    anchors.centerIn: parent
                                    text: root.fileTypeLabel(model.fileName || "")
                                    color: Theme.fgPrimary
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: String(model.fileName || "-")
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.fontBodyPx
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }

                            Text {
                                Layout.preferredWidth: root.colTypeWidth
                                text: root.fileTypeLabel(model.fileName || "")
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.fontBodyPx
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            Text {
                                Layout.preferredWidth: root.colSizeWidth
                                text: String(model.sizeText || "-")
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.fontBodyPx
                                horizontalAlignment: Text.AlignRight
                                verticalAlignment: Text.AlignVCenter
                            }

                            Text {
                                Layout.preferredWidth: root.colDateWidth
                                text: root.displayDate(model.uploadTime)
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.fontBodyPx
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            Text {
                                Layout.preferredWidth: root.colStatusWidth
                                text: root.displayStatus(model.status)
                                color: root.statusColor(model.status)
                                font.pixelSize: Theme.fontBodyPx
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            RowLayout {
                                Layout.preferredWidth: root.colActionsWidth
                                spacing: 6

                                AppButton {
                                    text: qsTr("Details")
                                    variant: "secondary"
                                    onClicked: root.openFileDetails(model.fileId)
                                }

                                AppButton {
                                    text: qsTr("Download")
                                    variant: "secondary"
                                    onClicked: root.requestDownload(model.fileId, model.fileName)
                                }

                                AppButton {
                                    text: qsTr("Print")
                                    variant: "primary"
                                    onClicked: root.requestPrint(model.fileId, model.fileName)
                                }

                                AppButton {
                                    text: qsTr("...")
                                    variant: "secondary"
                                    compact: true
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

                    footer: Item {
                        width: filesList.width
                        height: cloudFilesModel.count === 0 || root.visibleFileCount() === 0 ? 120 : 0

                        Text {
                            anchors.centerIn: parent
                            visible: parent.height > 0 && !root.loading
                            text: cloudFilesModel.count === 0
                                  ? qsTr("No cloud files. Click Refresh to load files.")
                                  : qsTr("No file matches current type filter.")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontBodyPx
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
