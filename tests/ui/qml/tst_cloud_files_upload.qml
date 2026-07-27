import QtQuick 2.15
import QtTest 1.3

TestCase {
    name: "CloudFilesUploadUi"
    property var cloudBridge: undefined
    property var uiSettingsBridge: undefined

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
    }

    function createQmlObject(path, props) {
        var component = Qt.createComponent(path)
        if (component.status !== Component.Ready && path.indexOf("../../../ui/qml/") === 0) {
            var sourcePath = "../../../src/accloud/ui/qml/" + path.slice(String("../../../ui/qml/").length)
            component = Qt.createComponent(sourcePath)
        }
        compare(component.status, Component.Ready, "Unable to load " + path + " -> " + component.errorString())
        var object = component.createObject(null, props ? props : {})
        verify(object !== null, "Unable to create object for " + path)
        return object
    }

    function createUploadBridgeMock() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'signal uploadFinished(bool ok, string message, string fileId, string gcodeId, int uploadStatus, bool unlockOk, bool localFileSynchronized);' +
                                         'signal uploadProgressChanged(real progress, string phase);' +
                                         'signal filesUpdatedFromCloud(var files, string message);' +
                                         'signal quotaUpdatedFromCloud(var quota, string message);' +
                                         'signal syncFailed(string scope, string message);' +
                                         'signal pwszCloudPreviewUpdateSuggested(var files, real totalBytes);' +
                                         'signal pwszCloudPreviewUpdateProgress(int current, int total, string fileName, string phase);' +
                                         'signal pwszCloudPreviewUpdateFinished(var summary);' +
                                         'function fetchFiles(page, limit) { return { ok: true, files: [] } }' +
                                         'function fetchQuota() { return { ok: true, totalBytes: 0, usedBytes: 0 } }' +
                                         'function loadCachedFiles(page, limit) { return { ok: true, files: [] } }' +
                                         'function loadCachedQuota() { return { ok: true, totalDisplay: "2 GB", usedDisplay: "1 GB", totalBytes: 2000, usedBytes: 1000 } }' +
                                         'function refreshFilesAsync(page, limit, force) { refreshCalls += 1 }' +
                                         'function inspectPwszPreview(localPath) { inspectCalls += 1; return { ok: true, isPwsz: true, hasPreview1: true, hasPreview2: !inspectNeedsCompletion, needsCompletion: inspectNeedsCompletion } }' +
                                         'function startUploadLocalFile(localPath, completePreview) { uploadCalls += 1; lastUploadArg = localPath; lastCompletePreview = completePreview }' +
                                         'function startPwszCloudPreviewUpdate(files) { cloudUpdateCalls += 1; lastCloudUpdateCount = files.length }' +
                                         'property int refreshCalls: 0;' +
                                         'property int uploadCalls: 0; property int inspectCalls: 0; property bool inspectNeedsCompletion: true;' +
                                         'property int cloudUpdateCalls: 0; property int lastCloudUpdateCount: 0;' +
                                         'property string lastUploadArg: ""; property bool lastCompletePreview: false;' +
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

}
