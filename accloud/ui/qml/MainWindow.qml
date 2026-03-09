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
    property bool debugUi: buildDebugEnabled
                               && Qt.application.arguments
                               && Qt.application.arguments.indexOf("--debug-ui") !== -1
    property string sessionTargetPath: "~/.config/accloud/session.json"
    property string sessionDetailsText: qsTr("No session check executed yet.")
    property int render3dDefaultQualityIndex: 2
    property string render3dDefaultPalette: "Palette Steel"
    property bool render3dDefaultContourOnly: false
    property int render3dDefaultCutoff: 100
    property int render3dDefaultStride: 1
    property string persistedThemeName: "WarmLight"
    property string persistedAccentName: "Teal"
    property string persistedLanguageCode: "system"

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

    function openUploadDialog() {
        uploadDialog.open()
    }

    function openPrintDialog() {
        printDialog.open()
    }

    function openViewerDialog() {
        viewerDialog.open()
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
        statusText = String(details.message)
        sessionDetailsDialog.open()
    }

    Component.onCompleted: {
        root.loadThemeFromSettings()
        root.loadLanguageFromSettings()
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

            var check = sessionImportBridge.checkStartup()
            if (check.sessionExists === true && check.connectionOk === true) {
                root.statusText = qsTr("Active session. Auto-refresh every 30s.")
            } else {
                root.statusText = String(check.message)
                sessionDialog.startupMessage = String(check.message)
                sessionDialog.mandatoryMode = true
                sessionDialog.open()
            }
        })
    }

    component HeaderActionButton: AppButton {
        variant: "secondary"
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
            objectName: "menuSession"
            title: qsTr("Session")

            MenuItem {
                objectName: "menuSessionDetails"
                text: qsTr("Details")
                onTriggered: root.showSessionDetails()
            }

            MenuItem {
                objectName: "menuSessionImportHar"
                text: qsTr("import HAR")
                onTriggered: sessionDialog.open()
            }
        }

        Menu {
            objectName: "menuParametre"
            title: qsTr("Settings")

            MenuItem {
                objectName: "menuSettingsSession"
                text: qsTr("Session")
                onTriggered: {
                    sessionPathField.text = root.sessionTargetPath
                    sessionPathDialog.open()
                }
            }

            MenuItem {
                objectName: "menuSettingsTheme"
                text: qsTr("Theme")
                onTriggered: {
                    root.statusText = qsTr("Opening theme settings panel.")
                    themeDialog.open()
                }
            }

            MenuItem {
                objectName: "menuSettingsRender3d"
                text: qsTr("3D rendering")
                onTriggered: {
                    root.statusText = qsTr("Opening default 3D rendering settings.")
                    render3dDefaultsDialog.open()
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
        }

        Menu {
            objectName: "menuHelp"
            title: qsTr("?")

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
            root.statusText = qsTr("Active session. %1").arg(message)
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

    Dialogs.ViewerDraftDialog {
        id: viewerDialog
        objectName: "viewerDraftDialog"
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
        id: languageDialog
        objectName: "languageDialog"
        title: qsTr("Language Settings")
        subtitle: qsTr("Choose app language. System language is used by default.")
        minimumWidth: 520
        maximumWidth: 680
        property string pendingLanguage: root.persistedLanguageCode
        property var languageCodes: ["system", "en", "fr"]
        property var languageLabels: []

        function languageIndexFor(codeValue) {
            var code = String(codeValue || "system")
            for (var i = 0; i < languageCodes.length; ++i) {
                if (String(languageCodes[i]) === code)
                    return i
            }
            return 0
        }

        function rebuildLanguageLabels() {
            languageLabels = [
                qsTr("System default"),
                qsTr("English"),
                qsTr("French")
            ]
        }

        onOpened: {
            rebuildLanguageLabels()
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
            model: languageDialog.languageLabels
            onActivated: {
                if (currentIndex >= 0 && currentIndex < languageDialog.languageCodes.length)
                    languageDialog.pendingLanguage = String(languageDialog.languageCodes[currentIndex])
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
                    root.statusText = qsTr("Language updated: %1").arg(languageCombo.currentText)
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
        id: render3dDefaultsDialog
        objectName: "render3dDefaultsDialog"
        title: qsTr("Default 3D Rendering Settings")
        subtitle: qsTr("Set default 3D rendering values here (without opening the viewer).")
        minimumWidth: 620
        maximumWidth: 620
        minimumHeight: 410
        maximumHeight: 410
        dialogSize: "medium"

        RowLayout {
            Layout.fillWidth: true
            Text { text: qsTr("Quality"); Layout.preferredWidth: 130 }
            AppComboBox {
                id: renderQualityCombo
                objectName: "renderQualityCombo"
                Layout.fillWidth: true
                textRole: "label"
                model: [
                    { "value": "q33", "label": qsTr("Quality 33") },
                    { "value": "q66", "label": qsTr("Quality 66") },
                    { "value": "q100", "label": qsTr("Quality 100") }
                ]
                currentIndex: root.render3dDefaultQualityIndex
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: qsTr("Palette"); Layout.preferredWidth: 130 }
            AppComboBox {
                id: renderPaletteCombo
                objectName: "renderPaletteCombo"
                Layout.fillWidth: true
                textRole: "label"
                model: [
                    { "value": "Palette Steel", "label": qsTr("Palette Steel") },
                    { "value": "Palette Resin", "label": qsTr("Palette Resin") },
                    { "value": "Palette Heat", "label": qsTr("Palette Heat") }
                ]
                Component.onCompleted: {
                    for (var i = 0; i < model.length; ++i) {
                        if (String(model[i].value) === root.render3dDefaultPalette) {
                            currentIndex = i
                            return
                        }
                    }
                    currentIndex = 0
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: qsTr("Layer cutoff"); Layout.preferredWidth: 130 }
            AppSlider {
                id: renderCutoffSlider
                objectName: "renderCutoffSlider"
                Layout.fillWidth: true
                from: 0
                to: 100
                stepSize: 1
                value: root.render3dDefaultCutoff
            }
            Text {
                text: Math.round(renderCutoffSlider.value) + "%"
                color: Theme.fgSecondary
                Layout.preferredWidth: 46
                horizontalAlignment: Text.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: qsTr("Stride"); Layout.preferredWidth: 130 }
            AppSpinBox {
                id: renderStrideSpin
                objectName: "renderStrideSpin"
                from: 1
                to: 8
                value: root.render3dDefaultStride
            }
            Item { Layout.fillWidth: true }
            AppCheckBox {
                id: renderContourOnlyCheck
                objectName: "renderContourOnlyCheck"
                text: qsTr("Contour only")
                checked: root.render3dDefaultContourOnly
            }
        }

        Item { Layout.fillHeight: true }

        footerLeadingData: [
            AppButton {
                text: qsTr("Reset")
                onClicked: {
                    renderQualityCombo.currentIndex = 2
                    renderPaletteCombo.currentIndex = 0
                    renderCutoffSlider.value = 100
                    renderStrideSpin.value = 1
                    renderContourOnlyCheck.checked = false
                }
            }
        ]

        footerTrailingData: [
            AppButton {
                text: qsTr("Close")
                onClicked: render3dDefaultsDialog.close()
            },
            AppButton {
                text: qsTr("Apply")
                variant: "primary"
                onClicked: {
                    root.render3dDefaultQualityIndex = renderQualityCombo.currentIndex
                    if (renderPaletteCombo.currentIndex >= 0
                            && renderPaletteCombo.currentIndex < renderPaletteCombo.model.length) {
                        root.render3dDefaultPalette = String(renderPaletteCombo.model[renderPaletteCombo.currentIndex].value)
                    }
                    root.render3dDefaultCutoff = Math.round(renderCutoffSlider.value)
                    root.render3dDefaultStride = renderStrideSpin.value
                    root.render3dDefaultContourOnly = renderContourOnlyCheck.checked
                    root.statusText = qsTr("Applied 3D defaults: %1, %2, cutoff %3%")
                            .arg(String(renderQualityCombo.currentText))
                            .arg(root.render3dDefaultPalette)
                            .arg(root.render3dDefaultCutoff)
                }
            }
        ]
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
            text: qsTr("Anycubic Cloud Control Room\nVersion: 0.1.0\nQt/QML interface for cloud workflow, runtime logs, and 3D rendering.")
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
            anchors.margins: Theme.paddingPage
            spacing: Theme.gapSection

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
                Layout.preferredHeight: root.debugUi ? 108 : 76
                radius: Theme.radiusDialog
                color: Theme.bgSurface
                border.width: Theme.borderWidth
                border.color: Theme.borderDefault

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.paddingPage
                    anchors.rightMargin: Theme.paddingPage
                    anchors.topMargin: 12
                    anchors.bottomMargin: 12
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
                            id: viewerDialogButton
                            objectName: "viewerDialogButton"
                            text: qsTr("3D Viewer Dialog")
                            onClicked: root.openViewerDialog()
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
                objectName: "tabsPanel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusDialog
                color: Theme.bgSurface
                border.width: Theme.borderWidth
                border.color: Theme.borderDefault

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.paddingPage
                    spacing: Theme.gapRow

                    AppTabBar {
                        id: controlTabs
                        objectName: "controlRoomTabs"
                        Layout.fillWidth: true
                        tabVariant: "navigation"
                        tabLook: "classic"
                        tabSizingMode: "equal"
                        minTabWidth: 140
                        connectActiveToPanel: true
                        panelColor: Theme.bgSurface

                        AppTabButton {
                            objectName: "filesTabButton"
                            text: qsTr("Files")
                        }

                        AppTabButton {
                            objectName: "printerTabButton"
                            text: qsTr("Printers")
                        }

                        AppTabButton {
                            objectName: "logTabButton"
                            text: root.buildDebugEnabled
                                  ? qsTr("Logs")
                                  : qsTr("Logs (disabled in this build)")
                        }
                    }

                    StackLayout {
                        objectName: "controlRoomStack"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: controlTabs.currentIndex

                        Pages.CloudFilesPage {
                            objectName: "cloudFilesPage"
                            onStatusBroadcast: function(message, severity, operationId) {
                                root.pushGlobalStatus(message, severity, operationId)
                            }
                        }

                        Pages.PrinterPage {
                            objectName: "printerPage"
                            debugUi: root.debugUi
                            onStatusBroadcast: function(message, severity, operationId) {
                                root.pushGlobalStatus(message, severity, operationId)
                            }
                        }

                        Item {
                            objectName: "logPageHost"
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Loader {
                                id: logPageLoader
                                anchors.fill: parent
                                active: root.buildDebugEnabled
                                source: "pages/LogPage.qml"
                            }

                            AppPageFrame {
                                anchors.fill: parent
                                visible: !root.buildDebugEnabled
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
                    }
                }
            }
        }
    }
}
