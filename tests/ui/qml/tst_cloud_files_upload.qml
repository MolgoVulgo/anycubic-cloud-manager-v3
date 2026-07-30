import QtQuick 2.15
import QtQuick.Controls 2.15
import QtTest 1.3

TestCase {
    name: "CloudFilesUploadUi"
    property var cloudBridge: undefined
    property var uiSettingsBridge: undefined
    property var dialogHostWindow: undefined

    function cleanup() {
        if (cloudBridge !== undefined && cloudBridge !== null && cloudBridge.destroy !== undefined) {
            cloudBridge.destroy()
        }
        cloudBridge = undefined
        if (uiSettingsBridge !== undefined && uiSettingsBridge !== null
                && uiSettingsBridge.destroy !== undefined) {
            uiSettingsBridge.destroy()
        }
        uiSettingsBridge = undefined
        if (dialogHostWindow !== undefined && dialogHostWindow !== null
                && dialogHostWindow.destroy !== undefined) {
            dialogHostWindow.destroy()
        }
        dialogHostWindow = undefined
    }

    function createQmlObject(path, props, parentObject) {
        var component = Qt.createComponent(path)
        if (component.status !== Component.Ready && path.indexOf("../../../ui/qml/") === 0) {
            var sourcePath = "../../../src/accloud/ui/qml/" + path.slice(String("../../../ui/qml/").length)
            component = Qt.createComponent(sourcePath)
        }
        compare(component.status, Component.Ready, "Unable to load " + path + " -> " + component.errorString())
        var owner = (parentObject !== undefined && parentObject !== null) ? parentObject : null
        var object = component.createObject(owner, props ? props : {})
        verify(object !== null, "Unable to create object for " + path)
        return object
    }

    function findObjectByName(root, name, visited) {
        if (root === null || root === undefined)
            return null
        if (visited === null || visited === undefined)
            visited = []
        for (var v = 0; v < visited.length; ++v) {
            if (visited[v] === root)
                return null
        }
        visited.push(root)

        if (root.objectName === name)
            return root

        var direct = [root.contentItem, root.background, root.header, root.footer, root.popupItem]
        for (var d = 0; d < direct.length; ++d) {
            var directNode = direct[d]
            if (directNode !== null && directNode !== undefined) {
                var directFound = findObjectByName(directNode, name, visited)
                if (directFound !== null)
                    return directFound
            }
        }

        var collections = [root.children, root.contentChildren, root.data]
        for (var c = 0; c < collections.length; ++c) {
            var kids = collections[c]
            if (kids === null || kids === undefined)
                continue
            for (var i = 0; i < kids.length; ++i) {
                var found = findObjectByName(kids[i], name, visited)
                if (found !== null)
                    return found
            }
        }
        return null
    }

    function createUploadBridgeMock() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'signal uploadFinished(bool ok, string message, string fileId, string gcodeId, int uploadStatus, bool unlockOk, bool localFileSynchronized);' +
                                         'signal uploadProgressChanged(real progress, string phase);' +
                                         'signal filesUpdatedFromCloud(var files, string message);' +
                                         'signal quotaUpdatedFromCloud(var quota, string message);' +
                                         'signal syncFailed(string scope, string message);' +
                                         'signal downloadProgress(int received, int total);' +
                                         'signal downloadFinished(bool ok, string message, string savedPath);' +
                                         'signal pwszCloudPreviewUpdateSuggested(var files, real totalBytes);' +
                                         'signal pwszCloudPreviewUpdateProgress(int current, int total, string fileName, string phase);' +
                                         'signal pwszCloudPreviewUpdateFinished(var summary);' +
                                         'function fetchFiles(page, limit) { return { ok: true, files: [] } }' +
                                         'function fetchQuota() { return { ok: true, totalBytes: 0, usedBytes: 0 } }' +
                                         'function loadCachedFiles(page, limit) { return { ok: true, files: [] } }' +
                                         'function loadCachedQuota() { return { ok: true, totalDisplay: "2 GB", usedDisplay: "1 GB", totalBytes: 2000, usedBytes: 1000 } }' +
                                         'function refreshFilesAsync(page, limit, force) { refreshCalls += 1 }' +
                                         'function getDownloadUrl(fileId) { downloadUrlCalls += 1; return { ok: true, url: "https://signed.invalid/file?signature=redacted" } }' +
                                         'function startDownload(url, savePath) { downloadCalls += 1; lastDownloadUrl = url; lastDownloadPath = savePath }' +
                                         'function inspectPwszPreview(localPath) { inspectCalls += 1; return { ok: true, isPwsz: true, hasPreview1: true, hasPreview2: !inspectNeedsCompletion, needsCompletion: inspectNeedsCompletion } }' +
                                         'function startUploadLocalFile(localPath, completePreview) { uploadCalls += 1; lastUploadArg = localPath; lastCompletePreview = completePreview }' +
                                         'function startPwszCloudPreviewUpdate(files) { cloudUpdateCalls += 1; lastCloudUpdateCount = files.length }' +
                                         'function cancelPwszCloudPreviewUpdate() { cloudUpdateCancelCalls += 1 }' +
                                         'property int refreshCalls: 0; property int downloadUrlCalls: 0; property int downloadCalls: 0;' +
                                         'property int uploadCalls: 0; property int inspectCalls: 0; property bool inspectNeedsCompletion: true;' +
                                         'property int cloudUpdateCalls: 0; property int cloudUpdateCancelCalls: 0; property int lastCloudUpdateCount: 0;' +
                                         'property string lastUploadArg: ""; property bool lastCompletePreview: false;' +
                                         'property string lastDownloadUrl: ""; property string lastDownloadPath: "";' +
                                         '}', this, "cloudFilesUploadBridgeMock")
    }


    function createSettingsMock(confirmBeforeModification) {
        var confirmValue = confirmBeforeModification === true ? "true" : "false"
        uiSettingsBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                              'property string confirmationValue: "' + confirmValue + '";' +
                                              'function getString(key, fallback) {' +
                                              '  if (key === "cloud.upload.completePwszPreview2") return "true";' +
                                              '  if (key === "cloud.upload.confirmPwszPreview2") return confirmationValue;' +
                                              '  return fallback;' +
                                              '}' +
                                              'function setString(key, value) { if (key === "cloud.upload.confirmPwszPreview2") confirmationValue = value }' +
                                              'function sync() {}' +
                                              '}', this, "cloudFilesUploadSettingsMock")
    }

    function test_download_dialog_uses_app_ui_and_prefills_cloud_file_name() {
        createUploadBridgeMock()
        dialogHostWindow = Qt.createQmlObject(
                    'import QtQuick 2.15; import QtQuick.Controls 2.15; '
                    + 'ApplicationWindow { width: 1280; height: 800; visible: true }',
                    this,
                    "downloadDialogHostWindow")
        verify(dialogHostWindow !== null)
        tryCompare(dialogHostWindow, "visible", true, 1000)

        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml",
                                   {"width": 1280, "height": 800},
                                   dialogHostWindow.contentItem)

        page.requestDownload("file-1", "B1-B2-B5(1).pwsz")

        var dialog = findObjectByName(page, "cloudDownloadFileDialog")
        var fileNameField = findObjectByName(page, "downloadFileNameField")
        var saveButton = findObjectByName(page, "downloadDialogSaveButton")
        var folderTree = findObjectByName(page, "downloadDialogFolderTree")
        var contentList = findObjectByName(page, "downloadDialogContentList")
        var upButton = findObjectByName(page, "downloadDialogUpButton")
        verify(dialog !== null)
        verify(fileNameField !== null)
        verify(saveButton !== null)
        verify(folderTree !== null)
        verify(contentList !== null)
        compare(upButton, null)
        compare(dialog.effectiveNameFilters.length, 1)
        tryCompare(dialog, "visible", true, 1000)
        compare(String(fileNameField.text), "B1-B2-B5(1).pwsz")
        compare(String(dialog.defaultSuffix), "pwsz")
        compare(String(dialog.finalName), "B1-B2-B5(1).pwsz")

        fileNameField.text = "renamed-part"
        compare(String(dialog.finalName), "renamed-part.pwsz")
        fileNameField.text = "../outside"
        compare(String(dialog.finalName), "outside.pwsz")

        dialog.commitSave("/tmp/B1-B2-B5(1).pwsz")
        wait(0)
        compare(cloudBridge.downloadUrlCalls, 1)
        compare(cloudBridge.downloadCalls, 1)
        compare(String(cloudBridge.lastDownloadPath), "/tmp/B1-B2-B5(1).pwsz")
        verify(String(cloudBridge.lastDownloadUrl).indexOf("https://signed.invalid/file") === 0)

        page.destroy()
    }

    function test_upload_selected_local_file_keeps_raw_input_for_backend() {
        createUploadBridgeMock()
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})

        var selectedInput = "file:///home/kaj/slices/test%20cube.pwmb"
        page.uploadSelectedLocalFile(selectedInput)
        wait(0)

        compare(cloudBridge.uploadCalls, 1)
        compare(String(cloudBridge.lastUploadArg), selectedInput)
        compare(String(page.statusSev), "info")
        compare(String(page.statusMsg), "Uploading test cube.pwmb...")

        page.destroy()
    }


    function test_pwsz_missing_preview_is_completed_when_confirmation_disabled() {
        createUploadBridgeMock()
        createSettingsMock(false)
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})

        var selectedInput = "file:///home/kaj/slices/cube.pwsz"
        page.uploadSelectedLocalFile(selectedInput)
        wait(0)

        compare(cloudBridge.inspectCalls, 1)
        compare(cloudBridge.uploadCalls, 1)
        compare(String(cloudBridge.lastUploadArg), selectedInput)
        compare(cloudBridge.lastCompletePreview, true)
        page.destroy()
    }

    function test_pwsz_with_preview_uses_original_file() {
        createUploadBridgeMock()
        createSettingsMock(false)
        cloudBridge.inspectNeedsCompletion = false
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})

        page.uploadSelectedLocalFile("file:///home/kaj/slices/complete.pwsz")
        wait(0)

        compare(cloudBridge.inspectCalls, 1)
        compare(cloudBridge.uploadCalls, 1)
        compare(cloudBridge.lastCompletePreview, false)
        page.destroy()
    }

    function test_upload_finished_processing_sets_warn() {
        createUploadBridgeMock()
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})

        var emitted = []
        page.statusBroadcast.connect(function(message, severity, operationId) {
            emitted.push({
                "message": String(message),
                "severity": String(severity)
            })
        })

        cloudBridge.uploadFinished(true, "", "file-1", "", 2, true, true)
        wait(0)

        var sawProcessingWarn = false
        for (var i = 0; i < emitted.length; ++i) {
            var entry = emitted[i]
            if (entry.severity === "warn"
                    && entry.message.toLowerCase().indexOf("processing") >= 0) {
                sawProcessingWarn = true
                break
            }
        }
        verify(sawProcessingWarn)

        page.destroy()
    }

    function test_upload_finished_zero_gcode_stays_processing() {
        createUploadBridgeMock()
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})

        var emitted = []
        page.statusBroadcast.connect(function(message, severity, operationId) {
            emitted.push({
                "message": String(message),
                "severity": String(severity)
            })
        })

        cloudBridge.uploadFinished(true, "", "file-1", "0", 2, true, true)
        wait(0)

        var sawProcessingWarn = false
        for (var i = 0; i < emitted.length; ++i) {
            var entry = emitted[i]
            if (entry.severity === "warn"
                    && entry.message.toLowerCase().indexOf("processing") >= 0) {
                sawProcessingWarn = true
                break
            }
        }
        verify(sawProcessingWarn)

        page.destroy()
    }


    function test_upload_finished_local_replacement_failure_sets_warn() {
        createUploadBridgeMock()
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})

        cloudBridge.uploadFinished(true, "Cloud upload succeeded; local replacement failed.",
                                   "file-1", "gcode-42", 2, true, false)
        wait(0)

        compare(String(page.statusSev), "warn")
        verify(String(page.statusMsg).indexOf("local replacement failed") >= 0)
        page.destroy()
    }

    function test_upload_finished_ready_sets_success() {
        createUploadBridgeMock()
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})

        var emitted = []
        page.statusBroadcast.connect(function(message, severity, operationId) {
            emitted.push({
                "message": String(message),
                "severity": String(severity)
            })
        })

        cloudBridge.uploadFinished(true, "", "file-1", "gcode-42", 2, true, true)
        wait(0)

        var sawCompletedSuccess = false
        for (var i = 0; i < emitted.length; ++i) {
            var entry = emitted[i]
            if (entry.severity === "success" && entry.message === "Upload completed.") {
                sawCompletedSuccess = true
                break
            }
        }
        verify(sawCompletedSuccess)

        page.destroy()
    }
    function test_invalid_cloud_thumbnails_offer_batch_update() {
        createUploadBridgeMock()
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})

        cloudBridge.pwszCloudPreviewUpdateSuggested([
            {"fileId": "1", "fileName": "a.pwsz", "sizeBytes": 1024},
            {"fileId": "2", "fileName": "b.pwsz", "sizeBytes": 2048}
        ], 3072)
        wait(0)

        compare(page.pwszCloudUpdateCandidates.length, 2)
        compare(Number(page.pwszCloudUpdateBytes), 3072)
        verify(page.pwszCloudUpdateProposalVisible)
        page.destroy()
    }

    function test_batch_update_progress_can_request_cancel() {
        createUploadBridgeMock()
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})

        page.pwszCloudUpdateRunning = true
        cloudBridge.pwszCloudPreviewUpdateProgress(1, 2, "a.pwsz", "Traitement")
        wait(0)

        var cancelButton = findChild(page, "pwszCloudUpdateCancelButton")
        verify(cancelButton !== null)
        verify(cancelButton.enabled)
        verify(page.requestPwszCloudUpdateCancellation())
        wait(0)

        compare(cloudBridge.cloudUpdateCancelCalls, 1)
        verify(page.pwszCloudUpdateCancelRequested)
        verify(!cancelButton.enabled)
        verify(!page.requestPwszCloudUpdateCancellation())
        compare(cloudBridge.cloudUpdateCancelCalls, 1)
        page.destroy()
    }

    function test_batch_update_phase_keys_are_localized() {
        createUploadBridgeMock()
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})

        compare(String(page.pwszCloudUpdatePhaseText("pwsz.update.download")),
                "Downloading cloud file")
        compare(String(page.pwszCloudUpdatePhaseText("pwsz.update.validate_thumbnail")),
                "Validating the new thumbnail")
        compare(String(page.pwszCloudUpdatePhaseText("custom.phase")), "custom.phase")
        page.destroy()
    }

    function test_batch_update_result_exposes_issue_details() {
        createUploadBridgeMock()
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})
        var summary = {
            "ok": false,
            "cancelled": false,
            "modified": 1,
            "skipped": 0,
            "failed": 0,
            "partial": 1,
            "cancelledItems": 0,
            "items": [{
                "fileName": "cube.pwsz",
                "status": "partial",
                "message": "Both versions were kept",
                "originalFileId": "old-1",
                "newFileId": "new-2"
            }]
        }

        compare(String(page.pwszCloudUpdateResultTitle(summary)), "Update completed with issues")
        var details = String(page.pwszCloudUpdateIssueDetails(summary))
        verify(details.indexOf("cube.pwsz") >= 0)
        verify(details.indexOf("old-1") >= 0)
        verify(details.indexOf("new-2") >= 0)
        verify(details.indexOf("Both versions were kept") >= 0)
        page.destroy()
    }

    function test_incomplete_inventory_is_visible_to_user() {
        createUploadBridgeMock()
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})

        cloudBridge.syncFailed("pwsz_preview_candidates", "pagination stopped")
        wait(0)

        compare(String(page.statusSev), "warn")
        verify(String(page.statusMsg).indexOf("inventory incomplete") >= 0)
        page.destroy()
    }

}
