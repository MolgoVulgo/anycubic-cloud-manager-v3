import QtQuick 2.15
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

    function findObjectByName(root, name) {
        if (root === null || root === undefined) {
            return null
        }
        if (root.objectName === name) {
            return root
        }

        var direct = [root.contentItem, root.background, root.header, root.footer, root.popupItem]
        for (var d = 0; d < direct.length; ++d) {
            var directNode = direct[d]
            if (directNode !== null && directNode !== undefined) {
                var directFound = findObjectByName(directNode, name)
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
                var found = findObjectByName(kids[i], name)
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

        var sessionButton = findObjectByName(window, "sessionSettingsButton")
        var uploadButton = findObjectByName(window, "uploadDialogButton")
        verify(sessionButton !== null)
        verify(uploadButton !== null)

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
        var targetField = findObjectByName(dialog, "sessionTargetField")
        verify(targetField !== null)
        compare(targetField.text, "~/.config/accloud/session.json")
        dialog.destroy()
    }

    function test_session_dialog_import_button_updates_status_without_bridge() {
        var dialog = createQmlObject("../../../ui/qml/dialogs/SessionSettingsDialog.qml")

        var harField = findObjectByName(dialog, "harFileField")
        var importButton = findObjectByName(dialog, "harImportButton")
        var statusLabel = findObjectByName(dialog, "harImportStatusLabel")
        var detailsPanel = findObjectByName(dialog, "harImportResultPanel")

        verify(harField !== null)
        verify(importButton !== null)
        verify(statusLabel !== null)
        verify(detailsPanel !== null)

        harField.text = "/tmp/session.har"
        verify(importButton.enabled)
        importButton.clicked()

        verify(statusLabel.text.indexOf("bridge backend indisponible") !== -1)
        verify(detailsPanel.text.indexOf("sessionImportBridge non défini") !== -1)
        dialog.destroy()
    }

    function test_session_dialog_import_button_calls_bridge_and_updates_result() {
        var dialog = createQmlObject("../../../ui/qml/dialogs/SessionSettingsDialog.qml")
        dialog.importBridge = {
            importHar: function(harPath, sessionPath) {
                return {
                    ok: true,
                    message: "mock import done",
                    entriesVisited: 4,
                    entriesAccepted: 2,
                    tokenKeys: ["Authorization", "access_token"]
                }
            }
        }

        var harField = findObjectByName(dialog, "harFileField")
        var importButton = findObjectByName(dialog, "harImportButton")
        var statusLabel = findObjectByName(dialog, "harImportStatusLabel")
        var detailsPanel = findObjectByName(dialog, "harImportResultPanel")

        verify(harField !== null)
        verify(importButton !== null)
        verify(statusLabel !== null)
        verify(detailsPanel !== null)

        harField.text = "/tmp/sample.har"
        importButton.clicked()

        verify(statusLabel.text.indexOf("import réussi") !== -1)
        verify(detailsPanel.text.indexOf("Import: OK") !== -1)
        verify(detailsPanel.text.indexOf("mock import done") !== -1)
        verify(detailsPanel.text.indexOf("2 acceptées / 4 visitées") !== -1)
        dialog.destroy()
    }
}
