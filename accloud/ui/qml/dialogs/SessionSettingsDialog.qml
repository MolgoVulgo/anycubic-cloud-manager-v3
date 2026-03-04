import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import "../components/Theme.js" as Theme

Dialog {
    id: root
    title: "Session Settings"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    readonly property real overlayWidth: (Overlay.overlay && Overlay.overlay.width > 0) ? Overlay.overlay.width : 1480
    readonly property real overlayHeight: (Overlay.overlay && Overlay.overlay.height > 0) ? Overlay.overlay.height : 920
    width: Math.min(1120, Math.max(860, overlayWidth * 0.82))
    height: Math.min(740, Math.max(560, overlayHeight * 0.84))
    property var importBridge: (typeof sessionImportBridge !== "undefined") ? sessionImportBridge : null
    property bool importInProgress: false
    property string statusMessage: "Status: prêt pour l'analyse HAR."
    property string resultDetails: "Aucune analyse exécutée."
    property string sessionTargetPath: "~/.config/accloud/session.json"
    property bool pendingValid: false
    property string analyzedHarPath: ""

    // Quand true : le dialog ne peut pas être fermé tant qu'aucune analyse valide n'a été sauvegardée.
    property bool mandatoryMode: false
    // Message de raison affiché en haut quand le dialog s'ouvre en mode obligatoire.
    property string startupMessage: ""

    closePolicy: Popup.NoAutoClose

    signal importCompleted(string message)

    function localPathFromUrl(urlValue) {
        var value = String(urlValue)
        if (value.indexOf("file://") === 0) {
            if (Qt.platform.os === "windows" && value.indexOf("file:///") === 0) {
                return decodeURIComponent(value.slice(8))
            }
            return decodeURIComponent(value.slice(7))
        }
        return value
    }

    function runAnalyzeForPath(harPath) {
        var trimmedHar = String(harPath).trim()
        if (trimmedHar.length === 0) {
            root.statusMessage = "Status: chemin HAR requis."
            root.pendingValid = false
            return
        }
        if (root.importBridge === null || typeof root.importBridge.analyzeHar !== "function") {
            root.statusMessage = "Status: bridge backend indisponible."
            root.resultDetails = "Impossible d'analyser : sessionImportBridge non défini."
            root.pendingValid = false
            return
        }

        root.importInProgress = true
        var response = root.importBridge.analyzeHar(trimmedHar, root.sessionTargetPath)
        root.importInProgress = false

        var ok = response.ok === true
        var entriesVisited = response.entriesVisited !== undefined ? response.entriesVisited : 0
        var entriesAccepted = response.entriesAccepted !== undefined ? response.entriesAccepted : 0
        var keys = response.tokenKeys !== undefined ? response.tokenKeys : []
        var keysText = (keys.length > 0) ? keys.join(", ") : "(aucun)"
        var message = response.message !== undefined ? String(response.message) : "Aucun message"
        var targetPath = response.sessionPath !== undefined ? String(response.sessionPath) : root.sessionTargetPath

        root.pendingValid = ok
        root.analyzedHarPath = trimmedHar
        root.resultDetails =
            "Analyse HAR: " + (ok ? "VALIDE" : "ERREUR")
            + "\nHAR: " + trimmedHar
            + "\nSession target: " + targetPath
            + "\nMessage: " + message
            + "\nEntrées: " + entriesAccepted + " acceptées / " + entriesVisited + " visitées"
            + "\nClés de tokens: " + keysText
            + "\n\nLa session sera sauvegardée à la fermeture de cette fenêtre."

        if (ok) {
            root.statusMessage = "Status: analyse valide. Fermez pour sauvegarder."
        } else {
            root.statusMessage = "Status: analyse invalide."
        }
    }

    function requestClose() {
        if (root.importInProgress) {
            return
        }

        if (root.mandatoryMode && !root.pendingValid) {
            root.statusMessage = "Status: import HAR valide requis avant fermeture."
            return
        }

        if (root.pendingValid) {
            if (root.importBridge === null || typeof root.importBridge.commitPendingSession !== "function") {
                root.statusMessage = "Status: commit impossible (bridge indisponible)."
                return
            }

            root.importInProgress = true
            var commit = root.importBridge.commitPendingSession(root.sessionTargetPath)
            root.importInProgress = false

            var commitOk = commit.ok === true
            var commitMsg = commit.message !== undefined ? String(commit.message) : "Aucun message"
            if (!commitOk) {
                root.statusMessage = "Status: sauvegarde échouée."
                root.resultDetails += "\n\nSauvegarde: ECHEC — " + commitMsg
                return
            }

            var connOk = commit.connectionOk === true
            var connMsg = commit.connectionMessage !== undefined ? String(commit.connectionMessage) : commitMsg
            root.statusMessage = "Status: session sauvegardée."
            root.resultDetails += "\n\nSauvegarde: OK\nConnexion cloud: "
                    + (connOk ? "OK" : "ECHEC — " + connMsg)
            root.pendingValid = false
            root.importCompleted(connMsg)
        } else if (root.importBridge !== null && typeof root.importBridge.discardPendingSession === "function") {
            root.importBridge.discardPendingSession()
        }

        root.close()
    }

    onOpened: {
        if (root.importBridge !== null && typeof root.importBridge.discardPendingSession === "function") {
            root.importBridge.discardPendingSession()
        }
        root.pendingValid = false
        root.analyzedHarPath = ""
        root.statusMessage = "Status: prêt pour l'analyse HAR."
        root.resultDetails = "Aucune analyse exécutée."
    }

    FileDialog {
        id: harFileDialog
        title: "Select HAR file"
        fileMode: FileDialog.OpenFile
        nameFilters: ["HAR files (*.har *.json)", "All files (*)"]
        onAccepted: {
            var localPath = root.localPathFromUrl(selectedFile)
            harFileField.text = localPath
            root.runAnalyzeForPath(localPath)
        }
    }

    background: Rectangle {
        radius: 14
        color: Theme.card
        border.width: 1
        border.color: Theme.panelStroke
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        // Bandeau de raison affiché uniquement en mode obligatoire
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: startupReasonText.implicitHeight + 16
            visible: root.mandatoryMode && root.startupMessage.length > 0
            radius: 8
            color: Theme.danger
            opacity: 0.85

            Text {
                id: startupReasonText
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                anchors.margins: 10
                text: root.startupMessage
                color: "#fff4f4"
                wrapMode: Text.WordWrap
                font.bold: true
            }
        }

        Text {
            text: "Importer une session depuis un HAR. Analyse automatique au choix du fichier."
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Text {
            text: "Session target (Parametre > session): " + root.sessionTargetPath
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            radius: 12
            color: Theme.panel
            border.width: 1
            border.color: Theme.panelStroke

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Text { text: "HAR file"; color: Theme.textPrimary; Layout.preferredWidth: 90 }
                TextField {
                    id: harFileField
                    objectName: "harFileField"
                    Layout.fillWidth: true
                    placeholderText: "/path/to/session.har"
                    onAccepted: root.runAnalyzeForPath(harFileField.text)
                }
                Button {
                    objectName: "harBrowseButton"
                    text: "Browse"
                    onClicked: harFileDialog.open()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12
            color: Theme.cardAlt
            border.width: 1
            border.color: Theme.panelStroke

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: "Security reminders:\n- Keep HAR files encrypted at rest.\n- Remove signed URLs from shared logs.\n- La sauvegarde session intervient uniquement à la fermeture si analyse valide."
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: Theme.panel
                    border.width: 1
                    border.color: Theme.panelStroke

                    ScrollView {
                        id: resultPanelScroll
                        anchors.fill: parent
                        anchors.margins: 8
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            active: true
                        }
                        ScrollBar.horizontal: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            active: true
                        }

                        TextArea {
                            id: resultPanel
                            objectName: "harImportResultPanel"
                            width: Math.max(resultPanelScroll.availableWidth, implicitWidth)
                            height: Math.max(resultPanelScroll.availableHeight, contentHeight)
                            readOnly: true
                            wrapMode: Text.Wrap
                            text: root.resultDetails
                            color: Theme.textPrimary
                            background: null
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                id: statusLabel
                objectName: "harImportStatusLabel"
                Layout.fillWidth: true
                text: root.statusMessage
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }

            Button {
                id: closeButton
                objectName: "harImportCloseButton"
                text: root.pendingValid ? "Fermer et sauvegarder" : "Fermer"
                enabled: !root.importInProgress
                         && (!root.mandatoryMode || root.pendingValid)
                onClicked: root.requestClose()
            }
        }
    }
}
