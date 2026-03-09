import QtQuick 2.15
import QtQuick.Controls 2.15
import QtTest 1.3

TestCase {
    name: "ControlRoomUi"
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

    function findObjectByName(root, name, visited) {
        if (root === null || root === undefined) {
            return null
        }
        if (visited === null || visited === undefined) {
            visited = []
        }
        for (var v = 0; v < visited.length; ++v) {
            if (visited[v] === root) {
                return null
            }
        }
        visited.push(root)

        if (root.objectName === name) {
            return root
        }

        var direct = [root.contentItem, root.background, root.header, root.footer, root.popupItem]
        for (var d = 0; d < direct.length; ++d) {
            var directNode = direct[d]
            if (directNode !== null && directNode !== undefined) {
                var directFound = findObjectByName(directNode, name, visited)
                if (directFound !== null) {
                    return directFound
                }
            }
        }

        var collections = [root.children, root.contentChildren, root.data]
        for (var c = 0; c < collections.length; ++c) {
            var kids = collections[c]
            if (kids === null || kids === undefined) {
                continue
            }
            for (var i = 0; i < kids.length; ++i) {
                var found = findObjectByName(kids[i], name, visited)
                if (found !== null) {
                    return found
                }
            }
        }

        return null
    }

    function test_main_window_has_control_room_layout() {
        var window = createQmlObject("../../../ui/qml/MainWindow.qml")
        compare(window.title, "Anycubic Cloud Control Room")

        var tabs = findObjectByName(window, "controlRoomTabs")
        verify(tabs !== null)
        compare(tabs.count, 3)

        var uploadButton = findObjectByName(window, "uploadDialogButton")
        verify(uploadButton !== null)

        var menuBar = findObjectByName(window, "mainMenuBar")
        verify(menuBar !== null)
        if (menuBar.contentChildren !== undefined) {
            verify(menuBar.contentChildren.length >= 3)
        }
        verify(findObjectByName(window, "render3dDefaultsDialog") !== null)

        window.close()
        window.destroy()
    }

    function test_cloud_files_has_mock_data_and_upload_action() {
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})
        compare(page.filesModel.count, 2)

        var uploadButton = findObjectByName(page, "uploadPwmbButton")
        verify(uploadButton !== null)
        compare(uploadButton.text, "Upload")

        page.destroy()
    }

    function test_cloud_files_header_and_row_columns_are_aligned() {
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})
        page.loadMockFiles()
        wait(160)

        var headerThumb = findObjectByName(page, "fileHeaderThumb")
        var headerName = findObjectByName(page, "fileHeaderName")
        var headerType = findObjectByName(page, "fileHeaderType")
        var headerSize = findObjectByName(page, "fileHeaderSize")
        var headerDate = findObjectByName(page, "fileHeaderDate")
        var headerActions = findObjectByName(page, "fileHeaderActions")

        verify(headerThumb !== null)
        verify(headerName !== null)
        verify(headerType !== null)
        verify(headerSize !== null)
        verify(headerDate !== null)
        verify(headerActions !== null)

        compare(headerName.width, page.colNameWidth)
        verify(page.colNameWidth > 200)
        compare(headerThumb.width, page.colThumbWidth)
        compare(headerType.width, page.colTypeWidth)
        compare(headerSize.width, page.colSizeWidth)
        compare(headerDate.width, page.colDateWidth)
        compare(headerActions.width, page.colActionsWidth)

        var totalColumns = page.colThumbWidth + page.colNameWidth + page.colTypeWidth
                         + page.colSizeWidth + page.colDateWidth + page.colActionsWidth
                         + page.tableColumnSpacing * 5
        compare(totalColumns, page.tableViewportWidth)

        compare(headerName.horizontalAlignment, Text.AlignLeft)
        compare(headerType.horizontalAlignment, Text.AlignHCenter)

        page.destroy()
    }

    function test_cloud_files_pagination_defaults_and_rows_per_page_options() {
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})
        page.loadMockFiles()
        wait(120)

        compare(page.pageSize, 10)
        compare(page.totalPages(), 1)

        var rowsPerPage = findObjectByName(page, "filesRowsPerPage")
        verify(rowsPerPage !== null)
        compare(rowsPerPage.model.length, 4)
        compare(rowsPerPage.model[0].value, 10)
        compare(rowsPerPage.model[1].value, 20)
        compare(rowsPerPage.model[2].value, 50)
        compare(rowsPerPage.model[3].value, 100)
        compare(rowsPerPage.model[0].label, "10")
        compare(rowsPerPage.model[1].label, "20")
        compare(rowsPerPage.model[2].label, "50")
        compare(rowsPerPage.model[3].label, "100")

        rowsPerPage.popup.open()
        wait(120)
        var popupList = rowsPerPage.popup.contentItem
        verify(popupList !== null)
        compare(popupList.count, 4)
        var item0 = popupList.itemAtIndex(0)
        var item1 = popupList.itemAtIndex(1)
        var item2 = popupList.itemAtIndex(2)
        var item3 = popupList.itemAtIndex(3)
        verify(item0 !== null)
        verify(item1 !== null)
        verify(item2 !== null)
        verify(item3 !== null)
        compare(String(item0.contentItem.text), "10")
        compare(String(item1.contentItem.text), "20")
        compare(String(item2.contentItem.text), "50")
        compare(String(item3.contentItem.text), "100")
        item1.clicked()
        wait(80)
        compare(page.pageSize, 20)
        compare(rowsPerPage.currentIndex, 1)

        page.destroy()
    }

    function test_cloud_files_type_filter_uses_present_supported_extensions_only() {
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})

        page.filesModel.clear()
        page.filesModel.append({ fileId: "a", fileName: "a.pm3", sizeText: "1 MB", uploadTime: "2026-03-07" })
        page.filesModel.append({ fileId: "b", fileName: "b.pwmb", sizeText: "2 MB", uploadTime: "2026-03-07" })
        page.filesModel.append({ fileId: "d", fileName: "d.pwsz", sizeText: "4 MB", uploadTime: "2026-03-07" })
        page.filesModel.append({ fileId: "c", fileName: "c.txt", sizeText: "3 MB", uploadTime: "2026-03-07" })
        page.refreshTypeFilterOptions()

        compare(page.typeFilterOptions.length, 4)
        compare(page.typeFilterOptions[0].code, "all")
        compare(page.typeFilterOptions[1].code, "pm3")
        compare(page.typeFilterOptions[2].code, "pwmb")
        compare(page.typeFilterOptions[3].code, "pwsz")
        compare(page.fileTypeLabel("demo.pm5s"), "PM5S")
        compare(page.fileTypeLabel("demo.unknown"), "UNKNOWN")
        compare(page.compatiblePrintersTooltip("part.pm3"), "Compatible printers: Photon Mono 3, Mono 3 Ultra")
        compare(page.compatiblePrintersTooltip("part.m5sp"), "Compatible printers: Photon Mono M5s Pro")
        compare(page.compatiblePrintersTooltip("part.dlp"), "Compatible printers: Anycubic DLP printers")

        page.destroy()
    }

    function test_file_card_shows_viewer_button_only_for_pwmb() {
        var nonPwmb = createQmlObject("../../../ui/qml/components/FileCard.qml", {"isPwmb": false})
        var viewerA = findObjectByName(nonPwmb, "openViewerButton")
        verify(viewerA !== null)
        compare(viewerA.visible, false)
        nonPwmb.destroy()

        var pwmb = createQmlObject("../../../ui/qml/components/FileCard.qml", {"isPwmb": true})
        var viewerB = findObjectByName(pwmb, "openViewerButton")
        verify(viewerB !== null)
        compare(viewerB.visible, true)
        pwmb.destroy()
    }

    function test_session_dialog_default_target_path() {
        var dialog = createQmlObject("../../../ui/qml/dialogs/SessionSettingsDialog.qml")
        var harField = findObjectByName(dialog, "harFileField")
        var closeButton = findObjectByName(dialog, "harImportCloseButton")
        verify(harField !== null)
        verify(closeButton !== null)
        dialog.destroy()
    }

    function test_session_dialog_import_button_updates_status_without_bridge() {
        var dialog = createQmlObject("../../../ui/qml/dialogs/SessionSettingsDialog.qml")

        var harField = findObjectByName(dialog, "harFileField")
        var statusLabel = findObjectByName(dialog, "harImportStatusLabel")
        var detailsPanel = findObjectByName(dialog, "harImportResultPanel")

        verify(harField !== null)
        verify(statusLabel !== null)
        verify(detailsPanel !== null)

        dialog.runAnalyzeForPath("/tmp/session.har")

        verify(statusLabel.text.indexOf("backend bridge unavailable") !== -1)
        verify(detailsPanel.text.indexOf("sessionImportBridge is undefined") !== -1
               || detailsPanel.text.indexOf("sessionImportBridge") !== -1)
        dialog.destroy()
    }

    function test_session_dialog_import_button_calls_bridge_and_updates_result() {
        var dialog = createQmlObject("../../../ui/qml/dialogs/SessionSettingsDialog.qml")
        var commitPath = ""
        dialog.importBridge = {
            analyzeHar: function(harPath, sessionPath) {
                return {
                    ok: true,
                    message: "mock import done",
                    entriesVisited: 4,
                    entriesAccepted: 2,
                    tokenKeys: ["Authorization", "access_token"],
                    sessionPath: sessionPath
                }
            },
            commitPendingSession: function(sessionPath) {
                commitPath = sessionPath
                return {
                    ok: true,
                    message: "session saved",
                    connectionOk: true,
                    connectionMessage: "cloud ok"
                }
            },
            discardPendingSession: function() {}
        }
        dialog.sessionTargetPath = "/tmp/session.json"

        var statusLabel = findObjectByName(dialog, "harImportStatusLabel")
        var detailsPanel = findObjectByName(dialog, "harImportResultPanel")
        var closeButton = findObjectByName(dialog, "harImportCloseButton")

        verify(statusLabel !== null)
        verify(detailsPanel !== null)
        verify(closeButton !== null)

        dialog.runAnalyzeForPath("/tmp/sample.har")

        verify(statusLabel.text.indexOf("valid analysis") !== -1)
        verify(detailsPanel.text.indexOf("HAR analysis: VALID") !== -1)
        verify(detailsPanel.text.indexOf("mock import done") !== -1)
        verify(detailsPanel.text.indexOf("/tmp/session.json") !== -1)

        dialog.requestClose()
        compare(commitPath, "/tmp/session.json")
        dialog.destroy()
    }

    function test_app_tab_components_load() {
        var tabBar = createQmlObject("../../../ui/qml/components/AppTabBar.qml")
        verify(tabBar !== null)
        verify(tabBar.spacing >= 0)
        tabBar.destroy()

        var tabButton = createQmlObject("../../../ui/qml/components/AppTabButton.qml", {"text": "Demo"})
        verify(tabButton !== null)
        compare(tabButton.text, "Demo")
        verify(tabButton.implicitHeight > 0)
        tabButton.destroy()
    }

    function test_log_page_dynamic_sources_and_filters() {
        var page = createQmlObject("../../../ui/qml/pages/LogPage.qml", {"width": 1280, "height": 800})
        page.logBackend = {
            fetchSnapshot: function(maxLines) {
                return {
                    ok: true,
                    message: "ok",
                    sources: ["app", "fault", "printer"],
                    components: ["bootstrap", "printer_agent"],
                    events: ["startup", "refresh_failed"],
                    entries: [
                        {
                            sink: "app",
                            ts: "2026-03-04T10:00:00.000+01:00",
                            level: "INFO",
                            source: "app",
                            component: "bootstrap",
                            event: "startup",
                            opId: "",
                            message: "app started",
                            formatted: "2026-03-04T10:00:00.000+01:00 [app] app INFO bootstrap.startup - app started"
                        },
                        {
                            sink: "printer",
                            ts: "2026-03-04T10:00:01.000+01:00",
                            level: "ERROR",
                            source: "printer",
                            component: "printer_agent",
                            event: "refresh_failed",
                            opId: "op_printer_42",
                            message: "refresh failed",
                            formatted: "2026-03-04T10:00:01.000+01:00 [printer] printer ERROR printer_agent.refresh_failed - refresh failed op_id=op_printer_42"
                        }
                    ]
                }
            }
        }
        page.refreshLogs()

        var sourceFilter = findObjectByName(page, "logSourceFilter")
        var componentFilter = findObjectByName(page, "logComponentFilter")
        var eventFilter = findObjectByName(page, "logEventFilter")
        var opIdFilter = findObjectByName(page, "logOpIdFilter")
        var logsScroll = findObjectByName(page, "logsScrollView")
        var logsArea = findObjectByName(page, "logsTextArea")

        verify(sourceFilter !== null)
        verify(componentFilter !== null)
        verify(eventFilter !== null)
        verify(opIdFilter !== null)
        verify(logsScroll !== null)
        verify(logsArea !== null)
        verify(logsScroll.ScrollBar.vertical !== null)
        verify(logsScroll.ScrollBar.horizontal !== null)
        compare(logsScroll.ScrollBar.vertical.policy, ScrollBar.AlwaysOn)
        compare(logsScroll.ScrollBar.vertical.active, true)

        verify(sourceFilter.find("printer") !== -1)
        verify(componentFilter.find("printer_agent") !== -1)
        verify(eventFilter.find("refresh_failed") !== -1)

        sourceFilter.currentIndex = sourceFilter.find("printer")
        verify(logsArea.text.indexOf("[printer]") !== -1)
        verify(logsArea.text.indexOf("[app]") === -1)

        opIdFilter.text = "op_printer_42"
        verify(logsArea.text.indexOf("op_id=op_printer_42") !== -1)

        opIdFilter.text = "op_printer_missing"
        compare(logsArea.text.trim(), "")

        page.destroy()
    }

    function test_printer_page_reason_catalog_mapping() {
        var sendCalls = 0
        cloudBridge = {
            fetchPrinters: function() {
                return {
                    ok: true,
                    message: "ok",
                    endpoint: "/mock/printers",
                    rawJson: "{}",
                    printers: [
                        {
                            id: "p1",
                            name: "Printer One",
                            model: "Mono M7",
                            type: "LCD",
                            state: "READY",
                            reason: "702",
                            available: 1,
                            progress: -1,
                            elapsedSec: -1,
                            remainingSec: -1,
                            currentFile: "",
                            lastSeen: "now"
                        }
                    ]
                }
            },
            fetchFiles: function() {
                return { ok: true, files: [] }
            },
            fetchCompatiblePrintersByExt: function() {
                return { ok: true, printers: [] }
            },
            fetchCompatiblePrintersByFileId: function() {
                return { ok: true, printers: [] }
            },
            fetchReasonCatalog: function() {
                return {
                    ok: true,
                    reasons: [
                        { reason: 702, desc: "OTA update failed", helpUrl: "https://help/702", type: "LCD", push: 0, popup: 0 }
                    ]
                }
            },
            fetchPrinterDetails: function() {
                return { ok: true, details: {} }
            },
            fetchPrinterProjects: function() {
                return { ok: true, projects: [] }
            },
            sendPrintOrder: function() {
                sendCalls += 1
                return { ok: true, taskId: "123" }
            }
        }

        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml", {"width": 1280, "height": 800})
        compare(page.reasonCatalogLoaded, true)
        compare(page.displayReason("702"), "OTA update failed (702)")
        compare(page.reasonHelpUrl("702"), "https://help/702")
        compare(sendCalls, 0)
        page.destroy()
        cloudBridge = undefined
    }

    function test_printer_page_guard_blocks_incompatible_file_id() {
        var sendCalls = 0
        cloudBridge = {
            fetchPrinters: function() {
                return {
                    ok: true,
                    message: "ok",
                    endpoint: "/mock/printers",
                    rawJson: "{}",
                    printers: [
                        {
                            id: "p1",
                            name: "Printer One",
                            model: "Mono M7",
                            type: "LCD",
                            state: "READY",
                            reason: "free",
                            available: 1,
                            progress: -1,
                            elapsedSec: -1,
                            remainingSec: -1,
                            currentFile: "",
                            lastSeen: "now"
                        }
                    ]
                }
            },
            fetchFiles: function() {
                return {
                    ok: true,
                    files: [
                        {
                            fileId: "f1",
                            fileName: "demo.pwmb",
                            sizeText: "1 MB",
                            status: "READY",
                            printTime: "1m",
                            resinUsage: "1 ml"
                        }
                    ]
                }
            },
            fetchCompatiblePrintersByExt: function(ext) {
                return {
                    ok: true,
                    printers: [
                        { id: "p1", available: 1, reason: "" }
                    ]
                }
            },
            fetchCompatiblePrintersByFileId: function(fileId) {
                return {
                    ok: true,
                    printers: [
                        { id: "p1", available: 0, reason: "unavailable reason:printer offline" }
                    ]
                }
            },
            fetchReasonCatalog: function() {
                return { ok: true, reasons: [] }
            },
            fetchPrinterDetails: function() {
                return { ok: true, details: {} }
            },
            fetchPrinterProjects: function() {
                return { ok: true, projects: [] }
            },
            sendPrintOrder: function() {
                sendCalls += 1
                return { ok: true, taskId: "123" }
            }
        }

        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml", {"width": 1280, "height": 800})
        page.openSelectCloudFileDialog("p1")
        verify(String(page.selectedCloudFileId).length > 0)
        page.openRemotePrintConfig()
        compare(page.remotePrintAllowed, false)
        verify(String(page.remotePrintBlockReason).toLowerCase().indexOf("offline") !== -1)

        page.startRemotePrint()
        compare(sendCalls, 0)
        verify(String(page.statusMsg).indexOf("Print blocked:") === 0)
        page.destroy()
        cloudBridge = undefined
    }

    function test_printer_tabs_title_contains_status() {
        cloudBridge = {
            fetchPrinters: function() {
                return {
                    ok: true,
                    message: "ok",
                    endpoint: "/mock/printers",
                    rawJson: "{}",
                    printers: [
                        {
                            id: "p1",
                            name: "Printer One",
                            model: "Mono M7",
                            type: "LCD",
                            state: "PRINTING",
                            reason: "printing",
                            available: 1,
                            progress: 12,
                            elapsedSec: 100,
                            remainingSec: 200,
                            currentFile: "demo.pwmb",
                            lastSeen: "now"
                        }
                    ]
                }
            },
            fetchFiles: function() { return { ok: true, files: [] } },
            fetchCompatiblePrintersByExt: function() { return { ok: true, printers: [] } },
            fetchCompatiblePrintersByFileId: function() { return { ok: true, printers: [] } },
            fetchReasonCatalog: function() { return { ok: true, reasons: [] } },
            fetchPrinterDetails: function() { return { ok: true, details: {} } },
            fetchPrinterProjects: function() { return { ok: true, projects: [] } },
            sendPrintOrder: function() { return { ok: true, taskId: "123" } }
        }

        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml", {"width": 1280, "height": 800})
        var tabButton = findObjectByName(page, "printerTabButton")
        verify(tabButton !== null)
        verify(String(tabButton.text).indexOf("Printer One") !== -1)
        verify(String(tabButton.text).indexOf("Printing") !== -1)
        page.destroy()
        cloudBridge = undefined
    }

    function test_cloud_files_cache_flow_forces_refresh_and_applies_cloud_signal() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'signal filesUpdatedFromCloud(var files, string message);' +
                                         'signal quotaUpdatedFromCloud(var quota, string message);' +
                                         'signal syncFailed(string scope, string message);' +
                                         'function fetchFiles(page, limit) { return { ok: true, files: [] } }' +
                                         'function fetchQuota() { return { ok: true, totalBytes: 0, usedBytes: 0 } }' +
                                         'function loadCachedFiles(page, limit) {' +
                                         '  return { ok: true, files: [{ fileId: "cached-1", fileName: "cached.pwmb", status: "READY", sizeText: "1 MB" }] }' +
                                         '}' +
                                         'function loadCachedQuota() { return { ok: true, totalDisplay: "2 GB", usedDisplay: "1 GB", totalBytes: 2000, usedBytes: 1000 } }' +
                                         'function refreshFilesAsync(page, limit, force) { refreshCalls += 1; lastForce = force }' +
                                         'property int refreshCalls: 0;' +
                                         'property bool lastForce: false;' +
                                         '}', this, "cloudFilesBridgeMock")

        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})
        compare(cloudBridge.refreshCalls, 1)
        compare(cloudBridge.lastForce, true)
        compare(page.filesModel.count, 1)
        compare(String(page.filesModel.get(0).fileId), "cached-1")

        cloudBridge.filesUpdatedFromCloud([
            { fileId: "cloud-1", fileName: "cloud.pwmb", status: "READY", sizeText: "2 MB" }
        ], "ok")
        wait(0)
        tryCompare(page.filesModel, "count", 1)
        compare(String(page.filesModel.get(0).fileId), "cloud-1")

        page.destroy()
    }

    function test_printers_cache_flow_forces_refresh_and_applies_cloud_signal() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'signal printersUpdatedFromCloud(var printers, string message);' +
                                         'signal syncFailed(string scope, string message);' +
                                         'function fetchPrinters() { return { ok: true, printers: [] } }' +
                                         'function fetchFiles() { return { ok: true, files: [] } }' +
                                         'function sendPrintOrder() { return { ok: true, taskId: "t1" } }' +
                                         'function fetchCompatiblePrintersByExt() { return { ok: true, printers: [] } }' +
                                         'function fetchCompatiblePrintersByFileId() { return { ok: true, printers: [] } }' +
                                         'function fetchReasonCatalog() { return { ok: true, reasons: [] } }' +
                                         'function fetchPrinterDetails() { return { ok: true, details: {} } }' +
                                         'function fetchPrinterProjects() { return { ok: true, projects: [] } }' +
                                         'function loadCachedPrinters() {' +
                                         '  return { ok: true, endpoint: "/mock/printers", rawJson: "{}", printers: [' +
                                         '    { id: "cached-p1", name: "Cached Printer", model: "Mono", type: "LCD", state: "READY", reason: "free", available: 1, progress: -1, elapsedSec: -1, remainingSec: -1, currentFile: "", lastSeen: "now", details: { firmwareVersion: "FW-DB-1" }, projects: [ { taskId: "t-cache-1", gcodeName: "cached.pwmb", printerId: "cached-p1", printerName: "Cached Printer", printStatus: 1, progress: 20, reason: "", createTime: 1, endTime: 0, img: "" } ] }' +
                                         '  ] }' +
                                         '}' +
                                         'function refreshPrintersAsync(force) { refreshCalls += 1; lastForce = force }' +
                                         'property int refreshCalls: 0;' +
                                         'property bool lastForce: false;' +
                                         '}', this, "printerBridgeMock")

        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml", {"width": 1280, "height": 800, "autoRefreshIntervalMs": 20})
        compare(cloudBridge.refreshCalls, 1)
        compare(cloudBridge.lastForce, true)
        compare(String(page.selectedPrinterId), "cached-p1")
        compare(String(page.selectedPrinterDetails.firmwareVersion), "FW-DB-1")

        cloudBridge.printersUpdatedFromCloud([
            { id: "cloud-p1", name: "Cloud Printer", model: "Mono X", type: "LCD", state: "PRINTING", reason: "printing", available: 1, progress: 30, elapsedSec: 300, remainingSec: 600, currentFile: "a.pwmb", lastSeen: "now", details: { firmwareVersion: "FW-CLOUD-2" }, projects: [] }
        ], "ok")
        wait(0)
        tryCompare(page, "selectedPrinterId", "cloud-p1")
        compare(String(page.selectedPrinterDetails.firmwareVersion), "FW-CLOUD-2")

        var timer = findObjectByName(page, "printersAutoRefreshTimer")
        verify(timer !== null)
        compare(timer.running, true)
        wait(65)
        verify(cloudBridge.refreshCalls >= 2)

        page.destroy()
    }
}
