import QtQuick 2.15
import QtQuick.Controls 2.15
import QtTest 1.3

TestCase {
    name: "ControlRoomUi"
    property var cloudBridge: undefined
    property var cloudFilesWorkflowBridge: undefined
    property var printWorkflowBridge: undefined
    property var mqttBridge: undefined
    property var sessionImportBridge: undefined
    property var uiSettingsBridge: undefined
    property bool accloudProdUi: false

    function cleanup() {
        if (cloudBridge !== undefined && cloudBridge !== null && cloudBridge.destroy !== undefined) {
            cloudBridge.destroy()
        }
        cloudBridge = undefined
        if (cloudFilesWorkflowBridge !== undefined && cloudFilesWorkflowBridge !== null
                && cloudFilesWorkflowBridge.destroy !== undefined) {
            cloudFilesWorkflowBridge.destroy()
        }
        cloudFilesWorkflowBridge = undefined
        if (printWorkflowBridge !== undefined && printWorkflowBridge !== null
                && printWorkflowBridge.destroy !== undefined) {
            printWorkflowBridge.destroy()
        }
        printWorkflowBridge = undefined
        if (mqttBridge !== undefined && mqttBridge !== null && mqttBridge.destroy !== undefined) {
            mqttBridge.destroy()
        }
        mqttBridge = undefined
        if (sessionImportBridge !== undefined && sessionImportBridge !== null
                && sessionImportBridge.destroy !== undefined) {
            sessionImportBridge.destroy()
        }
        sessionImportBridge = undefined
        if (uiSettingsBridge !== undefined && uiSettingsBridge !== null
                && uiSettingsBridge.destroy !== undefined) {
            uiSettingsBridge.destroy()
        }
        uiSettingsBridge = undefined
        accloudProdUi = false
    }

    function createQmlObject(path, props, parentObject) {
        var component = Qt.createComponent(path)
        if (component.status !== Component.Ready && path.indexOf("../../../ui/qml/") === 0) {
            var sourcePath = "../../../src/accloud/ui/qml/" + path.slice(String("../../../ui/qml/").length)
            component = Qt.createComponent(sourcePath)
        }
        compare(component.status, Component.Ready, "Unable to load " + path + " -> " + component.errorString())
        var object = component.createObject(parentObject ? parentObject : null, props ? props : {})
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

        var direct = [root.contentItem, root.background, root.header, root.footer, root.popupItem, root.menuBar]
        for (var d = 0; d < direct.length; ++d) {
            var directNode = direct[d]
            if (directNode !== null && directNode !== undefined) {
                var directFound = findObjectByName(directNode, name, visited)
                if (directFound !== null) {
                    return directFound
                }
            }
        }

        var collections = [root.children, root.contentChildren, root.contentData, root.actions, root.data, root.menus]
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

    function createCloudFilesWorkflowMock() {
        return Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                  'signal batchDeleteStarted(int total);' +
                                  'signal batchDeleteProgress(int completed, int total, int succeeded, string fileId, bool success);' +
                                  'signal batchDeleteFileSucceeded(string fileId);' +
                                  'signal batchDeleteFinished(var summary);' +
                                  'signal singleDeleteFinished(string fileId, var result);' +
                                  'property var deletedIds: [];' +
                                  'function startBatchDelete(files) {' +
                                  '  if (!files || files.length === 0) return false;' +
                                  '  batchDeleteStarted(files.length);' +
                                  '  var next = []; var succeeded = 0;' +
                                  '  for (var i = 0; i < files.length; ++i) {' +
                                  '    var id = String(files[i].fileId || ""); next.push(id); deletedIds = next.slice(0); succeeded += 1;' +
                                  '    batchDeleteFileSucceeded(id); batchDeleteProgress(i + 1, files.length, succeeded, id, true);' +
                                  '  }' +
                                  '  batchDeleteFinished({ requested: files.length, completed: files.length, succeeded: succeeded, failed: 0, failures: [], cancelled: false });' +
                                  '  return true;' +
                                  '}' +
                                  'function cancelBatchDelete() {}' +
                                  'function deleteSingleFile(fileId) {' +
                                  '  var id = String(fileId || ""); if (id.length === 0) return false;' +
                                  '  var next = deletedIds.slice(0); next.push(id); deletedIds = next;' +
                                  '  singleDeleteFinished(id, { ok: true, message: "ok" }); return true;' +
                                  '}' +
                                  '}', this, "cloudFilesWorkflowMock")
    }

    function createRemotePrintWorkflowMock(autoResult) {
        var mock = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                      'signal directPrintTrackingReleased(string printerId);' +
                                      'signal directCleanupNotice(int noticeKind);' +
                                      'signal remotePrintPreparationReady(var result);' +
                                      'signal remotePrintTrackingReleased(string printerId);' +
                                      'signal remoteCleanupNotice(int noticeKind, string printerId);' +
                                      'property int beginCalls: 0;' +
                                      'property int guardCalls: 0;' +
                                      'property bool autoEmit: true;' +
                                      'property var preparationResult: ({});' +
                                      'function beginRemotePrintPreparation(mode, fileId, fileName, fileData, printers, preferredPrinterId) {' +
                                      '  beginCalls += 1;' +
                                      '  if (autoEmit) remotePrintPreparationReady(preparationResult);' +
                                      '  return "workflow-request-" + String(beginCalls);' +
                                      '}' +
                                      'function evaluateRemotePrintGuard(mode, printer, fileData) {' +
                                      '  guardCalls += 1;' +
                                      '  if (!printer || String(printer.id || "").length === 0) return { ok: false, reasonKey: "select_printer", reason: "Select a printer first." };' +
                                      '  if (String(mode) !== "direct" && (!fileData || Object.keys(fileData).length === 0)) return { ok: false, reasonKey: "select_file", reason: "Select a cloud file first." };' +
                                      '  return { ok: true, reasonKey: "", reason: "" };' +
                                      '}' +
                                      'function trackDirectPrint(operation) { return true }' +
                                      'function reconcileDirectPrints(projects) {}' +
                                      'function trackRemotePrintCleanup(printerId, fileData) { return true }' +
                                      'function beginRemotePostPrintCleanup(printerId) {}' +
                                      'function cancelRemotePrintPreparation() {}' +
                                      '}', this, "remotePrintWorkflowMock")
        mock.preparationResult = autoResult || ({})
        return mock
    }

    function visibleTextExists(root, text, visited) {
        if (root === null || root === undefined) {
            return false
        }
        if (visited === null || visited === undefined) {
            visited = []
        }
        for (var v = 0; v < visited.length; ++v) {
            if (visited[v] === root) {
                return false
            }
        }
        visited.push(root)

        if (root.visible !== false && root.text !== undefined
                && String(root.text).indexOf(text) >= 0) {
            return true
        }

        var direct = [root.contentItem, root.background, root.header, root.footer, root.popupItem]
        for (var d = 0; d < direct.length; ++d) {
            if (visibleTextExists(direct[d], text, visited)) {
                return true
            }
        }

        var collections = [root.children, root.contentChildren, root.contentData, root.actions, root.data]
        for (var c = 0; c < collections.length; ++c) {
            var kids = collections[c]
            if (kids === null || kids === undefined) {
                continue
            }
            for (var i = 0; i < kids.length; ++i) {
                if (visibleTextExists(kids[i], text, visited)) {
                    return true
                }
            }
        }
        return false
    }

    function test_main_window_has_control_room_layout() {
        var window = createQmlObject("../../../ui/qml/MainWindow.qml")
        compare(window.title, "Anycubic Cloud Control Room")

        var tabs = findObjectByName(window, "controlRoomTabs")
        verify(tabs !== null)
        compare(tabs.count, 4)
        verify(findObjectByName(window, "mqttTabButton") !== null)

        var uploadButton = findObjectByName(window, "uploadDialogButton")
        verify(uploadButton !== null)

        var menuBar = findObjectByName(window, "mainMenuBar")
        verify(menuBar !== null)
        if (menuBar.contentChildren !== undefined) {
            verify(menuBar.contentChildren.length >= 3)
        }
        verify(findObjectByName(window, "render3dDefaultsDialog") === null)
        verify(findObjectByName(window, "menuSettingsRender3d") === null)
        verify(findObjectByName(window, "viewerDraftDialog") === null)
        verify(findObjectByName(window, "viewerDialogButton") === null)

        window.close()
        window.destroy()
    }

    function test_main_window_uses_compact_primary_shell() {
        var window = createQmlObject("../../../ui/qml/MainWindow.qml")
        wait(0)

        var header = findObjectByName(window, "controlRoomHeader")
        var tabsPanel = findObjectByName(window, "tabsPanel")
        var tabs = findObjectByName(window, "controlRoomTabs")
        var stack = findObjectByName(window, "controlRoomStack")
        verify(header !== null)
        verify(tabsPanel !== null)
        verify(tabs !== null)
        verify(stack !== null)
        compare(Math.round(header.height), 64)
        compare(tabs.tabSizingMode, "content")
        compare(tabs.minTabWidth, 120)
        compare(tabs.stripColor.a, 0)
        compare(tabs.inactiveColor.a, 0)
        verify(tabs.itemAt(0).width < tabs.width)
        compare(Math.round(stack.x), 1)
        compare(Math.round(stack.width), Math.round(tabsPanel.width - 2))

        window.close()
        window.destroy()

        var debugWindow = createQmlObject("../../../ui/qml/MainWindow.qml", {
            "buildDebugEnabled": true,
            "debugUi": true
        })
        wait(0)
        var debugHeader = findObjectByName(debugWindow, "controlRoomHeader")
        verify(debugHeader !== null)
        compare(Math.round(debugHeader.height), 80)

        debugWindow.close()
        debugWindow.destroy()
    }

    function test_cloud_file_details_dialog_prioritizes_user_information() {
        var hostWindow = Qt.createQmlObject('import QtQuick 2.15; import QtQuick.Controls 2.15; ApplicationWindow {' +
                                            'width: 1280; height: 860; visible: true' +
                                            '}', this, "cloudFileDetailsDialogHost")
        verify(hostWindow !== null)
        tryCompare(hostWindow, "visible", true)

        var dialog = createQmlObject("../../../ui/qml/pages/CloudFileDetailsDialog.qml", {
            "fileData": {
                "fileId": "file-42",
                "fileName": "demo_part.pwsz",
                "status": "READY",
                "statusCode": 1,
                "gcodeId": "gcode-42",
                "sizeText": "29.9 MB",
                "uploadTime": "28/07/2026",
                "createTime": "27/07/2026",
                "updateTime": "28/07/2026",
                "machine": "Anycubic Photon Mono M7 Pro",
                "material": "Resin",
                "printTime": "1h 58m",
                "layerThickness": "0.05 mm",
                "layers": 1082,
                "resinUsage": "30 ml",
                "dimensions": "54 x 54 x 81 mm",
                "bottomLayers": 5,
                "exposureTime": "2 s",
                "offTime": "0.5 s",
                "printers": "Anycubic Photon Mono M7 Pro",
                "thumbnailUrl": "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=",
                "downloadUrl": "https://signed.invalid/download?X-Amz-Signature=FAKE_TEST_VALUE",
                "region": "us-east-2",
                "bucket": "workbentch",
                "path": "file/demo_part.pwsz",
                "md5": "0123456789abcdef"
            },
            "fileTypeLabelProvider": function(fileName) { return "PWSZ" },
            "fileNameWithoutExtensionProvider": function(fileName) { return "demo_part" },
            "displayDateProvider": function(value) { return String(value || "-") },
            "buildDebugEnabled": true,
            "showAdvancedDetails": false
        }, hostWindow.contentItem)

        dialog.open()
        tryCompare(dialog, "visible", true)

        compare(String(dialog.title), "demo_part")
        verify(String(dialog.subtitle).indexOf("29.9 MB") >= 0)
        verify(String(dialog.subtitle).indexOf("Ready") >= 0)

        var thumbnail = findObjectByName(dialog, "cloudFileDetailsThumbnail")
        var compactSummary = findObjectByName(dialog, "cloudFileDetailsCompactSummaryCard")
        var tabsContainer = findObjectByName(dialog, "cloudFileDetailsTabsContainer")
        var overviewPanel = findObjectByName(dialog, "cloudFileDetailsOverviewPanel")
        var overviewFileCard = findObjectByName(dialog, "cloudFileDetailsOverviewFileCard")
        var overviewCompatibilityCard = findObjectByName(dialog, "cloudFileDetailsOverviewCompatibilityCard")
        var summaryFileNameField = findObjectByName(dialog, "cloudFileDetailsSummaryFileNameField")
        var overviewFileNameRow = findObjectByName(dialog, "cloudFileDetailsOverviewFileNameRow")
        var overviewTypeRow = findObjectByName(dialog, "cloudFileDetailsOverviewTypeRow")
        var overviewUploadedRow = findObjectByName(dialog, "cloudFileDetailsOverviewUploadedRow")
        var technicalTab = findObjectByName(dialog, "cloudFileDetailsTechnicalTab")
        var cloudMetadataTab = findObjectByName(dialog, "cloudFileDetailsCloudMetadataTab")
        var cloudMetadataPanel = findObjectByName(dialog, "cloudFileDetailsCloudMetadataPanel")
        var renameButton = findObjectByName(dialog, "cloudFileDetailsRenameButton")
        var deleteButton = findObjectByName(dialog, "cloudFileDetailsDeleteButton")
        var closeButton = findObjectByName(dialog, "cloudFileDetailsCloseButton")
        var downloadButton = findObjectByName(dialog, "cloudFileDetailsDownloadButton")
        var printButton = findObjectByName(dialog, "cloudFileDetailsPrintButton")
        verify(thumbnail !== null)
        verify(compactSummary !== null)
        verify(tabsContainer !== null)
        verify(overviewPanel !== null)
        verify(overviewFileCard !== null)
        verify(overviewCompatibilityCard !== null)
        verify(summaryFileNameField !== null)
        verify(overviewFileNameRow !== null)
        verify(overviewTypeRow !== null)
        verify(overviewUploadedRow !== null)
        verify(technicalTab !== null)
        verify(cloudMetadataTab !== null)
        verify(cloudMetadataPanel !== null)
        verify(renameButton === null)
        verify(deleteButton !== null)
        verify(closeButton !== null)
        verify(downloadButton !== null)
        verify(printButton !== null)
        verify(String(thumbnail.source).indexOf("data:image/png;base64,") === 0)
        tryVerify(function() {
            return overviewFileCard.height > overviewPanel.height * 0.75
                    && Math.abs(overviewFileCard.height
                                - overviewCompatibilityCard.height) <= 1
        }, 1000)
        tryVerify(function() {
            return Math.abs(summaryFileNameField.height - summaryFileNameField.implicitHeight) <= 1
                    && Math.abs(overviewFileNameRow.height - overviewFileNameRow.implicitHeight) <= 1
        }, 1000)
        tryVerify(function() {
            var firstGap = overviewTypeRow.y
                    - (overviewFileNameRow.y + overviewFileNameRow.height)
            return Math.abs(firstGap - 6) <= 1
                    && overviewUploadedRow.y + overviewUploadedRow.height
                       < overviewFileCard.height - 20
        }, 1000)
        tryVerify(function() {
            var deleteX = deleteButton.mapToItem(dialog.contentItem, 0, 0).x
            var closeX = closeButton.mapToItem(dialog.contentItem, 0, 0).x
            var downloadX = downloadButton.mapToItem(dialog.contentItem, 0, 0).x
            var printX = printButton.mapToItem(dialog.contentItem, 0, 0).x
            return deleteX < closeX && closeX < downloadX && downloadX < printX
        }, 1000)
        verify(closeButton.width >= 112)
        verify(downloadButton.width >= 112)
        verify(printButton.width >= 112)
        compare(technicalTab.visible, false)
        tryCompare(cloudMetadataTab, "visible", true)
        verify(!visibleTextExists(dialog, "Download URL"))
        verify(!visibleTextExists(dialog, "Thumbnail URL"))
        verify(!visibleTextExists(dialog, "signed.invalid"))

        dialog.showAdvancedDetails = true
        tryCompare(technicalTab, "visible", true)
        dialog.buildDebugEnabled = false
        tryCompare(cloudMetadataTab, "visible", false)
        dialog.close()
        tryCompare(dialog, "visible", false)
        dialog.destroy()
        hostWindow.close()
        hostWindow.destroy()
    }

    function test_cloud_file_technical_details_setting_is_persisted() {
        uiSettingsBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                               'property var values: ({});' +
                                               'function getString(key, fallback) {' +
                                               '  return values[key] !== undefined ? values[key] : fallback' +
                                               '}' +
                                               'function setString(key, value) { values[key] = value }' +
                                               'function sync() {}' +
                                               '}', this, "cloudFileDetailsSettingsMock")

        var window = createQmlObject("../../../ui/qml/MainWindow.qml")
        wait(0)
        compare(window.cloudFileAdvancedDetailsEnabled, false)
        var filesPage = findObjectByName(window, "cloudFilesPage")
        verify(filesPage !== null)
        compare(filesPage.showAdvancedDetails, false)

        window.persistCloudFileDetailsSetting(true)
        compare(window.cloudFileAdvancedDetailsEnabled, true)
        compare(String(uiSettingsBridge.values["ui.cloudFiles.showAdvancedDetails"]), "true")
        compare(filesPage.showAdvancedDetails, true)
        window.close()
        window.destroy()

        uiSettingsBridge.values = ({})
        var debugWindow = createQmlObject("../../../ui/qml/MainWindow.qml", {
            "buildDebugEnabled": true,
            "debugUi": true
        })
        wait(0)
        compare(debugWindow.cloudFileAdvancedDetailsEnabled, true)
        debugWindow.close()
        debugWindow.destroy()
    }

    function test_main_window_prod_hides_mqtt_and_logs_tabs() {
        accloudProdUi = true
        var window = createQmlObject("../../../ui/qml/MainWindow.qml")

        var mqttTab = findObjectByName(window, "mqttTabButton")
        var logTab = findObjectByName(window, "logTabButton")
        verify(mqttTab !== null)
        verify(logTab !== null)
        compare(mqttTab.visible, false)
        compare(logTab.visible, false)

        window.close()
        window.destroy()
    }

    function test_main_window_startup_triggers_mqtt_autoconnect() {
        mqttBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                        'property bool connected: false;' +
                                        'property int ensureCalls: 0;' +
                                        'function ensureAutoConnected() { ensureCalls += 1; return true }' +
                                        '}',
                                        this,
                                        "mainWindowMqttAutoConnectMock")
        sessionImportBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                                 'function checkStartup() { return { sessionExists: true, connectionOk: true, message: "ok" } }' +
                                                 '}',
                                                 this,
                                                 "mainWindowSessionStartupMock")

        var window = createQmlObject("../../../ui/qml/MainWindow.qml")
        wait(0)
        compare(mqttBridge.ensureCalls, 1)

        window.close()
        window.destroy()
    }

    function test_cloud_files_has_mock_data_and_upload_action() {
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})
        compare(page.filesModel.count, 2)

        var uploadButton = findObjectByName(page, "uploadPwmbButton")
        verify(uploadButton !== null)
        verify(uploadButton.enabled)
        verify(String(uploadButton.text).trim().length > 0)

        page.destroy()
    }

    function test_printer_details_basic_waiting_metrics() {
        var history = {
            count: 2,
            get: function(index) {
                if (index === 0) {
                    return { taskId: "t-running", gcodeName: "running.pwmb", printStatus: 1 }
                }
                return { taskId: "t-done", gcodeName: "done.pwmb", printStatus: 2 }
            }
        }
        var panel = createQmlObject("../../../ui/qml/pages/PrinterDetailPanel.qml", {
            "width": 980,
            "height": 520,
            "selectedPrinter": {
                "id": "p1",
                "name": "Anycubic Photon Mono M7 Pro",
                "model": "Anycubic Photon Mono M7 Pro",
                "type": "LCD",
                "state": "READY"
            },
            "selectedPrinterDetails": {
                "firmwareVersion": "v1.2.3",
                "printCount": "42",
                "releaseFilmLayers": 30001,
                "releaseFilmTimes": 60,
                "releaseFilmStatusCode": -1,
                "printTotalTime": "12h30m",
                "materialUsed": "1250 ml"
            },
            "printerHistoryModel": history
        })
        wait(0)

        compare(String(findObjectByName(panel, "printerFirmwareValue").text), "v1.2.3")
        compare(String(findObjectByName(panel, "printerPrintCountValue").text), "42")
        compare(String(findObjectByName(panel, "printerFilmStateValue").text),
                "60 prints | 30001 layers | Film a changer")
        compare(String(findObjectByName(panel, "printerTotalPrintTimeValue").text), "12.5 h")
        compare(String(findObjectByName(panel, "printerTotalResinValue").text), "1.25 L")
        compare(String(findObjectByName(panel, "printerLastPrintedFileValue").text), "done.pwmb")
        verify(!visibleTextExists(panel, "Model:"))
        verify(!visibleTextExists(panel, "Printer Type"))

        var detailsContent = findObjectByName(panel, "deviceDetailsContent")
        var detailsSectionHeader = findObjectByName(panel, "deviceDetailsSectionHeader")
        var basicDetailsGrid = findObjectByName(panel, "printerBasicDetailsGrid")
        var functionsCard = findObjectByName(panel, "printerFunctionsCard")
        verify(detailsContent !== null)
        verify(detailsSectionHeader !== null)
        verify(basicDetailsGrid !== null)
        verify(functionsCard !== null)
        compare(Math.round(detailsContent.y), 10)
        verify(basicDetailsGrid.y <= detailsSectionHeader.y + detailsSectionHeader.height + 10)
        verify(functionsCard.y <= basicDetailsGrid.y + basicDetailsGrid.height + 10)

        panel.destroy()
    }

    function test_printer_functions_card_visibility_and_feeding_modal_ui() {
        var panel = createQmlObject("../../../ui/qml/pages/PrinterDetailPanel.qml", {
            "width": 980,
            "height": 520,
            "selectedPrinter": {
                "id": "p1",
                "name": "Anycubic Photon Mono M7 Pro",
                "state": "READY",
                "reason": "free",
                "available": 1
            }
        })
        wait(0)

        var functionsCard = findObjectByName(panel, "printerFunctionsCard")
        verify(functionsCard !== null)
        compare(functionsCard.visible, true)
        compare(String(findObjectByName(panel, "printerFunctionFeedingButton").text), "Feeding")
        compare(String(findObjectByName(panel, "printerFunctionB1Button").text), "B1")
        compare(String(findObjectByName(panel, "printerFunctionB2Button").text), "B2")
        compare(String(findObjectByName(panel, "printerFunctionB3Button").text), "B3")

        var feedingDialog = findObjectByName(panel, "printerFeedingDialog")
        verify(feedingDialog !== null)
        compare(String(findObjectByName(panel, "printerFeedingFillButton").text), "Remplissage")
        compare(String(findObjectByName(panel, "printerFeedingDrainButton").text), "Vidage")
        compare(String(findObjectByName(panel, "printerFeedingCancelButton").text), "Cancel")

        panel.selectedPrinter = {
            "id": "p1",
            "name": "Anycubic Photon Mono M7 Pro",
            "state": "FREE",
            "available": 1
        }
        wait(0)
        compare(functionsCard.visible, true)

        panel.selectedPrinter = {
            "id": "p1",
            "name": "Anycubic Photon Mono M7 Pro",
            "state": "PRINTING",
            "reason": "printing",
            "available": 1
        }
        wait(0)
        compare(functionsCard.visible, true)

        panel.selectedPrinter = {
            "id": "p1",
            "name": "Anycubic Photon Mono M7 Pro",
            "state": "OFFLINE",
            "reason": "offline",
            "available": 0
        }
        wait(0)
        compare(functionsCard.visible, true)

        panel.destroy()
    }

    function test_cloud_files_header_and_row_columns_are_aligned() {
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})
        page.loadMockFiles()
        wait(160)

        var headerThumb = findObjectByName(page, "fileHeaderThumb")
        var headerSelect = findObjectByName(page, "fileHeaderSelect")
        var headerName = findObjectByName(page, "fileHeaderName")
        var headerType = findObjectByName(page, "fileHeaderType")
        var headerSize = findObjectByName(page, "fileHeaderSize")
        var headerDate = findObjectByName(page, "fileHeaderDate")
        var headerActions = findObjectByName(page, "fileHeaderActions")

        verify(headerSelect !== null)
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

        compare(headerSelect.width, page.colSelectWidth)
        var totalColumns = page.colSelectWidth + page.colThumbWidth + page.colNameWidth + page.colTypeWidth
                         + page.colSizeWidth + page.colDateWidth + page.colActionsWidth
                         + page.tableColumnSpacing * 6
        compare(totalColumns, page.tableViewportWidth)

        compare(headerName.horizontalAlignment, Text.AlignLeft)
        compare(headerType.horizontalAlignment, Text.AlignHCenter)

        page.destroy()
    }

    function test_cloud_files_table_uses_compact_rows_and_actions() {
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})
        page.loadMockFiles()
        wait(160)

        var row = findObjectByName(page, "cloudFilesTableRow")
        var thumb = findObjectByName(page, "fileRowThumb")
        var selectionCheckBox = findObjectByName(page, "fileRowSelectionCheckBox")
        var toolbar = findObjectByName(page, "cloudFilesToolbar")
        var primaryActionsHost = findObjectByName(page, "filesPrimaryActionsHost")
        var primaryActions = findObjectByName(page, "filesPrimaryActions")
        var tablePanel = findObjectByName(page, "filesTablePanel")
        var tableHeader = findObjectByName(page, "filesTableHeader")
        var filesList = findObjectByName(page, "filesList")
        var pagination = findObjectByName(page, "filesPaginationBar")
        verify(row !== null)
        verify(thumb !== null)
        verify(selectionCheckBox !== null)
        verify(toolbar !== null)
        verify(primaryActionsHost !== null)
        verify(primaryActions !== null)
        verify(tablePanel !== null)
        verify(tableHeader !== null)
        verify(filesList !== null)
        verify(pagination !== null)
        compare(row.height, 88)
        compare(thumb.width, 76)
        compare(thumb.height, 76)
        compare(page.colSelectWidth, 30)
        compare(page.colActionsWidth, 326)
        compare(page.actionDetailsWidth + page.actionDownloadWidth
                + page.actionPrintWidth + page.actionMenuWidth + 18, 318)
        compare(page.tableRowHorizontalMargin, 12)
        compare(tablePanel.border.width, 1)
        compare(tablePanel.tableRowHorizontalMargin, page.tableRowHorizontalMargin)
        compare(Math.round(tableHeader.x), page.tableRowHorizontalMargin)
        compare(Math.round(filesList.x), page.tableRowHorizontalMargin)
        compare(Math.round(tableHeader.width), Math.round(filesList.width))
        compare(pagination.horizontalMargin, page.tableRowHorizontalMargin)
        compare(Math.round(primaryActions.x + primaryActions.width / 2),
                Math.round(primaryActionsHost.width / 2))

        page.destroy()
    }

    function test_cloud_files_multi_selection_shows_delete_between_primary_actions() {
        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})
        page.loadMockFiles()
        wait(120)

        var refreshButton = findObjectByName(page, "refreshFilesButton")
        var deleteButton = findObjectByName(page, "deleteSelectedFilesButton")
        var uploadButton = findObjectByName(page, "uploadPwmbButton")
        var selectionCheckBox = findObjectByName(page, "fileRowSelectionCheckBox")
        verify(refreshButton !== null)
        verify(deleteButton !== null)
        verify(uploadButton !== null)
        verify(selectionCheckBox !== null)
        compare(deleteButton.visible, false)

        page.setFileSelected("demo-001", "rook_plate_v12.pwmb", true)
        wait(0)
        compare(page.selectedFilesCount, 1)
        compare(deleteButton.visible, true)
        compare(String(deleteButton.text), "Delete (1)")
        compare(selectionCheckBox.checked, true)
        var primaryActions = findObjectByName(page, "filesPrimaryActions")
        verify(primaryActions !== null)
        verify(refreshButton.parent === primaryActions)
        verify(deleteButton.parent === primaryActions)
        verify(uploadButton.parent === primaryActions)

        var refreshIndex = -1
        var deleteIndex = -1
        var uploadIndex = -1
        for (var actionIndex = 0; actionIndex < primaryActions.children.length; ++actionIndex) {
            var action = primaryActions.children[actionIndex]
            if (action === refreshButton)
                refreshIndex = actionIndex
            else if (action === deleteButton)
                deleteIndex = actionIndex
            else if (action === uploadButton)
                uploadIndex = actionIndex
        }
        verify(refreshIndex >= 0)
        verify(deleteIndex > refreshIndex)
        verify(uploadIndex > deleteIndex)

        page.setFileSelected("demo-002", "calibration_tower.pws", true)
        wait(0)
        compare(page.selectedFilesCount, 2)
        compare(String(deleteButton.text), "Delete (2)")

        page.clearFileSelection()
        wait(0)
        compare(page.selectedFilesCount, 0)
        compare(deleteButton.visible, false)

        page.destroy()
    }

    function test_cloud_files_batch_delete_uses_workflow_bridge_sequential_result() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                        'function fetchQuota() { return { ok: true, totalBytes: 2000, usedBytes: 1000, totalDisplay: "2 KB", usedDisplay: "1 KB" } }' +
                                        'function fetchFiles() { return { ok: true, files: [' +
                                        '{ fileId: "demo-001", fileName: "rook_plate_v12.pwmb", sizeText: "1 MB", uploadTime: "2026-03-05" },' +
                                        '{ fileId: "demo-002", fileName: "calibration_tower.pws", sizeText: "1 MB", uploadTime: "2026-03-05" }' +
                                        '] } }' +
                                        '}', this, "cloudFilesBatchDeleteCloudMock")
        cloudFilesWorkflowBridge = createCloudFilesWorkflowMock()

        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})
        wait(80)
        page.setFileSelected("demo-001", "rook_plate_v12.pwmb", true)
        page.setFileSelected("demo-002", "calibration_tower.pws", true)
        compare(page.selectedFilesCount, 2)

        page.startBatchDelete()
        tryCompare(page, "batchDeleteRunning", false, 1000)
        compare(cloudFilesWorkflowBridge.deletedIds.length, 2)
        compare(String(cloudFilesWorkflowBridge.deletedIds[0]), "demo-001")
        compare(String(cloudFilesWorkflowBridge.deletedIds[1]), "demo-002")
        compare(page.batchDeleteCompleted, 2)
        compare(page.batchDeleteSucceeded, 2)
        compare(page.selectedFilesCount, 0)
        compare(String(page.statusMsg), "Deleted 2 file(s).")

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

    function test_inline_status_bar_hides_operation_id_outside_debug() {
        var bar = createQmlObject("../../../ui/qml/components/InlineStatusBar.qml", {
                                      "message": "Printer refreshed",
                                      "severity": "success",
                                      "operationId": "op_printer_refresh"
                                  })
        var opId = findObjectByName(bar, "inlineStatusOperationId")
        verify(opId !== null)
        compare(opId.visible, false)

        bar.showOperationId = true
        compare(opId.visible, true)
        compare(String(opId.text), "op_printer_refresh")
        bar.destroy()
    }

    function test_printer_visual_tokens_and_modernized_controls() {
        var panel = createQmlObject("../../../ui/qml/pages/PrinterDetailPanel.qml", {
                                        "width": 900,
                                        "height": 560,
                                        "selectedPrinter": {
                                            id: "p1",
                                            name: "Printer One",
                                            model: "Mono M7",
                                            state: "PRINTING",
                                            progress: 42,
                                            currentLayer: 84,
                                            totalLayers: 200,
                                            elapsedSec: 600,
                                            remainingSec: 900,
                                            currentFile: "demo.pwmb",
                                            details: {
                                                firmwareVersion: "FW-NESTED",
                                                printCount: "12",
                                                releaseFilmLayers: 42,
                                                releaseFilmTimes: 3,
                                                releaseFilmStatusCode: 0,
                                                printTotalTime: "10h30m",
                                                materialUsed: 250,
                                                lastPrintedFile: "done.pwmb"
                                            }
                                        }
                                    })
        var runningStatus = panel.recentJobStatusInfo(1)
        var failedStatus = panel.recentJobStatusInfo(3)
        var activeCloudCanceledStatus = panel.recentJobStatusInfo(4, "task-active", "p1")
        verify(String(runningStatus.bg).length > 0)
        verify(String(runningStatus.border).length > 0)
        verify(String(failedStatus.bg).length > 0)
        verify(String(failedStatus.border).length > 0)
        compare(String(failedStatus.label), "Failed")
        compare(String(activeCloudCanceledStatus.label), "Canceled")

        panel.selectedLiveJobData = { taskId: "task-active", printerId: "p1", printStatus: 3 }
        var matchingFailedStatus = panel.recentJobStatusInfo(3, "task-active", "p1")
        activeCloudCanceledStatus = panel.recentJobStatusInfo(4, "task-active", "p1")
        compare(String(matchingFailedStatus.label), "Failed")
        compare(String(activeCloudCanceledStatus.label), "Canceled")

        panel.selectedLiveJobData = { taskId: "task-active", printerId: "p1", printStatus: 1 }
        activeCloudCanceledStatus = panel.recentJobStatusInfo(4, "task-active", "p1")
        compare(String(activeCloudCanceledStatus.label), "In progress")

        panel.selectedLiveJobData = ({})
        panel.selectedPrinter.details = {
            mqttJobStage: "finished",
            mqttPrintState: "finished",
            mqttActiveTaskId: "task-active"
        }
        activeCloudCanceledStatus = panel.recentJobStatusInfo(4, "task-active", "p1")
        compare(String(activeCloudCanceledStatus.label), "Canceled")

        panel.selectedPrinter.details = {
            mqttJobStage: "checking",
            mqttPrintState: "monitoring",
            mqttActiveTaskId: "task-active"
        }
        activeCloudCanceledStatus = panel.recentJobStatusInfo(4, "task-active", "p1")
        compare(String(activeCloudCanceledStatus.label), "In progress")

        var progressBar = findObjectByName(panel, "printerCurrentPrintProgressBar")
        verify(progressBar !== null)
        verify(Math.abs(progressBar.value - 0.42) < 0.001)
        var headerStatusChip = findObjectByName(panel, "printerHeaderStatusChip")
        verify(headerStatusChip !== null)
        panel.selectedPrinter.mqttPrintState = "finished"
        verify(String(panel.printerDisplayStatus()).toLowerCase() !== "finished")
        panel.selectedPrinter.details = { mqttPrintState: "finished" }
        verify(String(panel.printerDisplayStatus()).toLowerCase() !== "finished")
        panel.selectedPrinter.details = {
            mqttJobStage: "downloading",
            mqttPrintState: "downloading",
            mqttActiveTaskId: "task-1",
            mqttDownloadProgress: 42
        }
        var workflowRows = panel.workflowStatusRows()
        compare(workflowRows.length, 5)
        compare(String(workflowRows[0].label), "Task")
        for (var workflowIndex = 0; workflowIndex < workflowRows.length; ++workflowIndex) {
            verify(String(workflowRows[workflowIndex].label) !== "Command")
            verify(String(workflowRows[workflowIndex].label) !== "Finished")
        }
        panel.selectedPrinter.details = {
            mqttJobStage: "checking",
            mqttPrintState: "monitoring",
            mqttActiveTaskId: "task-active",
            mqttAutoChecks: { levelling: -1, platform: 0 }
        }
        compare(String(panel.checkStatus()), "wait")
        compare(String(panel.checkSummary("checking")), "levelling=-1")
        compare(panel.currentCheckIssues().length, 0)
        panel.selectedPrinter.details = {
            mqttJobStage: "checking",
            mqttPrintState: "monitoring",
            mqttActiveTaskId: "task-active",
            mqttAutoChecks: { levelling: 1 }
        }
        compare(String(panel.checkStatus()), "stop")
        panel.selectedPrinter.details = {
            firmwareVersion: "FW-NESTED",
            printCount: "12",
            releaseFilmLayers: 42,
            releaseFilmTimes: 3,
            releaseFilmStatusCode: 0,
            printTotalTime: "10h30m",
            materialUsed: 250,
            lastPrintedFile: "done.pwmb"
        }
        panel.selectedPrinterDetails = {
            firmwareVersion: "-",
            printCount: "-",
            printTotalTime: "-",
            materialUsed: "-"
        }
        var fallbackDetails = panel.effectiveBasicDetails()
        compare(String(fallbackDetails.firmwareVersion), "FW-NESTED")
        compare(String(panel.detailText(fallbackDetails, "firmwareVersion")), "FW-NESTED")
        compare(String(panel.printCountText(fallbackDetails)), "12")
        compare(String(panel.normalizedTotalPrintHoursText(fallbackDetails)), "10.5 h")
        compare(String(panel.normalizedTotalResinText(fallbackDetails)), "0.25 L")
        var jobModel = Qt.createQmlObject('import QtQuick 2.15; ListModel {}', panel, "recentJobsTestModel")
        jobModel.append({
                            taskId: "task-dev-only",
                            printerId: "p1",
                            gcodeName: "demo.pwmb",
                            printStatus: 2,
                            createTime: 100,
                            endTime: 220
                        })
        panel.timeTextProvider = function(seconds) { return String(Math.floor(Number(seconds) / 60)) + " min" }
        panel.printerHistoryModel = jobModel
        panel.developmentBuild = false
        wait(0)
        verify(findObjectByName(panel, "recentJobTaskId") === null)

        var headerFile = findObjectByName(panel, "recentJobsHeaderFile")
        var headerDate = findObjectByName(panel, "recentJobsHeaderDate")
        var headerDuration = findObjectByName(panel, "recentJobsHeaderDuration")
        var headerStatus = findObjectByName(panel, "recentJobsHeaderStatus")
        var jobDate = findObjectByName(panel, "recentJobDate")
        var jobDuration = findObjectByName(panel, "recentJobDuration")
        var jobStatus = findObjectByName(panel, "recentJobStatusBadge")
        verify(headerFile !== null)
        verify(headerDate !== null)
        verify(headerDuration !== null)
        verify(headerStatus !== null)
        verify(jobDate !== null)
        verify(jobDuration !== null)
        verify(jobStatus !== null)
        compare(String(headerDate.text), "Date")
        compare(String(headerDuration.text), "Duration")
        compare(headerDate.width, jobDate.width)
        compare(headerDuration.width, jobDuration.width)
        compare(headerStatus.width, jobStatus.width)
        compare(String(jobDuration.text), "2 min")
        compare(String(jobDate.text).length, 10)
        verify(String(jobDate.text).indexOf(":") === -1)
        panel.developmentBuild = true
        tryVerify(function() { return findObjectByName(panel, "recentJobTaskId") !== null }, 250)
        panel.developmentBuild = false
        tryVerify(function() { return findObjectByName(panel, "recentJobTaskId") === null }, 250)

        var detailHeader = findObjectByName(panel, "printerDetailHeader")
        var contentRow = findObjectByName(panel, "printerContentRow")
        var localFilesButton = findObjectByName(panel, "printerHeaderLocalFilesButton")
        verify(detailHeader !== null)
        verify(contentRow !== null)
        verify(localFilesButton !== null)
        verify(findObjectByName(panel, "printerHeaderDetailsButton") === null)
        verify(detailHeader.y < panel.height / 4)
        verify(contentRow.y < panel.height / 3)
        compare(String(localFilesButton.text), "Local Files")
        verify(localFilesButton.x < headerStatusChip.x)
        verify(!visibleTextExists(panel, "From Local File"))
        verify(!visibleTextExists(panel, "Printer One"))
        var requestedLocalPrinterId = ""
        panel.localFileRequested.connect(function(printerId) { requestedLocalPrinterId = printerId })
        localFilesButton.clicked()
        compare(requestedLocalPrinterId, "p1")
        panel.statusChipTextProvider = function(state) {
            return String(state || "").toUpperCase() === "OFFLINE" ? "Hors ligne" : "Prêt"
        }
        panel.selectedPrinter = {
            id: "p1",
            name: "Printer One",
            model: "Mono M7",
            state: "READY",
            available: 1,
            details: ({})
        }
        tryCompare(headerStatusChip, "status", "Prêt")
        compare(String(headerStatusChip.toneStatus), "READY")
        var readyReferenceChip = createQmlObject("../../../ui/qml/components/StatusChip.qml", {
                                                     "status": "READY"
                                                 })
        compare(String(headerStatusChip.toneColor()), String(readyReferenceChip.toneColor()))
        compare(String(headerStatusChip.color), String(readyReferenceChip.color))
        readyReferenceChip.destroy()

        panel.selectedPrinter = {
            id: "p1",
            name: "Printer One",
            model: "Mono M7",
            state: "OFFLINE",
            available: 0,
            details: ({})
        }
        panel.localFilePrintEnabled = false
        panel.localFilePrintBlockReason = "Printer offline."
        tryCompare(headerStatusChip, "status", "Hors ligne")
        compare(String(headerStatusChip.toneStatus), "OFFLINE")
        compare(localFilesButton.enabled, false)
        compare(String(localFilesButton.disabledStatus), "offline")
        compare(String(localFilesButton.background.color), String(headerStatusChip.color))
        compare(String(localFilesButton.ToolTip.text), "Printer offline.")
        requestedLocalPrinterId = ""
        localFilesButton.clicked()
        compare(requestedLocalPrinterId, "")
        panel.destroy()

        var fleetModel = Qt.createQmlObject('import QtQuick 2.15; ListModel {}', this, "printerFleetTestModel")
        fleetModel.append({ id: "p1", name: "Ready Printer", state: "READY", available: 1 })
        fleetModel.append({ id: "p2", name: "Printing Printer", state: "PRINTING", available: 1 })
        fleetModel.append({ id: "p3", name: "Offline Printer", state: "OFFLINE", available: 0 })
        var mainPanel = createQmlObject("../../../ui/qml/pages/PrinterMainPanel.qml", {
                                           "width": 1100,
                                           "height": 640,
                                           "printersModel": fleetModel,
                                           "selectedPrinterId": "p1",
                                           "selectedPrinter": fleetModel.get(0),
                                           "developmentBuild": false
                                       })
        var debugToggle = findObjectByName(mainPanel, "debugLabelsToggle")
        verify(debugToggle === null)
        verify(findObjectByName(mainPanel, "endpointJsonPanel") === null)
        verify(findObjectByName(mainPanel, "printerTabDetailsButton") === null)
        verify(findObjectByName(mainPanel, "printerFleetTotal") !== null)
        verify(findObjectByName(mainPanel, "printerFleetOnline") !== null)
        verify(findObjectByName(mainPanel, "printerFleetPrinting") !== null)
        verify(findObjectByName(mainPanel, "printerFleetOffline") !== null)
        var fleetCounts = mainPanel.calculateFleetCounts()
        compare(Number(fleetCounts.total), 3)
        compare(Number(fleetCounts.online), 2)
        compare(Number(fleetCounts.printing), 1)
        compare(Number(fleetCounts.offline), 1)
        mainPanel.destroy()
        fleetModel.destroy()
    }

    function test_log_page_dynamic_sources_and_filters() {
        var page = createQmlObject("../../../ui/qml/pages/LogPage.qml", {"width": 1280, "height": 800})
        page.logBackend = {
            fetchSnapshot: function(maxLines) {
                return {
                    ok: true,
                    message: "ok",
                    sources: ["app", "fault", "printer"],
                    components: ["bootstrap", "printer_agent", "mqtt_session"],
                    events: ["startup", "refresh_failed", "mqtt_state_changed"],
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
                        },
                        {
                            sink: "cloud",
                            ts: "2026-03-04T10:00:02.000+01:00",
                            level: "INFO",
                            source: "cloud",
                            component: "mqtt_session",
                            event: "mqtt_state_changed",
                            opId: "",
                            message: "connected",
                            formatted: "2026-03-04T10:00:02.000+01:00 [cloud] cloud INFO mqtt_session.mqtt_state_changed - connected"
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
        verify(sourceFilter.find("mqtt") !== -1)
        verify(componentFilter.find("printer_agent") !== -1)
        verify(eventFilter.find("refresh_failed") !== -1)

        sourceFilter.currentIndex = sourceFilter.find("printer")
        verify(logsArea.text.indexOf("[printer]") !== -1)
        verify(logsArea.text.indexOf("[app]") === -1)

        opIdFilter.text = "op_printer_42"
        verify(logsArea.text.indexOf("op_id=op_printer_42") !== -1)

        opIdFilter.text = "op_printer_missing"
        compare(logsArea.text.trim(), "")

        opIdFilter.clear()
        sourceFilter.currentIndex = sourceFilter.find("mqtt")
        verify(logsArea.text.indexOf("mqtt_session") !== -1)
        verify(logsArea.text.indexOf("[printer]") === -1)

        var pollTimer = findObjectByName(page, "logPollTimer")
        verify(pollTimer !== null)
        compare(pollTimer.running, true)
        page.pageActive = false
        compare(pollTimer.running, false)
        page.pageActive = true
        compare(pollTimer.running, true)
        page.visible = false
        compare(pollTimer.running, false)

        page.destroy()
    }

    function test_mqtt_page_bounds_diagnostics_by_visibility() {
        mqttBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'property string status: "idle";' +
                                         'property string subscribedTopics: "";' +
                                         'property var receivedTopics: [];' +
                                         'property int messageTick: 0;' +
                                         'property string telemetrySnapshot: "";' +
                                         'property var tailModel: null;' +
                                         'property int diagnosticsCalls: 0;' +
                                         'property bool diagnosticsActive: false;' +
                                         'function setUiDiagnosticsActive(active) {' +
                                         '  diagnosticsCalls += 1;' +
                                         '  diagnosticsActive = active === true;' +
                                         '}' +
                                         'function messagesForTopic(topic) { return ""; }' +
                                         'function connectRaw() { return true; }' +
                                         'function disconnectRaw() {}' +
                                         'function clearRaw() {}' +
                                         '}', this)

        var page = createQmlObject("../../../ui/qml/pages/MqttPage.qml", {
                                       "width": 1280,
                                       "height": 800,
                                       "pageActive": true
                                   })
        compare(mqttBridge.diagnosticsActive, true)
        verify(mqttBridge.diagnosticsCalls >= 1)

        page.pageActive = false
        compare(mqttBridge.diagnosticsActive, false)
        page.pageActive = true
        compare(mqttBridge.diagnosticsActive, true)

        page.destroy()
        wait(0)
        compare(mqttBridge.diagnosticsActive, false)
    }

    function test_mqtt_page_shows_only_runtime_connection_fields() {
        var page = createQmlObject("../../../ui/qml/pages/MqttPage.qml", {"width": 1280, "height": 800})

        // Runtime connection details are now managed by MqttBridge; manual inputs stay hidden.
        verify(findObjectByName(page, "mqttHostField") === null)
        verify(findObjectByName(page, "mqttPortField") === null)
        verify(findObjectByName(page, "mqttTlsCheck") === null)
        verify(findObjectByName(page, "mqttTopicsField") === null)
        verify(findObjectByName(page, "mqttClientIdField") === null)
        verify(findObjectByName(page, "mqttUsernameField") === null)
        verify(findObjectByName(page, "mqttPasswordField") === null)

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

    function test_printer_page_cloud_picker_filters_with_local_metadata_only() {
        var sendCalls = 0
        var fetchFilesCalls = 0
        var loadCachedFilesCalls = 0
        var compatByExtCalls = 0
        var compatByFileCalls = 0
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
                fetchFilesCalls += 1
                return {
                    ok: true,
                    files: [
                        {
                            fileId: "f1",
                            fileName: "demo.pwmb",
                            machine: "Anycubic Photon M3 Plus",
                            sizeText: "1 MB",
                            status: "READY",
                            printTime: "1m",
                            resinUsage: "1 ml"
                        }
                    ]
                }
            },
            loadCachedFiles: function() {
                loadCachedFilesCalls += 1
                return {
                    ok: true,
                    files: [
                        {
                            fileId: "f1",
                            fileName: "demo.pwmb",
                            machine: "Anycubic Photon M3 Plus",
                            sizeText: "1 MB",
                            status: "READY",
                            printTime: "1m",
                            resinUsage: "1 ml"
                        }
                    ]
                }
            },
            fetchCompatiblePrintersByExt: function(ext) {
                compatByExtCalls += 1
                return { ok: true, printers: [] }
            },
            fetchCompatiblePrintersByFileId: function(fileId) {
                compatByFileCalls += 1
                return { ok: true, printers: [] }
            },
            evaluateLocalPrinterFileCompatibility: function(printer, file) {
                return { ok: false, score: 0, reason: "Slice file does not match selected printer model.", reasonKey: "model_mismatch" }
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
        wait(0)
        compare(loadCachedFilesCalls, 1)
        compare(fetchFilesCalls, 0)
        compare(compatByExtCalls, 0)
        compare(compatByFileCalls, 0)
        compare(String(page.selectedCloudFileId), "")
        compare(page.selectedCloudFileData(), null)
        verify(String(page.statusMsg).indexOf("No compatible cloud file") !== -1)
        page.openRemotePrintConfig()
        compare(page.remotePrintAllowed, true)

        page.startRemotePrint()
        compare(sendCalls, 0)
        compare(page.remotePrintAllowed, false)
        compare(String(page.remotePrintBlockReason), "Select a cloud file first.")
        page.destroy()
        cloudBridge = undefined
    }

    function test_printer_page_open_remote_print_from_file_prefers_compatible_printer() {
        var printerProjectsCalls = 0
        cloudBridge = {
            fetchPrinters: function() {
                return {
                    ok: true,
                    message: "ok",
                    endpoint: "/mock/printers",
                    rawJson: "{}",
                    printers: [
                        { id: "p1", name: "Printer One", model: "Mono M7", type: "LCD", state: "READY", reason: "free", available: 1, progress: -1, elapsedSec: -1, remainingSec: -1, currentFile: "", lastSeen: "now" },
                        { id: "p2", name: "Printer Two", model: "Mono M5s", type: "LCD", state: "READY", reason: "free", available: 1, progress: -1, elapsedSec: -1, remainingSec: -1, currentFile: "", lastSeen: "now" }
                    ]
                }
            },
            fetchFiles: function() { return { ok: true, files: [] } },
            fetchReasonCatalog: function() { return { ok: true, reasons: [] } },
            fetchPrinterDetails: function() { return { ok: true, details: {} } },
            fetchPrinterProjects: function() { printerProjectsCalls += 1; return { ok: true, projects: [] } },
            sendPrintOrder: function() { return { ok: true, taskId: "123" } }
        }
        printWorkflowBridge = createRemotePrintWorkflowMock({
            requestId: "workflow-request-1",
            fileId: "f-route-1",
            fileName: "route_file.pwmb",
            compatibilityResult: { ok: true, printers: [ { id: "p2", available: 1, reason: "" } ] },
            compatiblePrinters: [ { id: "p2", name: "Printer Two", model: "Mono M5s", state: "READY", machineType: "" } ],
            selectedPrinterId: "p2",
            allowed: true,
            blockReason: "",
            blockReasonKey: "",
            bestEffortWarning: false
        })

        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml",
                                   {"width": 1280, "height": 800,
                                    "deferStartupInitialization": true})
        page.selectedPrinterId = "p1"
        page.openRemotePrintFromFile("f-route-1", "route_file.pwmb", {
                                         fileId: "f-route-1",
                                         fileName: "route_file.pwmb",
                                         printTime: "01h 10m",
                                         resinUsage: "24 ml"
                                     })

        compare(page.remotePrintPreparing, true)
        var dialog = findObjectByName(page, "remotePrintConfigDialog")
        var printerCombo = findObjectByName(dialog, "remotePrinterCombo")
        verify(dialog !== null)
        verify(printerCombo !== null)
        compare(String(dialog.selectedPrintTime), "01h 10m")
        compare(String(dialog.selectedResinUsage), "24 ml")
        compare(String(printerCombo.displayText), "Checking printer compatibility...")

        tryCompare(page, "remotePrintPreparing", false, 1000)
        compare(printWorkflowBridge.beginCalls, 1)
        compare(String(page.remotePrinterId), "p2")
        compare(String(printerCombo.displayText), "Printer Two")
        var compatiblePrinterModel = printerCombo.model
        verify(compatiblePrinterModel !== null)
        tryCompare(compatiblePrinterModel, "count", 1, 1000)
        compare(String(compatiblePrinterModel.get(0).id), "p2")
        compare(String(compatiblePrinterModel.get(0).name), "Printer Two")
        compare(printerProjectsCalls, 0)
        compare(String(page.selectedCloudFileId), "f-route-1")
        var selectedFile = page.selectedCloudFileData()
        verify(selectedFile !== null)
        compare(String(selectedFile.printTime), "01h 10m")
        compare(String(selectedFile.resinUsage), "24 ml")
        verify(String(page.statusMsg).indexOf("Remote print prepared for") === 0)

        page.destroy()
    }

    function test_printer_page_remote_print_compatibility_uses_workflow_bridge() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'property int asyncByFileCalls: 0;' +
                                         'property int asyncByExtCalls: 0;' +
                                         'function fetchPrinters() { return { ok: true, printers: [' +
                                         '{ id: "p1", name: "Printer One", model: "Mono M7", type: "LCD", state: "READY", reason: "free", available: 1, progress: -1, elapsedSec: -1, remainingSec: -1, currentFile: "", lastSeen: "now" },' +
                                         '{ id: "p2", name: "Printer Two", model: "Mono M5s", type: "LCD", state: "READY", reason: "free", available: 1, progress: -1, elapsedSec: -1, remainingSec: -1, currentFile: "", lastSeen: "now" }' +
                                         '] } }' +
                                         'function fetchFiles() { return { ok: true, files: [] } }' +
                                         'function fetchReasonCatalog() { return { ok: true, reasons: [] } }' +
                                         'function fetchPrinterDetails() { return { ok: true, details: {} } }' +
                                         'function fetchPrinterProjects() { return { ok: true, projects: [] } }' +
                                         'function sendPrintOrder() { return { ok: true, taskId: "123" } }' +
                                         'function fetchCompatiblePrintersByFileIdAsync() { asyncByFileCalls += 1 }' +
                                         'function fetchCompatiblePrintersByExtAsync() { asyncByExtCalls += 1 }' +
                                         '}', this, "printerCompatibilityTransportMock")
        printWorkflowBridge = createRemotePrintWorkflowMock(({}))
        printWorkflowBridge.autoEmit = false

        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml", {"width": 1280, "height": 800})
        page.selectedPrinterId = "p1"
        page.openRemotePrintFromFile("f-stale", "stale_file.pwmb")
        wait(0)
        page.openRemotePrintFromFile("f-async", "route_file.pwmb")
        wait(0)

        compare(printWorkflowBridge.beginCalls, 2)
        compare(cloudBridge.asyncByFileCalls, 0)
        compare(cloudBridge.asyncByExtCalls, 0)
        compare(page.remotePrintPreparing, true)

        printWorkflowBridge.remotePrintPreparationReady({
            requestId: "workflow-request-2",
            fileId: "f-async",
            fileName: "route_file.pwmb",
            compatibilityResult: { ok: true, printers: [ { id: "p2", available: 1, reason: "" } ] },
            compatiblePrinters: [ { id: "p2", name: "Printer Two", model: "Mono M5s", state: "READY", machineType: "" } ],
            selectedPrinterId: "p2",
            allowed: true,
            blockReason: "",
            blockReasonKey: "",
            bestEffortWarning: false
        })
        wait(0)

        compare(page.remotePrintPreparing, false)
        compare(String(page.remotePrinterId), "p2")
        compare(String(page.selectedPrinterId), "p2")
        verify(String(page.statusMsg).indexOf("Remote print prepared for") === 0)
        page.destroy()
    }

    function test_remote_post_print_cleanup_uses_semantic_workflow_signals() {
        printWorkflowBridge = createRemotePrintWorkflowMock(({}))
        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml",
                                   {"width": 1280, "height": 800,
                                    "deferStartupInitialization": true})
        page.markRemotePrintAccepted("p1", {
                                         fileId: "cloud-remote-ui",
                                         fileName: "remote-ui.pwmb",
                                         deleteAfterPrint: true
                                     }, "task-remote-ui")
        verify(page.pendingRemotePrintForPrinter("p1") !== null)

        printWorkflowBridge.remoteCleanupNotice(4, "p1")
        wait(0)
        verify(String(page.statusMsg).indexOf("File deleted locally and in cloud") !== -1)
        printWorkflowBridge.remotePrintTrackingReleased("p1")
        wait(0)
        compare(page.pendingRemotePrintForPrinter("p1"), null)

        page.destroy()
    }

    function test_printer_local_file_modal_lists_deletes_and_starts() {
        mqttBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject { property bool connected: true }',
                                        this,
                                        "printerMqttBridgeMock")
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'signal filesUpdatedFromCloud(var files, string message);' +
                                         'signal printersUpdatedFromCloud(var printers, string message);' +
                                         'signal printerInsightsUpdatedFromCloud(string printerId, var details, var projects, string detailsRawJson, string projectsRawJson, string message);' +
                                         'signal syncFailed(string scope, string message);' +
                                         'property var orderCalls: [];' +
                                         'function fetchQuota() { return { ok: true, totalBytes: 0, usedBytes: 0 } }' +
                                         'function fetchPrinters() { return { ok: true, message: "ok", printers: [ { id: "p1", name: "Printer One", model: "Mono M7", type: "LCD", state: "READY", reason: "free", available: 1, progress: -1, elapsedSec: -1, remainingSec: -1, currentFile: "", lastSeen: "now" } ] } }' +
                                         'function fetchFiles(page, limit) { return { ok: true, files: [] } }' +
                                         'function fetchCompatiblePrintersByExt(ext) { return { ok: true, printers: [] } }' +
                                         'function fetchCompatiblePrintersByFileId(fileId) { return { ok: true, printers: [] } }' +
                                         'function fetchReasonCatalog() { return { ok: true, reasons: [] } }' +
                                         'function fetchPrinterDetails(printerId) { return { ok: true, details: {} } }' +
                                         'function fetchPrinterProjects(printerId, page, limit) { return { ok: true, projects: [] } }' +
                                         'function sendPrintOrder() { return { ok: true, taskId: "123" } }' +
                                         'function sendPrinterOrder(printerId, orderId, data, projectId) { orderCalls.push({ printerId: String(printerId), orderId: Number(orderId), data: data, projectId: String(projectId || "") }); return { ok: true, msgId: "msg-" + String(orderId) } }' +
                                         '}', this, "printerLocalFileBridgeMock")

        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml", {"width": 1280, "height": 800})
        compare(page.localFilePrintEnabled, true)
        compare(page.localFileStartPrintOrderId, 1)
        compare(page.localFilesListOrderId, 103)
        compare(page.localFileDeleteOrderId, 104)

        page.selectedPrinterDetails = {
            firmwareVersion: "4.0.8.6",
            printCount: "88",
            printTotalTime: "132hour33min",
            materialUsed: "3264.23ml",
            releaseFilmLayers: "9884"
        }
        page.selectedPrinterLiveSnapshot = {
            id: "p1",
            printerKey: "printer-key-1",
            machineType: "128",
            name: "Printer One",
            model: "Mono M7",
            type: "LCD",
            state: "OFFLINE",
            reason: "offline",
            available: 0,
            currentFile: "cube",
            details: ({})
        }
        tryCompare(page, "localFilePrintEnabled", false)
        compare(String(page.localFilePrintBlockReason), "Printer offline.")
        verify(findObjectByName(page, "printerDetailsDialog") === null)

        page.selectedPrinterLiveSnapshot = {
            id: "p1",
            name: "Printer One",
            model: "Mono M7",
            type: "LCD",
            state: "READY",
            reason: "free",
            available: 1,
            currentFile: "",
            details: ({})
        }
        tryCompare(page, "localFilePrintEnabled", true)
        compare(String(page.localFilePrintBlockReason), "")
        page.openLocalFileDialogForRemotePrint("p1")
        compare(cloudBridge.orderCalls.length, 2)
        compare(cloudBridge.orderCalls[0].orderId, 1231)
        compare(cloudBridge.orderCalls[1].orderId, 103)

        var dialog = findObjectByName(page, "selectLocalPrinterFileDialog")
        verify(dialog !== null)
        compare(dialog.deleteEnabled, true)

        page.applyPrinterLocalFilesFromMqtt("p1", "local", [
            { filename: "plate-a.pwmb", size: 2048, timestamp: 0, isDir: false },
            { fileName: "plate-c.pwmb", size: 3072, timestamp: 0, is_dir: 0 },
            { filename: "folder", size: 0, timestamp: 0, isDir: true }
        ], "done", 0, "")
        wait(0)

        var localModel = findObjectByName(page, "printerLocalFilesModel")
        var cloudPrintModel = findObjectByName(page, "printCloudFilesModel")
        var compatiblePrintersModel = findObjectByName(page, "remoteCompatiblePrintersModel")
        verify(localModel !== null)
        verify(cloudPrintModel !== null)
        verify(compatiblePrintersModel !== null)
        verify(typeof localModel.replaceOrPatchFiles === "function")
        verify(typeof cloudPrintModel.replaceOrPatchFiles === "function")
        verify(typeof compatiblePrintersModel.replaceOrPatchPrinters === "function")
        compare(localModel.count, 2)
        compare(String(localModel.get(0).fileName), "plate-a.pwmb")
        compare(String(localModel.get(1).fileName), "plate-c.pwmb")
        compare(String(page.selectedPrinterLocalFileName), "plate-a.pwmb")

        page.deletePrinterLocalFile("plate-a.pwmb")
        compare(cloudBridge.orderCalls.length, 3)
        compare(cloudBridge.orderCalls[2].orderId, 104)
        compare(String(cloudBridge.orderCalls[2].data.filename), "plate-a.pwmb")
        compare(localModel.count, 1)
        compare(String(page.selectedPrinterLocalFileName), "plate-c.pwmb")
        verify(String(page.statusMsg).indexOf("Local file deleted") === 0)

        page.applyPrinterLocalFilesFromMqtt("p1", "local", [
            { filename: "plate-b.pwmb", size: 4096, timestamp: 0, isDir: false }
        ], "done", 0, "")
        page.startPrintFromPrinterLocalFile()
        compare(cloudBridge.orderCalls.length, 4)
        compare(cloudBridge.orderCalls[3].orderId, 1)
        compare(String(cloudBridge.orderCalls[3].data.filename), "plate-b.pwmb")
        verify(String(page.statusMsg).indexOf("Local print task sent") === 0)

        page.destroy()
        cloudBridge = undefined
        mqttBridge = undefined
    }

    function test_printer_local_file_modal_autoconnects_mqtt_then_requests_list() {
        mqttBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                        'property bool connected: false;' +
                                        'property int ensureCalls: 0;' +
                                        'function ensureAutoConnected() { ensureCalls += 1; return true }' +
                                        '}',
                                        this,
                                        "printerMqttAutoConnectMock")
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'signal filesUpdatedFromCloud(var files, string message);' +
                                         'signal printersUpdatedFromCloud(var printers, string message);' +
                                         'signal printerInsightsUpdatedFromCloud(string printerId, var details, var projects, string detailsRawJson, string projectsRawJson, string message);' +
                                         'signal syncFailed(string scope, string message);' +
                                         'property var orderCalls: [];' +
                                         'function fetchQuota() { return { ok: true, totalBytes: 0, usedBytes: 0 } }' +
                                         'function fetchPrinters() { return { ok: true, message: "ok", printers: [ { id: "p1", name: "Printer One", model: "Mono M7", type: "LCD", state: "READY", reason: "free", available: 1, progress: -1, elapsedSec: -1, remainingSec: -1, currentFile: "", lastSeen: "now" } ] } }' +
                                         'function fetchFiles(page, limit) { return { ok: true, files: [] } }' +
                                         'function fetchCompatiblePrintersByExt(ext) { return { ok: true, printers: [] } }' +
                                         'function fetchCompatiblePrintersByFileId(fileId) { return { ok: true, printers: [] } }' +
                                         'function fetchReasonCatalog() { return { ok: true, reasons: [] } }' +
                                         'function fetchPrinterDetails(printerId) { return { ok: true, details: {} } }' +
                                         'function fetchPrinterProjects(printerId, page, limit) { return { ok: true, projects: [] } }' +
                                         'function sendPrintOrder() { return { ok: true, taskId: "123" } }' +
                                         'function sendPrinterOrder(printerId, orderId, data, projectId) { orderCalls.push({ printerId: String(printerId), orderId: Number(orderId), data: data, projectId: String(projectId || "") }); return { ok: true, msgId: "msg-" + String(orderId) } }' +
                                         '}',
                                         this,
                                         "printerLocalFileAutoConnectBridgeMock")

        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml", {"width": 1280, "height": 800})
        page.openLocalFileDialogForRemotePrint("p1")
        compare(mqttBridge.ensureCalls, 1)
        compare(cloudBridge.orderCalls.length, 0)

        mqttBridge.connected = true
        wait(0)
        compare(cloudBridge.orderCalls.length, 2)
        compare(cloudBridge.orderCalls[0].orderId, 1231)
        compare(cloudBridge.orderCalls[1].orderId, 103)

        page.destroy()
        cloudBridge = undefined
        mqttBridge = undefined
    }

    function test_printer_tabs_title_contains_name_only() {
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

        var page = null
        try {
            page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml", {"width": 1280, "height": 800})
            var tabButton = findObjectByName(page, "printerTabButton")
            verify(tabButton !== null)
            verify(String(tabButton.text).indexOf("Printer One") !== -1)
            verify(String(tabButton.text).indexOf("Printing") === -1)

            var headerStatusChip = findObjectByName(page, "printerHeaderStatusChip")
            verify(headerStatusChip !== null)
            compare(String(headerStatusChip.status), "Printing")
        } finally {
            if (page !== null) {
                page.destroy()
                wait(0)
            }
            cloudBridge = undefined
        }
    }

    function test_main_window_print_intent_routes_from_files_to_printers() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'signal filesUpdatedFromCloud(var files, string message);' +
                                         'signal quotaUpdatedFromCloud(var quota, string message);' +
                                         'signal printersUpdatedFromCloud(var printers, string message);' +
                                         'signal printerInsightsUpdatedFromCloud(string printerId, var details, var projects, string detailsRawJson, string projectsRawJson, string message);' +
                                         'signal syncFailed(string scope, string message);' +
                                         'property int sendPrintCalls: 0;' +
                                         'property int insightRefreshCalls: 0;' +
                                         'property string lastPrintPrinterId: "";' +
                                         'property string lastPrintFileId: "";' +
                                         'function fetchQuota() { return { ok: true, totalBytes: 0, usedBytes: 0 } }' +
                                         'function fetchFiles(page, limit) { return { ok: true, files: [ { fileId: "route-file-1", fileName: "route_file.pwmb", status: "READY", sizeText: "1 MB", printTime: "02h 05m", resinUsage: "51 ml" } ] } }' +
                                         'function fetchPrinters() { return { ok: true, endpoint: "/mock/printers", rawJson: "{}", printers: [ { id: "route-p1", name: "Route Printer", model: "Mono", type: "LCD", state: "READY", reason: "free", available: 1, progress: -1, elapsedSec: -1, remainingSec: -1, currentFile: "", lastSeen: "now" } ] } }' +
                                         'function fetchCompatiblePrintersByExt(ext) { return { ok: true, printers: [ { id: "route-p1", available: 1, reason: "" } ] } }' +
                                         'function fetchCompatiblePrintersByFileId(fileId) { return { ok: true, printers: [ { id: "route-p1", available: 1, reason: "" } ] } }' +
                                         'function fetchReasonCatalog() { return { ok: true, reasons: [] } }' +
                                         'function fetchPrinterDetails(printerId) { return { ok: true, details: {} } }' +
                                         'function fetchPrinterProjects(printerId, page, limit) { return { ok: true, projects: [] } }' +
                                         'function sendPrintOrder(printerId, fileId, deleteAfterPrint, liftCompensation) { sendPrintCalls += 1; lastPrintPrinterId = String(printerId); lastPrintFileId = String(fileId); return { ok: true, taskId: "task-route-1" } }' +
                                         '}', this, "mainWindowPrintBridgeMock")
        printWorkflowBridge = createRemotePrintWorkflowMock({
            requestId: "workflow-request-1",
            fileId: "route-file-1",
            fileName: "route_file.pwmb",
            compatibilityResult: { ok: true, printers: [ { id: "route-p1", available: 1, reason: "" } ] },
            compatiblePrinters: [ { id: "route-p1", name: "Route Printer", model: "Mono", state: "READY", machineType: "" } ],
            selectedPrinterId: "route-p1",
            allowed: true,
            blockReason: "",
            blockReasonKey: "",
            bestEffortWarning: false
        })

        var window = createQmlObject("../../../ui/qml/MainWindow.qml")
        var tabs = findObjectByName(window, "controlRoomTabs")
        var cloudPage = findObjectByName(window, "cloudFilesPage")
        var printerPage = findObjectByName(window, "printerPage")

        verify(tabs !== null)
        verify(cloudPage !== null)
        verify(printerPage !== null)
        compare(tabs.currentIndex, 0)

        cloudPage.requestPrint("route-file-1", "route_file.pwmb")

        compare(tabs.currentIndex, 0)
        compare(String(printerPage.selectedCloudFileId), "route-file-1")
        var printConfigDialog = findObjectByName(printerPage, "remotePrintConfigDialog")
        verify(printConfigDialog !== null)
        compare(printConfigDialog.visible, true)
        compare(printerPage.remotePrintPreparing, true)
        compare(String(printConfigDialog.selectedPrintTime), "02h 05m")
        compare(String(printConfigDialog.selectedResinUsage), "51 ml")
        var startButton = findObjectByName(printConfigDialog, "remotePrintStartButton")
        verify(startButton !== null)
        compare(startButton.enabled, false)

        tryCompare(printerPage, "remotePrintPreparing", false, 1000)

        compare(String(printerPage.remotePrinterId), "route-p1")
        compare(printerPage.remotePrintAllowed, true)
        compare(startButton.enabled, true)

        printerPage.startRemotePrint()
        tryCompare(cloudBridge, "sendPrintCalls", 1, 1000)

        compare(cloudBridge.sendPrintCalls, 1)
        compare(String(cloudBridge.lastPrintPrinterId), "route-p1")
        compare(String(cloudBridge.lastPrintFileId), "route-file-1")
        compare(printConfigDialog.visible, false)
        compare(tabs.currentIndex, 1)
        compare(String(printerPage.selectedPrinterId), "route-p1")
        verify(printerPage.selectedPrinterLiveSnapshot !== null)
        compare(String(printerPage.selectedPrinterLiveSnapshot.state), "PRINTING")
        verify(String(printerPage.selectedPrinterLiveSnapshot.currentFile).indexOf("route_file.pwmb") !== -1)

        window.close()
        window.destroy()
        cloudBridge = undefined
    }

    function test_printer_remote_print_failure_keeps_confirmation_open() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'signal printersUpdatedFromCloud(var printers, string message);' +
                                         'signal printerInsightsUpdatedFromCloud(string printerId, var details, var projects, string detailsRawJson, string projectsRawJson, string message);' +
                                         'signal syncFailed(string scope, string message);' +
                                         'property int sendPrintCalls: 0;' +
                                         'function fetchQuota() { return { ok: true, totalBytes: 0, usedBytes: 0 } }' +
                                         'function fetchFiles(page, limit) { return { ok: true, files: [] } }' +
                                         'function fetchPrinters() { return { ok: true, endpoint: "/mock/printers", rawJson: "{}", printers: [ { id: "fail-p1", name: "Fail Printer", model: "Mono", type: "LCD", state: "READY", reason: "free", available: 1, progress: -1, elapsedSec: -1, remainingSec: -1, currentFile: "", lastSeen: "now" } ] } }' +
                                         'function fetchCompatiblePrintersByExt(ext) { return { ok: true, printers: [ { id: "fail-p1", available: 1, reason: "" } ] } }' +
                                         'function fetchCompatiblePrintersByFileId(fileId) { return { ok: true, printers: [ { id: "fail-p1", available: 1, reason: "" } ] } }' +
                                         'function fetchReasonCatalog() { return { ok: true, reasons: [] } }' +
                                         'function fetchPrinterDetails(printerId) { return { ok: true, details: {} } }' +
                                         'function fetchPrinterProjects(printerId, page, limit) { return { ok: true, projects: [] } }' +
                                         'function sendPrintOrder(printerId, fileId, deleteAfterPrint, liftCompensation) { sendPrintCalls += 1; return { ok: false, message: "backend blocked" } }' +
                                         '}', this, "remotePrintFailureBridgeMock")
        printWorkflowBridge = createRemotePrintWorkflowMock({
            requestId: "workflow-request-1",
            fileId: "fail-file-1",
            fileName: "fail_file.pwmb",
            compatibilityResult: { ok: true, printers: [ { id: "fail-p1", available: 1, reason: "" } ] },
            compatiblePrinters: [ { id: "fail-p1", name: "Fail Printer", model: "Mono", state: "READY", machineType: "" } ],
            selectedPrinterId: "fail-p1",
            allowed: true,
            blockReason: "",
            blockReasonKey: "",
            bestEffortWarning: false
        })

        var window = createQmlObject("../../../ui/qml/MainWindow.qml")
        var cloudPage = findObjectByName(window, "cloudFilesPage")
        var page = findObjectByName(window, "printerPage")
        verify(cloudPage !== null)
        verify(page !== null)

        cloudPage.requestPrint("fail-file-1", "fail_file.pwmb")
        wait(0)
        var tabs = findObjectByName(window, "controlRoomTabs")
        verify(tabs !== null)
        compare(tabs.currentIndex, 0)
        var dialog = findObjectByName(page, "remotePrintConfigDialog")
        verify(dialog !== null)
        compare(dialog.visible, true)

        tryCompare(page, "remotePrintPreparing", false, 1000)
        compare(String(page.remotePrinterId), "fail-p1")
        compare(page.remotePrintAllowed, true)

        page.startRemotePrint()
        wait(0)

        compare(cloudBridge.sendPrintCalls, 1)
        compare(dialog.visible, true)
        compare(tabs.currentIndex, 0)
        verify(String(page.statusMsg).indexOf("Print order failed") === 0)
        verify(String(page.lastJobsRefreshReason) !== "print_started")

        window.close()
        window.destroy()
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
                                         'function refreshFilesAsync(page, limit, force) { automaticRefreshCalls += 1; lastAutomaticForce = force }' +
                                         'function refreshFilesAndThumbnailsAsync(page, limit, force) { explicitRefreshCalls += 1; lastExplicitForce = force }' +
                                         'property int automaticRefreshCalls: 0;' +
                                         'property int explicitRefreshCalls: 0;' +
                                         'property bool lastAutomaticForce: false;' +
                                         'property bool lastExplicitForce: false;' +
                                         '}', this, "cloudFilesBridgeMock")

        var page = createQmlObject("../../../ui/qml/pages/CloudFilesPage.qml", {"width": 1280, "height": 800})
        wait(0)
        compare(cloudBridge.automaticRefreshCalls, 1)
        compare(cloudBridge.lastAutomaticForce, true)
        compare(cloudBridge.explicitRefreshCalls, 0)
        compare(page.filesModel.count, 1)
        compare(String(page.filesModel.get(0).fileId), "cached-1")

        cloudBridge.filesUpdatedFromCloud([
            { fileId: "cloud-1", fileName: "cloud.pwmb", status: "READY", sizeText: "2 MB" }
        ], "ok")
        wait(0)
        tryCompare(page.filesModel, "count", 1)
        compare(String(page.filesModel.get(0).fileId), "cloud-1")

        var refreshButton = findObjectByName(page, "refreshFilesButton")
        verify(refreshButton !== null)
        refreshButton.clicked()
        wait(0)
        compare(cloudBridge.automaticRefreshCalls, 1)
        compare(cloudBridge.explicitRefreshCalls, 1)
        compare(cloudBridge.lastExplicitForce, true)

        page.destroy()
    }

    function test_printer_page_only_promotes_real_active_project() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'function fetchPrinters() { return { ok: true, printers: [] } }' +
                                         'function fetchFiles() { return { ok: true, files: [] } }' +
                                         'function sendPrintOrder() { return { ok: true } }' +
                                         '}', this, "printerActiveProjectBridgeMock")
        printWorkflowBridge = createRemotePrintWorkflowMock(({}))

        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml", {
                                       "width": 1280,
                                       "height": 800,
                                       "deferStartupInitialization": true
                                   })
        var terminalProjects = [
            { taskId: "task-failed", printerId: "p1", printStatus: 3, createTime: 30 },
            { taskId: "task-finished", printerId: "p1", printStatus: 2, createTime: 20 }
        ]
        verify(page.firstActiveProject(terminalProjects) === null)

        var history = findObjectByName(page, "printerHistoryModel")
        verify(history !== null)
        history.replaceOrPatchJobs(terminalProjects)
        verify(page.activeProjectFromHistory() === null)

        page.setLiveProjectFromList(terminalProjects)
        compare(Object.keys(page.liveProjectData).length, 0)

        var activeProjects = terminalProjects.slice(0)
        activeProjects.unshift({ taskId: "task-running", printerId: "p1", printStatus: 1, createTime: 40 })
        compare(String(page.firstActiveProject(activeProjects).taskId), "task-running")
        page.setLiveProjectFromList(activeProjects)
        compare(String(page.liveProjectData.taskId), "task-running")

        page.setLiveProjectFromList(terminalProjects)
        compare(Object.keys(page.liveProjectData).length, 0)
        page.destroy()
    }

    function test_printer_cloud_file_cache_miss_uses_metadata_only_refresh() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'signal cachedFilesLoaded(var result);' +
                                         'signal filesUpdatedFromCloud(var files, string message);' +
                                         'function fetchPrinters() { return { ok: true, printers: [] } }' +
                                         'function fetchFiles() { return { ok: true, files: [] } }' +
                                         'function sendPrintOrder() { return { ok: true } }' +
                                         'function loadCachedFilesAsync(page, limit) { cacheLoadCalls += 1 }' +
                                         'function refreshFilesMetadataAsync(page, limit, force) { metadataRefreshCalls += 1; lastMetadataForce = force }' +
                                         'function refreshFilesAsync(page, limit, force) { thumbnailRefreshCalls += 1 }' +
                                         'property int cacheLoadCalls: 0;' +
                                         'property int metadataRefreshCalls: 0;' +
                                         'property int thumbnailRefreshCalls: 0;' +
                                         'property bool lastMetadataForce: false;' +
                                         '}', this, "printerFilesMetadataBridgeMock")

        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml", {
                                       "width": 1280,
                                       "height": 800,
                                       "deferStartupInitialization": true
                                   })
        page.loadCloudFilesForRemotePrint("printer-1")
        compare(cloudBridge.cacheLoadCalls, 1)
        cloudBridge.cachedFilesLoaded({ ok: true, files: [] })
        wait(0)
        compare(cloudBridge.metadataRefreshCalls, 1)
        compare(cloudBridge.lastMetadataForce, true)
        compare(cloudBridge.thumbnailRefreshCalls, 0)

        page.destroy()
        cloudBridge = undefined
    }

    function test_printer_page_defers_hidden_mqtt_cache_refresh() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'property int cachedAsyncCalls: 0;' +
                                         'function fetchPrinters() { return { ok: true, printers: [] }; }' +
                                         'function fetchFiles() { return { ok: true, files: [] }; }' +
                                         'function sendPrintOrder() { return { ok: true }; }' +
                                         'function loadCachedPrintersAsync() { cachedAsyncCalls += 1; }' +
                                         '}', this)
        mqttBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'property bool connected: true;' +
                                         'signal realtimeEventTickChanged();' +
                                         'signal printerFileListReceived(string printerId, string source, var records, string state, int code, string message);' +
                                         '}', this)

        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml", {
                                       "width": 1280,
                                       "height": 800,
                                       "deferStartupInitialization": true,
                                       "pageActive": false,
                                       "mqttRealtimeDebounceMs": 10
                                   })
        mqttBridge.realtimeEventTickChanged()
        wait(30)
        compare(cloudBridge.cachedAsyncCalls, 0)
        compare(page.mqttRealtimeRefreshPending, true)

        page.pageActive = true
        tryVerify(function() { return cloudBridge.cachedAsyncCalls === 1 }, 250)
        compare(page.mqttRealtimeRefreshPending, false)
        page.destroy()
    }

    function test_printers_cache_flow_forces_refresh_and_applies_cloud_signal() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'signal printersUpdatedFromCloud(var printers, string message);' +
                                         'signal printerInsightsUpdatedFromCloud(string printerId, var details, var projects, string detailsRawJson, string projectsRawJson, string message);' +
                                         'signal syncFailed(string scope, string message);' +
                                         'function fetchPrinters() { return { ok: true, printers: [] } }' +
                                         'function fetchFiles() { return { ok: true, files: [] } }' +
                                         'function sendPrintOrder() { sendPrintCalls += 1; return { ok: true, taskId: "t1" } }' +
                                         'function fetchCompatiblePrintersByExt() { return { ok: true, printers: [{ id: "cached-p1", available: 1, reason: "" }] } }' +
                                         'function fetchCompatiblePrintersByFileId() { return { ok: true, printers: [{ id: "cached-p1", available: 1, reason: "" }] } }' +
                                         'function fetchReasonCatalog() { return { ok: true, reasons: [] } }' +
                                         'function fetchPrinterDetails() { return { ok: true, details: {} } }' +
                                         'function fetchPrinterProjects() { return { ok: true, projects: [] } }' +
                                         'function loadCachedPrinterProjects(printerId) {' +
                                         '  if (String(printerId) !== "cached-p1") return { ok: true, projects: [] };' +
                                         '  return { ok: true, projects: [ { taskId: "t-cache-1", gcodeName: "cached.pwmb", printerId: "cached-p1", printerName: "Cached Printer", printStatus: 1, progress: 20, reason: "", createTime: 1, endTime: 0, img: "" } ] };' +
                                         '}' +
                                         'function loadCachedPrinters() {' +
                                         '  return { ok: true, endpoint: "/mock/printers", rawJson: "{}", printers: [' +
                                         '    { id: "cached-p1", name: "Cached Printer", model: "Mono", type: "LCD", state: "READY", reason: "free", available: 1, progress: -1, elapsedSec: -1, remainingSec: -1, currentFile: "", lastSeen: "now", details: { firmwareVersion: "FW-DB-1", printTotalTime: "10h30m" }, projects: [ { taskId: "t-cache-1", gcodeName: "cached.pwmb", printerId: "cached-p1", printerName: "Cached Printer", printStatus: 1, progress: 20, reason: "", createTime: 1, endTime: 0, img: "" } ] }' +
                                         '  ] }' +
                                         '}' +
                                         'function refreshPrintersAsync(force) { refreshCalls += 1; lastForce = force }' +
                                         'function refreshPrinterInsightsAsync(printerId, page, limit, force) { insightRefreshCalls += 1; lastInsightPrinterId = String(printerId); lastInsightForce = force === true }' +
                                         'property int refreshCalls: 0;' +
                                         'property int insightRefreshCalls: 0;' +
                                         'property int sendPrintCalls: 0;' +
                                         'property string lastInsightPrinterId: "";' +
                                         'property bool lastInsightForce: false;' +
                                         'property bool lastForce: false;' +
                                         '}', this, "printerBridgeMock")
        printWorkflowBridge = createRemotePrintWorkflowMock({
            requestId: "workflow-request-cache",
            fileId: "file-1",
            fileName: "file.pwmb",
            compatiblePrinters: [ { id: "cached-p1", name: "Cached Printer", model: "Mono", state: "READY", machineType: "" } ],
            selectedPrinterId: "cached-p1",
            allowed: true,
            blockReason: "",
            blockReasonKey: "",
            bestEffortWarning: false
        })

        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml", {"width": 1280, "height": 800, "autoRefreshIntervalMs": 20})
        compare(cloudBridge.refreshCalls, 1)
        compare(cloudBridge.lastForce, true)
        compare(cloudBridge.insightRefreshCalls, 1)
        compare(cloudBridge.lastInsightPrinterId, "cached-p1")
        compare(cloudBridge.lastInsightForce, true)
        compare(String(page.lastJobsRefreshReason), "startup")
        compare(String(page.selectedPrinterId), "cached-p1")
        compare(String(page.selectedPrinterDetails.firmwareVersion), "FW-DB-1")
        compare(String(page.selectedPrinterDetails.printTotalTime), "10h30m")

        var history = findObjectByName(page, "printerHistoryModel")
        verify(history !== null)
        tryCompare(history, "count", 1)
        compare(String(history.get(0).taskId), "t-cache-1")

        cloudBridge.printerInsightsUpdatedFromCloud("cached-p1", { firmwareVersion: "FW-CLOUD-2", printTotalTime: "12h30m", materialUsed: "250 ml" }, [
            { taskId: "t-cloud-2", gcodeName: "cloud.pwmb", printerId: "cached-p1", printerName: "Cached Printer", printStatus: 2, progress: 100, reason: "", createTime: 2, endTime: 3, img: "" }
        ], "", "", "ok")
        wait(0)
        compare(String(page.selectedPrinterDetails.firmwareVersion), "FW-CLOUD-2")
        compare(String(page.selectedPrinterDetails.printTotalTime), "12h30m")
        compare(String(page.selectedPrinterDetails.materialUsed), "250 ml")
        compare(history.count, 2)
        compare(String(history.get(0).taskId), "t-cloud-2")
        compare(String(history.get(1).taskId), "t-cache-1")

        cloudBridge.printerInsightsUpdatedFromCloud("cached-p1", {}, [
            { taskId: "t-cache-1", gcodeName: "cached-updated.pwmb", printerId: "cached-p1", printerName: "Cached Printer", printStatus: 1, progress: 77, reason: "", createTime: 1, endTime: 0, img: "" }
        ], "", "", "ok")
        wait(0)
        compare(history.count, 2)
        compare(String(history.get(1).taskId), "t-cache-1")
        compare(String(history.get(1).gcodeName), "cached-updated.pwmb")
        compare(Number(history.get(1).progress), 77)

        var insightCallsBeforePrinterRefresh = cloudBridge.insightRefreshCalls
        cloudBridge.printersUpdatedFromCloud([
            { id: "cached-p1", name: "Cached Printer", model: "Mono", type: "LCD", state: "PRINTING", reason: "printing", available: 1, progress: 30, elapsedSec: 300, remainingSec: 600, currentFile: "a.pwmb", lastSeen: "now", details: { mqttResinBlocking: false, printTotalTime: "-" }, projects: [] }
        ], "ok")
        wait(0)
        tryCompare(page, "selectedPrinterId", "cached-p1")
        compare(String(page.selectedPrinterDetails.firmwareVersion), "FW-CLOUD-2")
        compare(String(page.selectedPrinterDetails.printTotalTime), "12h30m")
        compare(String(page.selectedPrinterDetails.materialUsed), "250 ml")
        compare(cloudBridge.insightRefreshCalls, insightCallsBeforePrinterRefresh)

        cloudBridge.printersUpdatedFromCloud([
            { id: "cached-p1", name: "Cached Printer", model: "Mono", type: "LCD", state: "READY", reason: "free", available: 1, progress: -1, elapsedSec: -1, remainingSec: -1, currentFile: "", lastSeen: "now", details: { firmwareVersion: "FW-CLOUD-2" }, projects: [] }
        ], "ok")
        wait(0)
        compare(cloudBridge.insightRefreshCalls, insightCallsBeforePrinterRefresh + 1)
        compare(String(page.lastJobsRefreshReason), "print_finished")
        compare(cloudBridge.lastInsightForce, true)

        cloudBridge.printersUpdatedFromCloud([
            { id: "cached-p1", name: "Cached Printer", model: "Mono", type: "LCD", state: "READY", reason: "free", available: 1, progress: -1, elapsedSec: -1, remainingSec: -1, currentFile: "", lastSeen: "now", details: { firmwareVersion: "FW-CLOUD-2" }, projects: [] }
        ], "ok")
        wait(0)
        compare(cloudBridge.insightRefreshCalls, insightCallsBeforePrinterRefresh + 1)

        var insightCallsBeforePrintStart = cloudBridge.insightRefreshCalls
        page.openRemotePrintFromFile("file-1", "file.pwmb")
        wait(0)
        page.startRemotePrint()
        wait(0)
        compare(cloudBridge.sendPrintCalls, 1)
        compare(cloudBridge.insightRefreshCalls, insightCallsBeforePrintStart + 1)
        compare(String(page.lastJobsRefreshReason), "print_started")
        compare(cloudBridge.lastInsightForce, true)

        var timer = findObjectByName(page, "printersAutoRefreshTimer")
        verify(timer !== null)
        compare(timer.running, true)
        wait(65)
        verify(cloudBridge.refreshCalls >= 2)

        page.pageActive = false
        compare(timer.running, false)
        var refreshCallsWhileHidden = cloudBridge.refreshCalls
        wait(65)
        compare(cloudBridge.refreshCalls, refreshCallsWhileHidden)
        page.pageActive = true
        compare(timer.running, true)

        page.destroy()
    }

    function test_direct_print_failure_cleanup_preference_defaults_off_and_persists() {
        uiSettingsBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                               'property var values: ({});' +
                                               'property int getCalls: 0;' +
                                               'property int directCleanupGetCalls: 0;' +
                                               'property string directCleanupFallback: "";' +
                                               'function getString(key, fallback) {' +
                                               '  getCalls += 1;' +
                                               '  if (String(key) === "printing.directDeleteLocalOnFailure") {' +
                                               '    directCleanupGetCalls += 1;' +
                                               '    directCleanupFallback = String(fallback);' +
                                               '  }' +
                                               '  return values[key] !== undefined ? values[key] : fallback' +
                                               '}' +
                                               'function setString(key, value) { values[key] = value }' +
                                               'function sync() {}' +
                                               '}', this, "directPrintCleanupSettingsMock")
        var window = createQmlObject("../../../ui/qml/MainWindow.qml")
        tryVerify(function() { return uiSettingsBridge.directCleanupGetCalls > 0 }, 1000)
        verify(uiSettingsBridge.directCleanupGetCalls >= 1)
        compare(String(uiSettingsBridge.directCleanupFallback), "false")
        compare(window.directDeleteLocalOnFailureEnabled, false)

        var menuItem = findObjectByName(window, "menuSettingsDirectDeleteLocalOnFailure")
        verify(menuItem !== null)
        compare(menuItem.checked, false)

        window.persistDirectPrintFailureCleanup(true)
        compare(window.directDeleteLocalOnFailureEnabled, true)
        compare(String(uiSettingsBridge.values["printing.directDeleteLocalOnFailure"]), "true")
        compare(menuItem.checked, true)
        window.close()
        window.destroy()

        var restoredWindow = createQmlObject("../../../ui/qml/MainWindow.qml")
        tryVerify(function() { return uiSettingsBridge.directCleanupGetCalls >= 2 }, 1000)
        tryCompare(restoredWindow, "directDeleteLocalOnFailureEnabled", true, 1000)
        restoredWindow.close()
        restoredWindow.destroy()
    }

    function test_failed_direct_print_cleanup_uses_semantic_workflow_notices() {
        cloudBridge = Qt.createQmlObject('import QtQuick 2.15; QtObject {' +
                                         'function fetchPrinters() { return { ok: true, printers: [] } }' +
                                         'function fetchFiles() { return { ok: true, files: [] } }' +
                                         'function sendPrintOrder() { return { ok: true } }' +
                                         '}', this, "directFailureCleanupBridgeMock")
        printWorkflowBridge = createRemotePrintWorkflowMock(({}))

        var page = createQmlObject("../../../ui/qml/pages/PrinterPage.qml",
                                   {"width": 1280, "height": 800,
                                    "deferStartupInitialization": true})

        page.pendingRemotePrintByPrinterId = {
            "p1": { "printerId": "p1", "printStatus": 1, "currentFile": "cube.pwsz" }
        }
        printWorkflowBridge.directPrintTrackingReleased("p1")
        wait(0)
        verify(page.pendingRemotePrintByPrinterId["p1"] === undefined)

        printWorkflowBridge.directCleanupNotice(4)
        wait(0)
        verify(String(page.statusMsg).indexOf("Direct print failed") === 0)
        compare(String(page.statusSev), "warn")

        printWorkflowBridge.directCleanupNotice(2)
        wait(0)
        verify(String(page.statusMsg).indexOf("Direct print failed") === 0)
        verify(String(page.statusMsg).indexOf("cloud file was kept") > 0)

        printWorkflowBridge.directCleanupNotice(5)
        wait(0)
        verify(String(page.statusMsg).indexOf("Direct print finished") === 0)
        compare(String(page.statusSev), "success")

        printWorkflowBridge.directCleanupNotice(6)
        wait(0)
        verify(String(page.statusMsg).indexOf("cloud deletion failed") > 0)
        compare(String(page.statusSev), "warn")

        page.destroy()
    }

}
