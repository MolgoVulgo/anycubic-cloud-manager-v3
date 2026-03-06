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
    title: "Anycubic Cloud Control Room"
    property string statusText: "Vérification de la session en cours…"
    property bool debugUi: Qt.application.arguments
                               && Qt.application.arguments.indexOf("--debug-ui") !== -1
    property string sessionTargetPath: "~/.config/accloud/session.json"
    property string sessionDetailsText: "Aucune vérification de session exécutée."
    property int render3dDefaultQualityIndex: 2
    property string render3dDefaultPalette: "Palette Steel"
    property bool render3dDefaultContourOnly: false
    property int render3dDefaultCutoff: 100
    property int render3dDefaultStride: 1
    property string persistedThemeName: "WarmLight"
    property string persistedAccentName: "Teal"

    function hasUiSettingsBridge() {
        return (typeof uiSettingsBridge !== "undefined")
                && uiSettingsBridge !== null
                && typeof uiSettingsBridge.getString === "function"
                && typeof uiSettingsBridge.setString === "function"
    }

    function reloadControlRoomShell() {
        if (typeof controlRoomShellLoader === "undefined"
                || controlRoomShellLoader === null
                || controlRoomShellLoader.active !== true) {
            return
        }

        var tabIndex = 0
        if (controlRoomShellLoader.item
                && controlRoomShellLoader.item.currentTabIndex !== undefined) {
            tabIndex = controlRoomShellLoader.item.currentTabIndex
        }

        controlRoomShellLoader.active = false
        controlRoomShellLoader.active = true

        if (controlRoomShellLoader.item
                && typeof controlRoomShellLoader.item.setCurrentTabIndex === "function") {
            controlRoomShellLoader.item.setCurrentTabIndex(tabIndex)
        }
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

        root.reloadControlRoomShell()
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
            // Normalise les valeurs persistées si elles étaient invalides.
            uiSettingsBridge.setString("ui.themeName", root.persistedThemeName)
            uiSettingsBridge.setString("ui.accentName", root.persistedAccentName)
            if (typeof uiSettingsBridge.sync === "function")
                uiSettingsBridge.sync()
        }
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
            sessionDetailsText = "Backend session indisponible."
            statusText = "Session details indisponible."
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
        Qt.callLater(function() {
            if (typeof sessionImportBridge === "undefined"
                    || sessionImportBridge === null
                    || typeof sessionImportBridge.checkStartup !== "function") {
                root.statusText = "Mode interface: backend indisponible."
                return
            }
            if (typeof sessionImportBridge.defaultSessionPath === "function") {
                root.sessionTargetPath = String(sessionImportBridge.defaultSessionPath())
            }

            var check = sessionImportBridge.checkStartup()
            if (check.sessionExists === true && check.connectionOk === true) {
                root.statusText = "Session active. Auto-refresh toutes les 30s."
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
            title: "Session"

            MenuItem {
                objectName: "menuSessionDetails"
                text: "Details"
                onTriggered: root.showSessionDetails()
            }

            MenuItem {
                objectName: "menuSessionImportHar"
                text: "import HAR"
                onTriggered: sessionDialog.open()
            }
        }

        Menu {
            objectName: "menuParametre"
            title: "Parametre"

            MenuItem {
                objectName: "menuSettingsSession"
                text: "session"
                onTriggered: {
                    sessionPathField.text = root.sessionTargetPath
                    sessionPathDialog.open()
                }
            }

            MenuItem {
                objectName: "menuSettingsTheme"
                text: "theme"
                onTriggered: {
                    root.statusText = "Parametre theme: ouverture du panneau de configuration."
                    themeDialog.open()
                }
            }

            MenuItem {
                objectName: "menuSettingsRender3d"
                text: "rendu 3d"
                onTriggered: {
                    root.statusText = "Ouverture des parametres par defaut du rendu 3D."
                    render3dDefaultsDialog.open()
                }
            }
        }

        Menu {
            objectName: "menuHelp"
            title: "?"

            MenuItem {
                objectName: "menuHelpAbout"
                text: "A propos"
                onTriggered: aboutDialog.open()
            }

            MenuItem {
                objectName: "menuHelpGit"
                text: "git"
                onTriggered: gitDialog.open()
            }
        }
    }

    Dialogs.SessionSettingsDialog {
        id: sessionDialog
        objectName: "sessionSettingsDialog"
        sessionTargetPath: root.sessionTargetPath
        onImportCompleted: function(message) {
            root.statusText = "Session active. " + message
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

    Dialog {
        id: sessionPathDialog
        objectName: "sessionPathDialog"
        title: "Parametre session"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: Overlay.overlay
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: 640
        height: 260

        background: Rectangle {
            radius: 12
            color: Theme.card
            border.width: 1
            border.color: Theme.panelStroke
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: "Chemin cible de session.json utilisé par Session > import HAR."
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }

            TextField {
                id: sessionPathField
                objectName: "sessionPathField"
                Layout.fillWidth: true
                text: root.sessionTargetPath
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "Defaut"
                    onClicked: {
                        if (typeof sessionImportBridge !== "undefined"
                                && sessionImportBridge !== null
                                && typeof sessionImportBridge.defaultSessionPath === "function") {
                            sessionPathField.text = String(sessionImportBridge.defaultSessionPath())
                        }
                    }
                }
                Button {
                    text: "Appliquer"
                    onClicked: {
                        root.sessionTargetPath = sessionPathField.text.trim().length > 0
                                ? sessionPathField.text.trim()
                                : root.sessionTargetPath
                        root.statusText = "Session target: " + root.sessionTargetPath
                        sessionPathDialog.close()
                    }
                }
                Button {
                    text: "Fermer"
                    onClicked: sessionPathDialog.close()
                }
            }
        }
    }

    Dialog {
        id: sessionDetailsDialog
        objectName: "sessionDetailsDialog"
        title: "Session details"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: Overlay.overlay
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: 620
        height: 330

        background: Rectangle {
            radius: 12
            color: Theme.card
            border.width: 1
            border.color: Theme.panelStroke
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

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
                    background: null
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "Fermer"
                    onClicked: sessionDetailsDialog.close()
                }
            }
        }
    }

    AppDialogFrame {
        id: themeDialog
        objectName: "themeDialog"
        title: "Theme Settings"
        subtitle: "Preset + accent appliqués en live. La persistance est faite à la validation."
        minimumWidth: 560
        maximumWidth: 680
        showCloseButton: false
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
            title: "Preset"
            subtitle: "Palette globale de l'application"
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
            title: "Accent"
            subtitle: "Couleur principale des actions primary"
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
                anchors.margins: 10
                spacing: 8

                Text {
                    text: "Preview"
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
                        border.width: 1
                        border.color: Theme.borderDefault
                    }

                    Text {
                        text: "Primary text sample"
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontBodyPx
                    }

                    Text {
                        text: "Secondary text sample"
                        color: Theme.fgSecondary
                        opacity: 0.9
                        font.pixelSize: Theme.fontCaptionPx
                    }

                    Item { Layout.fillWidth: true }

                    AppButton {
                        text: "Primary"
                        variant: "primary"
                    }
                }
            }
        }

        AppButton {
            text: "Reset to defaults"
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
                text: "Close"
                variant: "secondary"
                onClicked: themeDialog.close()
            },
            AppButton {
                text: "Apply"
                variant: "primary"
                onClicked: {
                    themeDialog.committed = true
                    root.applyThemeSelection(themeDialog.pendingTheme, themeDialog.pendingAccent, true)
                    root.statusText = "Theme: " + root.persistedThemeName + " / Accent: " + root.persistedAccentName
                    themeDialog.close()
                }
            }
        ]
        footerLeadingData: [
            AppButton {
                text: "Cancel changes"
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

    Dialog {
        id: render3dDefaultsDialog
        objectName: "render3dDefaultsDialog"
        title: "Parametre rendu 3D (defaut)"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: Overlay.overlay
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: 620
        height: 410

        background: Rectangle {
            radius: 12
            color: Theme.card
            border.width: 1
            border.color: Theme.panelStroke
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: "Reglez ici les valeurs par defaut du rendu 3D (sans ouvrir le viewer)."
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Qualite"; Layout.preferredWidth: 130 }
                ComboBox {
                    id: renderQualityCombo
                    objectName: "renderQualityCombo"
                    Layout.fillWidth: true
                    model: ["Quality 33", "Quality 66", "Quality 100"]
                    currentIndex: root.render3dDefaultQualityIndex
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Palette"; Layout.preferredWidth: 130 }
                ComboBox {
                    id: renderPaletteCombo
                    objectName: "renderPaletteCombo"
                    Layout.fillWidth: true
                    model: ["Palette Steel", "Palette Resin", "Palette Heat"]
                    currentIndex: Math.max(0, model.indexOf(root.render3dDefaultPalette))
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Layer cutoff"; Layout.preferredWidth: 130 }
                Slider {
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
                    color: Theme.textSecondary
                    Layout.preferredWidth: 46
                    horizontalAlignment: Text.AlignRight
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Stride"; Layout.preferredWidth: 130 }
                SpinBox {
                    id: renderStrideSpin
                    objectName: "renderStrideSpin"
                    from: 1
                    to: 8
                    value: root.render3dDefaultStride
                }
                Item { Layout.fillWidth: true }
                CheckBox {
                    id: renderContourOnlyCheck
                    objectName: "renderContourOnlyCheck"
                    text: "Contour only"
                    checked: root.render3dDefaultContourOnly
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "Reinitialiser"
                    onClicked: {
                        renderQualityCombo.currentIndex = 2
                        renderPaletteCombo.currentIndex = 0
                        renderCutoffSlider.value = 100
                        renderStrideSpin.value = 1
                        renderContourOnlyCheck.checked = false
                    }
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Appliquer"
                    onClicked: {
                        root.render3dDefaultQualityIndex = renderQualityCombo.currentIndex
                        root.render3dDefaultPalette = String(renderPaletteCombo.currentText)
                        root.render3dDefaultCutoff = Math.round(renderCutoffSlider.value)
                        root.render3dDefaultStride = renderStrideSpin.value
                        root.render3dDefaultContourOnly = renderContourOnlyCheck.checked
                        root.statusText = "Defaults rendu 3D appliques: "
                                + String(renderQualityCombo.currentText)
                                + ", " + root.render3dDefaultPalette
                                + ", cutoff " + root.render3dDefaultCutoff + "%"
                    }
                }
                Button {
                    text: "Fermer"
                    onClicked: render3dDefaultsDialog.close()
                }
            }
        }
    }

    Dialog {
        id: aboutDialog
        objectName: "aboutDialog"
        title: "A propos"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: Overlay.overlay
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: 560
        height: 280

        background: Rectangle {
            radius: 12
            color: Theme.card
            border.width: 1
            border.color: Theme.panelStroke
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: "Anycubic Cloud Control Room\nVersion: 0.1.0\nInterface Qt/QML pour workflow cloud, logs runtime, et rendu 3D."
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "Fermer"
                    onClicked: aboutDialog.close()
                }
            }
        }
    }

    Dialog {
        id: gitDialog
        objectName: "gitDialog"
        title: "git"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: Overlay.overlay
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: 620
        height: 320

        background: Rectangle {
            radius: 12
            color: Theme.card
            border.width: 1
            border.color: Theme.panelStroke
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

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
                    text: "Raccourcis utiles:\n"
                        + "- git status --short\n"
                        + "- git log --oneline -n 20\n"
                        + "- git branch --show-current\n"
                        + "- git diff --stat"
                    background: null
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "Fermer"
                    onClicked: gitDialog.close()
                }
            }
        }
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
                            text: "Anycubic Cloud Control Room"
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
                            text: "Print Dialog"
                            onClicked: root.openPrintDialog()
                        }

                        HeaderActionButton {
                            id: viewerDialogButton
                            objectName: "viewerDialogButton"
                            text: "3D Viewer Dialog"
                            onClicked: root.openViewerDialog()
                        }

                        HeaderActionButton {
                            id: uploadDialogButton
                            objectName: "uploadDialogButton"
                            text: "Upload Dialog"
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

                    TabBar {
                        id: controlTabs
                        objectName: "controlRoomTabs"
                        Layout.fillWidth: true

                        TabButton {
                            objectName: "filesTabButton"
                            text: "Files"
                        }

                        TabButton {
                            objectName: "printerTabButton"
                            text: "Printers"
                        }

                        TabButton {
                            objectName: "logTabButton"
                            text: "Logs"
                        }
                    }

                    StackLayout {
                        objectName: "controlRoomStack"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: controlTabs.currentIndex

                        Pages.CloudFilesPage {
                            objectName: "cloudFilesPage"
                        }

                        Pages.PrinterPage {
                            objectName: "printerPage"
                            debugUi: root.debugUi
                        }

                        Pages.LogPage {
                            objectName: "logPage"
                        }
                    }
                }
            }
        }
    }
}
