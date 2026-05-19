import QtQuick 2.15
import QtTest 1.3

TestCase {
    name: "CloudFilesUploadUi"
    property var cloudBridge: undefined

    function cleanup() {
        if (cloudBridge !== undefined && cloudBridge !== null && cloudBridge.destroy !== undefined) {
            cloudBridge.destroy()
        }
        cloudBridge = undefined
    }

    function createQmlObject(path, props) {
        var component = Qt.createComponent(path)
        compare(component.status, Component.Ready, "Unable to load " + path + " -> " + component.errorString())
        var object = component.createObject(null, props ? props : {})
        verify(object !== null, "Unable to create object for " + path)
        return object
    }

    function createUploadBridgeMock() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'signal uploadFinished(bool ok, string message, string fileId, string gcodeId, int uploadStatus, bool unlockOk);' +
                                         'signal uploadProgressChanged(real progress, string phase);' +
                                         'signal filesUpdatedFromCloud(var files, string message);' +
                                         'signal quotaUpdatedFromCloud(var quota, string message);' +
                                         'signal syncFailed(string scope, string message);' +
                                         'function fetchFiles(page, limit) { return { ok: true, files: [] } }' +
                                         'function fetchQuota() { return { ok: true, totalBytes: 0, usedBytes: 0 } }' +
                                         'function loadCachedFiles(page, limit) { return { ok: true, files: [] } }' +
                                         'function loadCachedQuota() { return { ok: true, totalDisplay: "2 GB", usedDisplay: "1 GB", totalBytes: 2000, usedBytes: 1000 } }' +
                                         'function refreshFilesAsync(page, limit, force) { refreshCalls += 1 }' +
                                         'function startUploadLocalFile(localPath) { uploadCalls += 1; lastUploadArg = localPath }' +
                                         'property int refreshCalls: 0;' +
                                         'property int uploadCalls: 0;' +
                                         'property string lastUploadArg: "";' +
                                         '}', this, "cloudFilesUploadBridgeMock")
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

        cloudBridge.uploadFinished(true, "", "file-1", "", 2, true)
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

        cloudBridge.uploadFinished(true, "", "file-1", "gcode-42", 2, true)
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
}
