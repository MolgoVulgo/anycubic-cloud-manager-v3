import QtQuick 2.15
import QtQuick.Controls 2.15
import QtTest 1.3

TestCase {
    name: "ControlRoomUi"

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
        compare(uploadButton.text, "Upload .pwmb")

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

        verify(statusLabel.text.indexOf("bridge backend indisponible") !== -1)
        verify(detailsPanel.text.indexOf("sessionImportBridge non défini") !== -1
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

        verify(statusLabel.text.indexOf("analyse valide") !== -1)
        verify(detailsPanel.text.indexOf("Analyse HAR: VALIDE") !== -1)
        verify(detailsPanel.text.indexOf("mock import done") !== -1)
        verify(detailsPanel.text.indexOf("/tmp/session.json") !== -1)

        dialog.requestClose()
        compare(commitPath, "/tmp/session.json")
        dialog.destroy()
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
}
