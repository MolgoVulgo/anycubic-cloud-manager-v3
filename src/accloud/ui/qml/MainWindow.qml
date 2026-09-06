import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "components/Theme.js" as Theme
import "components"
import "pages" as Pages
import "dialogs" as Dialogs

ApplicationWindow {
    id: root
    objectName: "controlRoomWindow"
    width: 1480
    height: 920
    visible: true
    title: qsTr("Anycubic Cloud Control Room")
    property string statusText: qsTr("Checking active session...")
    property string globalStatusMsg: qsTr("Ready.")
    property string globalStatusSev: "info"
    property string globalStatusOpId: "op_shell_status"
    property bool buildDebugEnabled: (typeof accloudBuildDebugEnabled !== "undefined")
                                     && accloudBuildDebugEnabled === true
    property bool prodUi: (typeof accloudProdUi !== "undefined")
                          && accloudProdUi === true
    property bool viewer3dEnabled:
        (typeof accloudViewer3dEnabled === "undefined")
        || accloudViewer3dEnabled === true
    property bool debugUi: buildDebugEnabled
                               && Qt.application.arguments
                               && Qt.application.arguments.indexOf("--debug-ui") !== -1
    property string sessionTargetPath: ""
    property string sessionDetailsText: qsTr("No session check executed yet.")
    property string persistedThemeName: "WarmLight"
    property string persistedAccentName: "Teal"
    property string persistedLanguageCode: "system"
    property bool pwszPreviewCompletionEnabled: true
    property bool pwszPreviewConfirmationEnabled: true
    property bool cloudFileAdvancedDetailsEnabled: false
    property bool directDeleteLocalOnFailureEnabled: false
    property int render3dWorkerCount: 4
    property string render3dPalettePreset: "technical_cyan"
    readonly property color render3dPartColor: root.render3dPaletteFor(root.render3dPalettePreset).partColor
    readonly property color render3dBackgroundColor: root.render3dPaletteFor(root.render3dPalettePreset).backgroundColor
    readonly property string directDeleteLocalOnFailureSettingsKey: "printing.directDeleteLocalOnFailure"
    readonly property string cloudFileAdvancedDetailsSettingsKey: "ui.cloudFiles.showAdvancedDetails"
    readonly property string render3dWorkerCountSettingsKey: "render3d.workerCount"
    readonly property string render3dPalettePresetSettingsKey: "render3d.palettePreset"

    function hasUiSettingsBridge() {
        return (typeof uiSettingsBridge !== "undefined")
                && uiSettingsBridge !== null
                && typeof uiSettingsBridge.getString === "function"
                && typeof uiSettingsBridge.setString === "function"
    }

    function hasI18nBridge() {
        return (typeof appI18nBridge !== "undefined")
                && appI18nBridge !== null
                && typeof appI18nBridge.setLanguage === "function"
                && appI18nBridge.languageCode !== undefined
                && appI18nBridge.effectiveLanguageCode !== undefined
    }

    function translateLocalizedText(rawText) {
        var text = String(rawText || "")
        if (text.length === 0)
            return text

        var replacements = {
            "请求被接受": qsTr("Request accepted"),
            "操作成功": qsTr("Operation successful"),
            "连接成功": qsTr("Connection successful"),
            "用户不存在": qsTr("User does not exist"),
            "设备离线": qsTr("Printer offline"),
            "打印中": qsTr("Printing in progress"),
            "失败": qsTr("Failed"),
            "成功": qsTr("Success"),
            "错误": qsTr("Error"),
            "超时": qsTr("Timeout")
        }

        for (var key in replacements) {
            if (Object.prototype.hasOwnProperty.call(replacements, key))
                text = text.split(key).join(replacements[key])
        }

        if (/[\u4e00-\u9fff]/.test(text))
            text = text.replace(/[\u4e00-\u9fff]+/g, qsTr("localized backend message"))

        return text
    }

    function backendStatusText(rawMessage, fallbackMessage) {
        var text = translateLocalizedText(String(rawMessage || "").trim())
        return text.length > 0 ? text : String(fallbackMessage || qsTr("Operation status unavailable."))
    }

    function pushGlobalStatus(message, severity, operationId) {
        var msg = String(message || "").trim()
        if (msg.length === 0)
            return
        globalStatusMsg = translateLocalizedText(msg)
        globalStatusSev = String(severity || "info")
        globalStatusOpId = String(operationId || "op_shell_status")
    }

    function applyThemeSelection(themeNameValue, accentNameValue, persist) {
        var candidateTheme = String(themeNameValue || "").trim()
        if (candidateTheme.length === 0 || !Theme.setThemePreset(candidateTheme)) {
            candidateTheme = "WarmLight"
            Theme.setThemePreset(candidateTheme)
        }

        var candidateAccent = String(accentNameValue || "").trim()
        if (candidateAccent.length === 0 || !Theme.setAccent(candidateAccent)) {
            candidateAccent = "Teal"
            Theme.setAccent(candidateAccent)
        }

        if (persist === true) {
            root.persistedThemeName = Theme.themeName
            root.persistedAccentName = Theme.accentName
            if (root.hasUiSettingsBridge()) {
                uiSettingsBridge.setString("ui.themeName", root.persistedThemeName)
                uiSettingsBridge.setString("ui.accentName", root.persistedAccentName)
                if (typeof uiSettingsBridge.sync === "function")
                    uiSettingsBridge.sync()
            }
        }

    }

    function restorePersistedTheme() {
        root.applyThemeSelection(root.persistedThemeName, root.persistedAccentName, false)
    }

    function loadThemeFromSettings() {
        var themeValue = "WarmLight"
        var accentValue = "Teal"

        if (root.hasUiSettingsBridge()) {
            themeValue = uiSettingsBridge.getString("ui.themeName", themeValue)
            accentValue = uiSettingsBridge.getString("ui.accentName", accentValue)
        }

        root.applyThemeSelection(themeValue, accentValue, false)
        root.persistedThemeName = Theme.themeName
        root.persistedAccentName = Theme.accentName

        if (root.hasUiSettingsBridge()) {
            // Normalize persisted values if they were invalid.
            uiSettingsBridge.setString("ui.themeName", root.persistedThemeName)
            uiSettingsBridge.setString("ui.accentName", root.persistedAccentName)
            if (typeof uiSettingsBridge.sync === "function")
                uiSettingsBridge.sync()
        }
    }

    function loadLanguageFromSettings() {
        if (!root.hasI18nBridge())
            return
        root.persistedLanguageCode = String(appI18nBridge.languageCode || "system")
    }

    function normalizeRender3dWorkerCount(value) {
        var parsed = Number(value)
        if (!isFinite(parsed))
            parsed = 4
        return Math.max(1, Math.min(16, Math.round(parsed)))
    }

    function render3dPaletteOptions() {
        return [
            {
                "id": "technical_cyan",
                "label": qsTr("Technical cyan"),
                "partColor": "#55B7C6",
                "backgroundColor": "#171A1F"
            },
            {
                "id": "industrial_amber",
                "label": qsTr("Industrial amber"),
                "partColor": "#F2B84B",
                "backgroundColor": "#20242B"
            },
            {
                "id": "mineral_ivory",
                "label": qsTr("Mineral ivory"),
                "partColor": "#E8E2D6",
                "backgroundColor": "#243447"
            },
            {
                "id": "night_coral",
                "label": qsTr("Night coral"),
                "partColor": "#FF7A66",
                "backgroundColor": "#111827"
            },
            {
                "id": "light_graphite",
                "label": qsTr("Light graphite"),
                "partColor": "#334155",
                "backgroundColor": "#F4F1EA"
            }
        ]
    }

    function normalizeRender3dPalettePreset(value) {
        var requested = String(value || "").trim()
        var options = root.render3dPaletteOptions()
        for (var i = 0; i < options.length; ++i) {
            if (String(options[i].id) === requested)
                return requested
        }
        return "technical_cyan"
    }

    function render3dPaletteFor(value) {
        var normalized = root.normalizeRender3dPalettePreset(value)
        var options = root.render3dPaletteOptions()
        for (var i = 0; i < options.length; ++i) {
            if (String(options[i].id) === normalized)
                return options[i]
        }
        return options[0]
    }

    function render3dPaletteIndex(value) {
        var normalized = root.normalizeRender3dPalettePreset(value)
        var options = root.render3dPaletteOptions()
        for (var i = 0; i < options.length; ++i) {
            if (String(options[i].id) === normalized)
                return i
        }
        return 0
    }

    function loadRender3dSettings() {
        var configured = 4
        var palettePreset = "technical_cyan"
        if (root.hasUiSettingsBridge()) {
            configured = root.normalizeRender3dWorkerCount(uiSettingsBridge.getString(
                    root.render3dWorkerCountSettingsKey, "4"))
            palettePreset = root.normalizeRender3dPalettePreset(uiSettingsBridge.getString(
                    root.render3dPalettePresetSettingsKey, "technical_cyan"))
            uiSettingsBridge.setString(root.render3dWorkerCountSettingsKey, String(configured))
            uiSettingsBridge.setString(root.render3dPalettePresetSettingsKey, palettePreset)
            if (typeof uiSettingsBridge.sync === "function")
                uiSettingsBridge.sync()
        }
        root.render3dWorkerCount = configured
        root.render3dPalettePreset = palettePreset
    }

    function persistRender3dWorkerCount(value) {
        root.render3dWorkerCount = root.normalizeRender3dWorkerCount(value)
        if (!root.hasUiSettingsBridge())
            return
        uiSettingsBridge.setString(root.render3dWorkerCountSettingsKey,
                                   String(root.render3dWorkerCount))
        if (typeof uiSettingsBridge.sync === "function")
            uiSettingsBridge.sync()
    }

    function persistRender3dPalettePreset(value) {
        root.render3dPalettePreset = root.normalizeRender3dPalettePreset(value)
        if (!root.hasUiSettingsBridge())
            return
        uiSettingsBridge.setString(root.render3dPalettePresetSettingsKey,
                                   root.render3dPalettePreset)
        if (typeof uiSettingsBridge.sync === "function")
            uiSettingsBridge.sync()
    }

    function loadPwszUploadSettings() {
        if (!root.hasUiSettingsBridge())
            return
        root.pwszPreviewCompletionEnabled = String(uiSettingsBridge.getString(
                "cloud.upload.completePwszPreview2", "true")).toLowerCase() !== "false"
        root.pwszPreviewConfirmationEnabled = String(uiSettingsBridge.getString(
                "cloud.upload.confirmPwszPreview2", "true")).toLowerCase() !== "false"
    }

    function persistPwszUploadSetting(key, value) {
        if (!root.hasUiSettingsBridge())
            return
        uiSettingsBridge.setString(key, value === true ? "true" : "false")
        if (typeof uiSettingsBridge.sync === "function")
            uiSettingsBridge.sync()
    }

    function loadDirectPrintSettings() {
        if (!root.hasUiSettingsBridge()) {
            root.directDeleteLocalOnFailureEnabled = false
            return
        }
        root.directDeleteLocalOnFailureEnabled = String(uiSettingsBridge.getString(
                root.directDeleteLocalOnFailureSettingsKey, "false")).toLowerCase() === "true"
    }

    function persistDirectPrintFailureCleanup(value) {
        root.directDeleteLocalOnFailureEnabled = value === true
        if (!root.hasUiSettingsBridge())
            return
        uiSettingsBridge.setString(root.directDeleteLocalOnFailureSettingsKey,
                                   root.directDeleteLocalOnFailureEnabled ? "true" : "false")
        if (typeof uiSettingsBridge.sync === "function")
            uiSettingsBridge.sync()
    }

    function loadCloudFileDetailsSettings() {
        var defaultValue = root.debugUi ? "true" : "false"
        if (!root.hasUiSettingsBridge()) {
            root.cloudFileAdvancedDetailsEnabled = root.debugUi
            return
        }
        root.cloudFileAdvancedDetailsEnabled = String(uiSettingsBridge.getString(
                root.cloudFileAdvancedDetailsSettingsKey,
                defaultValue)).toLowerCase() === "true"
    }

    function persistCloudFileDetailsSetting(value) {
        root.cloudFileAdvancedDetailsEnabled = value === true
        if (!root.hasUiSettingsBridge())
            return
        uiSettingsBridge.setString(root.cloudFileAdvancedDetailsSettingsKey,
                                   root.cloudFileAdvancedDetailsEnabled ? "true" : "false")
        if (typeof uiSettingsBridge.sync === "function")
            uiSettingsBridge.sync()
    }

    function openUploadDialog() {
        uploadDialog.open()
    }

    function openPrintDialog() {
        printDialog.open()
    }

    function showSessionDetails() {
        if (typeof sessionImportBridge === "undefined"
                || sessionImportBridge === null
                || typeof sessionImportBridge.sessionDetails !== "function") {
            sessionDetailsText = qsTr("Backend session unavailable.")
            statusText = qsTr("Session details unavailable.")
            sessionDetailsDialog.open()
            return
        }
        var details = sessionImportBridge.sessionDetails(root.sessionTargetPath)
        sessionDetailsText = String(details.details)
        statusText = backendStatusText(details.message, qsTr("Session details loaded."))
        sessionDetailsDialog.open()
    }

    function applyStartupCheckResult(check) {
        if (check.sessionExists === true && check.connectionOk === true) {
            root.statusText = qsTr("Active session. Auto-refresh every 30s.")
            if (typeof mqttBridge !== "undefined"
                    && mqttBridge !== null
                    && mqttBridge.connected !== true
                    && typeof mqttBridge.ensureAutoConnected === "function") {
                mqttBridge.ensureAutoConnected()
            }
        } else {
            var startupText = backendStatusText(check.message, qsTr("Session validation required."))
            root.statusText = startupText
            sessionDialog.startupMessage = startupText
            sessionDialog.mandatoryMode = true
            sessionDialog.open()
        }
    }

    Component.onCompleted: {
        root.loadThemeFromSettings()
        root.loadLanguageFromSettings()
        root.loadPwszUploadSettings()
        root.loadRender3dSettings()
        root.loadDirectPrintSettings()
        root.loadCloudFileDetailsSettings()
        Qt.callLater(function() {
            if (typeof sessionImportBridge === "undefined"
                    || sessionImportBridge === null
                    || typeof sessionImportBridge.checkStartup !== "function") {
                root.statusText = qsTr("UI mode: backend unavailable.")
                return
            }
            if (typeof sessionImportBridge.defaultSessionPath === "function") {
                root.sessionTargetPath = String(sessionImportBridge.defaultSessionPath())
            }

            if (typeof sessionImportBridge.checkStartupAsync === "function") {
                root.statusText = qsTr("Checking active session...")
                sessionImportBridge.checkStartupAsync()
                return
            }
            root.applyStartupCheckResult(sessionImportBridge.checkStartup())
        })
    }


    Connections {
        target: (typeof sessionImportBridge !== "undefined"
                 && sessionImportBridge !== null) ? sessionImportBridge : null
        ignoreUnknownSignals: true

        function onStartupCheckFinished(result) {
            root.applyStartupCheckResult(result)
        }
    }

    component HeaderActionButton: AppButton {
        variant: "secondary"
        compact: true
        font.pixelSize: Theme.fontBodyPx
    }

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.rootGradientStart }
            GradientStop { position: 1.0; color: Theme.rootGradientEnd }
        }
    }

    menuBar: MenuBar {
        objectName: "mainMenuBar"

        Menu {
            objectName: "menuParametre"
            title: qsTr("Settings")

            Menu {
                objectName: "menuSettingsInterface"
                title: qsTr("Interface")

                MenuItem {
                    objectName: "menuSettingsTheme"
                    text: qsTr("Theme")
                    onTriggered: {
                        root.statusText = qsTr("Opening theme settings panel.")
                        themeDialog.open()
                    }
                }

                MenuItem {
                    objectName: "menuSettingsLanguage"
                    text: qsTr("Language")
                    onTriggered: {
                        root.statusText = qsTr("Opening language settings panel.")
                        languageDialog.open()
                    }
                }

                MenuSeparator {}

                MenuItem {
                    objectName: "menuSettingsCloudFileAdvancedDetails"
                    text: qsTr("Show technical file details")
                    checkable: true
                    checked: root.cloudFileAdvancedDetailsEnabled
                    onTriggered: {
                        root.persistCloudFileDetailsSetting(checked)
                        root.statusText = checked
                                ? qsTr("Technical file details enabled.")
                                : qsTr("Technical file details hidden.")
                    }
                }
            }

            Menu {
                objectName: "menuSettingsPrinting"
                title: qsTr("Printing")

                MenuItem {
                    objectName: "menuSettingsDirectDeleteLocalOnFailure"
                    text: qsTr("Delete printer-local copy when a direct print fails")
                    checkable: true
                    checked: root.directDeleteLocalOnFailureEnabled
                    onTriggered: {
                        root.persistDirectPrintFailureCleanup(checked)
                        root.statusText = checked
                                ? qsTr("Failed direct prints will remove the printer-local copy when cleanup was requested.")
                                : qsTr("Failed direct prints will keep the printer-local copy.")
                    }
                }
            }

            Menu {
                objectName: "menuSettingsFilesUpload"
                title: qsTr("Files / Upload")

                MenuItem {
                    objectName: "menuSettingsPwszPreviewCompletion"
                    text: qsTr("Complete PWSZ previews before upload")
                    checkable: true
                    checked: root.pwszPreviewCompletionEnabled
                    onTriggered: {
                        root.pwszPreviewCompletionEnabled = checked
                        root.persistPwszUploadSetting("cloud.upload.completePwszPreview2", checked)
                        root.statusText = checked
                                ? qsTr("Automatic PWSZ preview completion enabled.")
                                : qsTr("Automatic PWSZ preview completion disabled.")
                    }
                }

                MenuItem {
                    objectName: "menuSettingsPwszPreviewConfirmation"
                    text: qsTr("Confirm before modifying PWSZ files")
                    checkable: true
                    checked: root.pwszPreviewConfirmationEnabled
                    onTriggered: {
                        root.pwszPreviewConfirmationEnabled = checked
                        root.persistPwszUploadSetting("cloud.upload.confirmPwszPreview2", checked)
                    }
                }
            }

            Menu {
                objectName: "menuSettingsViewer3d"
                title: qsTr("3D Viewer")

                MenuItem {
                    objectName: "menuSettingsRender3dWorkers"
                    text: qsTr("3D generation workers: %1").arg(root.render3dWorkerCount)
                    enabled: root.viewer3dEnabled
                    onTriggered: {
                        render3dWorkersSpin.value = root.render3dWorkerCount
                        render3dWorkersDialog.open()
                    }
                }

                MenuItem {
                    objectName: "menuSettingsRender3dPalette"
                    text: qsTr("3D colors: %1").arg(
                              root.render3dPaletteFor(root.render3dPalettePreset).label)
                    enabled: root.viewer3dEnabled
                    onTriggered: {
                        render3dPaletteDialog.prepare()
                        render3dPaletteDialog.open()
                    }
                }
            }
        }

        Menu {
            objectName: "menuSession"
            title: qsTr("Session")

            MenuItem {
                objectName: "menuSessionDetails"
                text: qsTr("Details")
                onTriggered: root.showSessionDetails()
            }

            MenuItem {
                objectName: "menuSessionImportHar"
                text: qsTr("Import HAR...")
                onTriggered: sessionDialog.open()
            }

            MenuSeparator {}

            MenuItem {
                objectName: "menuSessionLocation"
                text: qsTr("Session location...")
                onTriggered: {
                    sessionPathField.text = root.sessionTargetPath
                    sessionPathDialog.open()
                }
            }
        }

        Menu {
            objectName: "menuHelp"
            title: qsTr("Help")

            MenuItem {
                objectName: "menuHelpAbout"
                text: qsTr("About")
                onTriggered: aboutDialog.open()
            }

            MenuItem {
                objectName: "menuHelpGit"
                text: qsTr("git")
                onTriggered: gitDialog.open()
            }
        }
    }

    Dialogs.SessionSettingsDialog {
        id: sessionDialog
        objectName: "sessionSettingsDialog"
        sessionTargetPath: root.sessionTargetPath
        onImportCompleted: function(message) {
            root.statusText = qsTr("Active session. %1").arg(backendStatusText(message, qsTr("Ready.")))
            sessionDialog.mandatoryMode = false
            sessionDialog.close()
        }
    }

    Dialogs.UploadDraftDialog {
        id: uploadDialog
        objectName: "uploadDraftDialog"
    }

    Dialogs.PrintDraftDialog {
        id: printDialog
        objectName: "printDraftDialog"
    }

    AppDialogFrame {
        id: sessionPathDialog
        objectName: "sessionPathDialog"
        title: qsTr("Session Settings")
        subtitle: qsTr("Target session.json path used by Session > Import HAR.")
        minimumWidth: 640
        maximumWidth: 640
        minimumHeight: 260
        maximumHeight: 260
        dialogSize: "medium"

        AppTextField {
            id: sessionPathField
            objectName: "sessionPathField"
            Layout.fillWidth: true
            text: root.sessionTargetPath
        }

        footerLeadingData: [
            AppButton {
                text: qsTr("Default")
                onClicked: {
                    if (typeof sessionImportBridge !== "undefined"
                            && sessionImportBridge !== null
                            && typeof sessionImportBridge.defaultSessionPath === "function") {
                        sessionPathField.text = String(sessionImportBridge.defaultSessionPath())
                    }
                }
            }
        ]

        footerTrailingData: [
            AppButton {
                text: qsTr("Close")
                onClicked: sessionPathDialog.close()
            },
            AppButton {
                text: qsTr("Apply")
                variant: "primary"
                onClicked: {
                    root.sessionTargetPath = sessionPathField.text.trim().length > 0
                            ? sessionPathField.text.trim()
                            : root.sessionTargetPath
                    root.statusText = qsTr("Session target: %1").arg(root.sessionTargetPath)
                    sessionPathDialog.close()
                }
            }
        ]
    }

    AppDialogFrame {
        id: sessionDetailsDialog
        objectName: "sessionDetailsDialog"
        title: qsTr("Session details")
        minimumWidth: 620
        maximumWidth: 620
        minimumHeight: 330
        maximumHeight: 330
        dialogSize: "medium"

        ScrollView {
            id: sessionDetailsScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                active: true
            }
            ScrollBar.horizontal: ScrollBar {
                policy: ScrollBar.AsNeeded
                active: true
            }

            TextArea {
                id: sessionDetailsTextArea
                objectName: "sessionDetailsTextArea"
                width: sessionDetailsScroll.availableWidth
                height: Math.max(sessionDetailsScroll.availableHeight, sessionDetailsTextArea.contentHeight)
                readOnly: true
                text: root.sessionDetailsText
                wrapMode: TextEdit.Wrap
                color: Theme.fgPrimary
                background: null
            }
        }

        footerTrailingData: [
            AppButton {
                text: qsTr("Close")
                onClicked: sessionDetailsDialog.close()
            }
        ]
    }

    AppDialogFrame {
        id: render3dWorkersDialog
        objectName: "render3dWorkersDialog"
        title: qsTr("3D generation settings")
        subtitle: qsTr("Configure the number of parallel mesh workers.")
        minimumWidth: 460
        maximumWidth: 560

        bodyData: [
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gapRow

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Parallel workers")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                }

                AppSpinBox {
                    id: render3dWorkersSpin
                    objectName: "render3dWorkersSpin"
                    from: 1
                    to: 16
                    value: root.render3dWorkerCount
                }
            },
            Text {
                Layout.fillWidth: true
                text: qsTr("Default: 4. Higher values use more CPU and memory during 3D generation.")
                color: Theme.fgSecondary
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontCaptionPx
            }
        ]

        footerTrailingData: [
            AppButton {
                text: qsTr("Cancel")
                variant: "secondary"
                onClicked: render3dWorkersDialog.close()
            },
            AppButton {
                objectName: "render3dWorkersApplyButton"
                text: qsTr("Apply")
                variant: "primary"
                onClicked: {
                    root.persistRender3dWorkerCount(render3dWorkersSpin.value)
                    root.statusText = qsTr("3D generation workers updated: %1").arg(root.render3dWorkerCount)
                    render3dWorkersDialog.close()
                }
            }
        ]
    }

    AppDialogFrame {
        id: render3dPaletteDialog
        objectName: "render3dPaletteDialog"
        title: qsTr("3D color settings")
        subtitle: qsTr("Choose a color pair for the printed part and the viewport background.")
        minimumWidth: 520
        maximumWidth: 620
        property var paletteOptions: []
        property int pendingIndex: 0
        readonly property var pendingPalette: paletteOptions.length > 0
                                               ? paletteOptions[Math.max(0, Math.min(
                                                     pendingIndex, paletteOptions.length - 1))]
                                               : root.render3dPaletteFor("technical_cyan")

        function prepare() {
            paletteOptions = root.render3dPaletteOptions()
            pendingIndex = root.render3dPaletteIndex(root.render3dPalettePreset)
        }

        bodyData: [
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Color preset")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                }

                AppComboBox {
                    id: render3dPaletteCombo
                    objectName: "render3dPaletteCombo"
                    Layout.fillWidth: true
                    model: render3dPaletteDialog.paletteOptions
                    textRole: "label"
                    currentIndex: render3dPaletteDialog.pendingIndex
                    onActivated: function(index) {
                        render3dPaletteDialog.pendingIndex = index
                    }
                }
            },
            Text {
                Layout.fillWidth: true
                text: qsTr("Preview")
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontBodyPx
                font.bold: true
            },
            Rectangle {
                objectName: "render3dPalettePreviewBackground"
                Layout.fillWidth: true
                Layout.preferredHeight: 130
                radius: Theme.radiusControl
                color: render3dPaletteDialog.pendingPalette.backgroundColor
                border.width: Theme.borderWidth
                border.color: Theme.borderDefault

                Rectangle {
                    objectName: "render3dPalettePreviewPart"
                    anchors.centerIn: parent
                    width: Math.min(parent.width * 0.48, 240)
                    height: 62
                    radius: Theme.radiusControl
                    color: render3dPaletteDialog.pendingPalette.partColor
                    border.width: 1
                    border.color: Qt.darker(color, 1.25)
                }
            },
            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Part: %1").arg(
                              render3dPaletteDialog.pendingPalette.partColor)
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontCaptionPx
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Background: %1").arg(
                              render3dPaletteDialog.pendingPalette.backgroundColor)
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontCaptionPx
                }
            },
            Text {
                Layout.fillWidth: true
                text: qsTr("The colors are applied immediately without rebuilding the 3D mesh.")
                color: Theme.fgSecondary
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontCaptionPx
            }
        ]

        footerTrailingData: [
            AppButton {
                text: qsTr("Cancel")
                variant: "secondary"
                onClicked: render3dPaletteDialog.close()
            },
            AppButton {
                objectName: "render3dPaletteApplyButton"
                text: qsTr("Apply")
                variant: "primary"
                onClicked: {
                    var selected = render3dPaletteDialog.pendingPalette
                    root.persistRender3dPalettePreset(selected.id)
                    root.statusText = qsTr("3D colors updated: %1").arg(selected.label)
                    render3dPaletteDialog.close()
                }
            }
        ]
    }

    AppDialogFrame {
        id: languageDialog
        objectName: "languageDialog"
        title: qsTr("Language Settings")
        subtitle: qsTr("Choose app language. System language is used by default.")
        minimumWidth: 520
        maximumWidth: 680
        property string pendingLanguage: root.persistedLanguageCode
        property var languageOptions: []

        function languageIndexFor(codeValue) {
            var code = String(codeValue || "system")
            for (var i = 0; i < languageOptions.length; ++i) {
                var option = languageOptions[i]
                if (String(option.value || "") === code)
                    return i
            }
            return 0
        }

        function rebuildLanguageOptions() {
            languageOptions = [
                { "value": "system", "label": qsTr("System default") },
                { "value": "en", "label": qsTr("English") },
                { "value": "fr", "label": qsTr("French") }
            ]
        }

        onOpened: {
            rebuildLanguageOptions()
            pendingLanguage = root.hasI18nBridge()
                    ? String(appI18nBridge.languageCode || "system")
                    : "system"
            languageCombo.currentIndex = languageIndexFor(pendingLanguage)
        }

        SectionHeader {
            Layout.fillWidth: true
            title: qsTr("Language")
            subtitle: qsTr("Fallback: English if system language is unavailable")
        }

        AppComboBox {
            id: languageCombo
            Layout.fillWidth: true
            textRole: "label"
            model: languageDialog.languageOptions
            onActivated: {
                if (currentIndex >= 0 && currentIndex < languageDialog.languageOptions.length)
                    languageDialog.pendingLanguage = String(languageDialog.languageOptions[currentIndex].value || "system")
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.hasI18nBridge()
                  ? qsTr("Current effective language: %1").arg(String(appI18nBridge.effectiveLanguageCode || "en"))
                  : qsTr("Language bridge unavailable.")
            color: Theme.fgSecondary
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontCaptionPx
        }

        footerTrailingData: [
            AppButton {
                text: qsTr("Close")
                variant: "secondary"
                onClicked: languageDialog.close()
            },
            AppButton {
                text: qsTr("Apply")
                variant: "primary"
                enabled: root.hasI18nBridge()
                onClicked: {
                    if (!root.hasI18nBridge())
                        return
                    appI18nBridge.setLanguage(languageDialog.pendingLanguage)
                    root.persistedLanguageCode = String(appI18nBridge.languageCode || "system")
                    var selectedLabel = (languageCombo.currentIndex >= 0
                                         && languageCombo.currentIndex < languageDialog.languageOptions.length)
                            ? String(languageDialog.languageOptions[languageCombo.currentIndex].label || "")
                            : qsTr("System default")
                    root.statusText = qsTr("Language updated: %1").arg(selectedLabel)
                    languageDialog.close()
                }
            }
        ]
    }

    AppDialogFrame {
        id: themeDialog
        objectName: "themeDialog"
        title: qsTr("Theme Settings")
        subtitle: qsTr("Preset + accent applied live. Persistence is saved on validation.")
        minimumWidth: 560
        maximumWidth: 680
        property string pendingTheme: root.persistedThemeName
        property string pendingAccent: root.persistedAccentName
        property bool committed: false

        function refreshComboState() {
            var tIndex = themePresetCombo.find(themeDialog.pendingTheme)
            themePresetCombo.currentIndex = tIndex >= 0 ? tIndex : 0

            var aIndex = accentPresetCombo.find(themeDialog.pendingAccent)
            accentPresetCombo.currentIndex = aIndex >= 0 ? aIndex : 0
        }

        onOpened: {
            committed = false
            pendingTheme = root.persistedThemeName
            pendingAccent = root.persistedAccentName
            refreshComboState()
            root.applyThemeSelection(pendingTheme, pendingAccent, false)
        }

        onClosed: {
            if (!committed) {
                root.restorePersistedTheme()
            }
        }

        SectionHeader {
            Layout.fillWidth: true
            title: qsTr("Preset")
            subtitle: qsTr("Global app palette")
        }

        AppComboBox {
            id: themePresetCombo
            objectName: "themePresetCombo"
            Layout.fillWidth: true
            model: Theme.availableThemePresets()
            onActivated: function() {
                themeDialog.pendingTheme = String(currentText)
                root.applyThemeSelection(themeDialog.pendingTheme, themeDialog.pendingAccent, false)
            }
        }

        SectionHeader {
            Layout.fillWidth: true
            title: qsTr("Accent")
            subtitle: qsTr("Primary action color")
        }

        AppComboBox {
            id: accentPresetCombo
            objectName: "accentPresetCombo"
            Layout.fillWidth: true
            model: Theme.availableAccents()
            onActivated: function() {
                themeDialog.pendingAccent = String(currentText)
                root.applyThemeSelection(themeDialog.pendingTheme, themeDialog.pendingAccent, false)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 116
            radius: Theme.radiusControl
            color: Theme.bgSurface
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 8

                Text {
                    text: qsTr("Preview")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontSectionPx
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        radius: 8
                        color: Theme.accent
                        border.width: Theme.borderWidth
                        border.color: Theme.borderDefault
                    }

                    Text {
                        text: qsTr("Primary text sample")
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontBodyPx
                    }

                    Text {
                        text: qsTr("Secondary text sample")
                        color: Theme.fgSecondary
                        opacity: 0.9
                        font.pixelSize: Theme.fontCaptionPx
                    }

                    Item { Layout.fillWidth: true }

                    AppButton {
                        text: qsTr("Primary")
                        variant: "primary"
                    }
                }
            }
        }

        AppButton {
            text: qsTr("Reset to defaults")
            variant: "secondary"
            onClicked: {
                themeDialog.pendingTheme = "WarmLight"
                themeDialog.pendingAccent = "Teal"
                themeDialog.refreshComboState()
                root.applyThemeSelection(themeDialog.pendingTheme, themeDialog.pendingAccent, false)
            }
        }

        footerTrailingData: [
            AppButton {
                text: qsTr("Close")
                variant: "secondary"
                onClicked: themeDialog.close()
            },
            AppButton {
                text: qsTr("Apply")
                variant: "primary"
                onClicked: {
                    themeDialog.committed = true
                    root.applyThemeSelection(themeDialog.pendingTheme, themeDialog.pendingAccent, true)
                    root.statusText = qsTr("Theme: %1 / Accent: %2")
                            .arg(root.persistedThemeName)
                            .arg(root.persistedAccentName)
                    themeDialog.close()
                }
            }
        ]
        footerLeadingData: [
            AppButton {
                text: qsTr("Cancel changes")
                variant: "secondary"
                onClicked: {
                    themeDialog.committed = false
                    themeDialog.close()
                }
            }
        ]

        onRejected: {
            committed = false
        }
    }

    AppDialogFrame {
        id: aboutDialog
        objectName: "aboutDialog"
        title: qsTr("About")
        minimumWidth: 560
        maximumWidth: 560
        minimumHeight: 280
        maximumHeight: 280
        dialogSize: "small"

        Text {
            Layout.fillWidth: true
            text: qsTr("Anycubic Cloud Control Room\nVersion: 0.1.0\nQt/QML interface for cloud workflow and runtime logs.")
            color: Theme.fgPrimary
            wrapMode: Text.WordWrap
        }

        Item { Layout.fillHeight: true }

        footerTrailingData: [
            AppButton {
                text: qsTr("Close")
                onClicked: aboutDialog.close()
            }
        ]
    }

    AppDialogFrame {
        id: gitDialog
        objectName: "gitDialog"
        title: qsTr("git")
        minimumWidth: 620
        maximumWidth: 620
        minimumHeight: 320
        maximumHeight: 320
        dialogSize: "medium"

        ScrollView {
            id: gitInfoScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                active: true
            }
            ScrollBar.horizontal: ScrollBar {
                policy: ScrollBar.AsNeeded
                active: true
            }

            TextArea {
                id: gitInfoTextArea
                objectName: "gitInfoTextArea"
                width: Math.max(gitInfoScroll.availableWidth, gitInfoTextArea.contentWidth)
                height: Math.max(gitInfoScroll.availableHeight, gitInfoTextArea.contentHeight)
                readOnly: true
                wrapMode: TextEdit.NoWrap
                text: qsTr("Useful shortcuts:\n")
                    + "- git status --short\n"
                    + "- git log --oneline -n 20\n"
                    + "- git branch --show-current\n"
                    + "- git diff --stat"
                color: Theme.fgPrimary
                background: null
            }
        }

        footerTrailingData: [
            AppButton {
                text: qsTr("Close")
                onClicked: gitDialog.close()
            }
        ]
    }

    Loader {
        id: controlRoomShellLoader
        anchors.fill: parent
        sourceComponent: controlRoomShellComponent
    }

    Component {
        id: controlRoomShellComponent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            property int currentTabIndex: controlTabs.currentIndex
            function setCurrentTabIndex(index) {
                if (index >= 0 && index < controlTabs.count) {
                    controlTabs.currentIndex = index
                }
            }

            Rectangle {
                id: controlRoomHeader
                objectName: "controlRoomHeader"
                Layout.fillWidth: true
                Layout.preferredHeight: root.debugUi ? 80 : 64
                radius: Theme.radiusDialog
                color: Theme.bgSurface
                border.width: Theme.borderWidth
                border.color: Theme.borderDefault

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.paddingPage
                    anchors.rightMargin: Theme.paddingPage
                    anchors.topMargin: 8
                    anchors.bottomMargin: 8
                    spacing: Theme.gapRow

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            id: titleLabel
                            objectName: "controlRoomTitle"
                            text: qsTr("Anycubic Cloud Control Room")
                            color: Theme.fgPrimary
                            font.pixelSize: 20
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            id: subtitleLabel
                            objectName: "controlRoomSubtitle"
                            text: root.statusText
                            color: Theme.fgSecondary
                            opacity: 0.9
                            font.pixelSize: Theme.fontCaptionPx
                            elide: Text.ElideRight
                        }
                    }

                    RowLayout {
                        visible: root.debugUi
                        spacing: 8

                        HeaderActionButton {
                            id: printDialogButton
                            objectName: "printDialogButton"
                            text: qsTr("Print Dialog")
                            onClicked: root.openPrintDialog()
                        }

                        HeaderActionButton {
                            id: uploadDialogButton
                            objectName: "uploadDialogButton"
                            text: qsTr("Upload Dialog")
                            variant: "primary"
                            onClicked: root.openUploadDialog()
                        }
                    }
                }
            }

            Rectangle {
                id: tabsPanel
                objectName: "tabsPanel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusDialog
                color: "transparent"
                border.width: Theme.borderWidth
                border.color: Theme.borderDefault

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 0
                    spacing: 0

                    AppTabBar {
                        id: controlTabs
                        objectName: "controlRoomTabs"
                        Layout.fillWidth: true
                        tabVariant: "navigation"
                        tabLook: "classic"
                        tabSizingMode: "content"
                        minTabWidth: 120
                        connectActiveToPanel: true
                        panelColor: Theme.bgSurface
                        inactiveColor: "transparent"
                        stripColor: "transparent"
                        tabTopCornerRadius: Theme.radiusControl

                        AppTabButton {
                            objectName: "filesTabButton"
                            text: qsTr("Files")
                        }

                        AppTabButton {
                            objectName: "printerTabButton"
                            text: qsTr("Printers")
                        }

                        AppTabButton {
                            objectName: "mqttTabButton"
                            text: qsTr("MQTT")
                            visible: !root.prodUi
                        }

                        AppTabButton {
                            objectName: "logTabButton"
                            text: root.buildDebugEnabled
                                  ? qsTr("Logs")
                                  : qsTr("Logs (disabled in this build)")
                            visible: !root.prodUi
                        }

                        onCurrentIndexChanged: {
                            if (currentIndex === 1
                                    && printerPage
                                    && typeof printerPage.ensureStartupInitialized === "function") {
                                printerPage.ensureStartupInitialized()
                            }
                        }
                    }

                    StackLayout {
                        id: controlRoomStack
                        objectName: "controlRoomStack"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: Theme.borderWidth
                        Layout.rightMargin: Theme.borderWidth
                        Layout.bottomMargin: Theme.borderWidth
                        currentIndex: controlTabs.currentIndex

                        Pages.CloudFilesPage {
                            id: cloudFilesPage
                            objectName: "cloudFilesPage"
                            embeddedInTabsContainer: true
                            viewerEnabled: root.viewer3dEnabled
                            render3dWorkerCount: root.render3dWorkerCount
                            render3dPalettePreset: root.render3dPalettePreset
                            render3dPartColor: root.render3dPartColor
                            render3dBackgroundColor: root.render3dBackgroundColor
                            showAdvancedDetails: root.cloudFileAdvancedDetailsEnabled
                            onStatusBroadcast: function(message, severity, operationId) {
                                if (root !== null && root !== undefined
                                        && typeof root.pushGlobalStatus === "function") {
                                    root.pushGlobalStatus(message, severity, operationId)
                                }
                            }
                            onPwszUploadSettingsChanged: root.loadPwszUploadSettings()
                            onPrintIntentRequested: function(fileId, fileName, fileData) {
                                if (typeof printerPage.openRemotePrintFromFile === "function") {
                                    printerPage.openRemotePrintFromFile(fileId, fileName, fileData)
                                } else {
                                    root.pushGlobalStatus(qsTr("Remote print entrypoint is unavailable."),
                                                          "warn",
                                                          "op_files_print_entry")
                                }
                            }
                            onDirectPrintIntentRequested: function(localPath, fileName, completePreview) {
                                if (typeof printerPage.openDirectPrintFromLocalFile === "function") {
                                    printerPage.openDirectPrintFromLocalFile(localPath,
                                                                            fileName,
                                                                            completePreview,
                                                                            root.directDeleteLocalOnFailureEnabled)
                                } else {
                                    root.pushGlobalStatus(qsTr("Direct print entrypoint is unavailable."),
                                                          "warn",
                                                          "op_direct_print_entry")
                                }
                            }
                        }

                        Pages.PrinterPage {
                            id: printerPage
                            objectName: "printerPage"
                            embeddedInTabsContainer: true
                            developmentBuild: root.buildDebugEnabled
                            deferStartupInitialization: true
                            directDeleteLocalOnFailurePreference: root.directDeleteLocalOnFailureEnabled
                            pageActive: controlTabs.currentIndex === 1
                            onStatusBroadcast: function(message, severity, operationId) {
                                if (root !== null && root !== undefined
                                        && typeof root.pushGlobalStatus === "function") {
                                    root.pushGlobalStatus(message, severity, operationId)
                                }
                            }
                            onRemotePrintAccepted: function(printerId, taskId) {
                                controlTabs.currentIndex = 1
                            }
                        }

                        Item {
                            objectName: "mqttPageHost"
                            visible: !root.prodUi
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Loader {
                                id: mqttPageLoader
                                anchors.fill: parent
                                active: controlTabs.currentIndex === 2
                                sourceComponent: Pages.MqttPage {
                                    objectName: "mqttPage"
                                    embeddedInTabsContainer: true
                                    pageActive: controlTabs.currentIndex === 2
                                }
                            }
                        }

                        Item {
                            objectName: "logPageHost"
                            visible: !root.prodUi
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Loader {
                                id: logPageLoader
                                anchors.fill: parent
                                active: root.buildDebugEnabled && controlTabs.currentIndex === 3
                                source: "pages/LogPage.qml"
                            }

                            AppPageFrame {
                                anchors.fill: parent
                                visible: !root.buildDebugEnabled
                                embeddedInTabsContainer: true
                                sectionTitle: qsTr("Logs")
                                sectionSubtitle: qsTr("Debug tools are disabled in this build")

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.gapRow

                                    Text {
                                        text: qsTr("Rebuild with ACCLOUD_DEBUG=ON to enable the runtime log viewer.")
                                        color: Theme.fgSecondary
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }

                    }

                    InlineStatusBar {
                        objectName: "globalStatusBar"
                        Layout.fillWidth: true
                        message: root.globalStatusMsg
                        severity: root.globalStatusSev
                        operationId: root.globalStatusOpId
                        showOperationId: root.debugUi
                    }

                }
            }
        }
    }
}
