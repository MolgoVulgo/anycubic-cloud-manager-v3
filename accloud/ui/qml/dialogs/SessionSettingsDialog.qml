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
    property string statusMessage: "Status: prêt pour l'import"
    property string resultDetails: "Aucun import exécuté."

    // Quand true : le dialog ne peut pas être fermé tant que la connexion n'est pas validée.
    property bool mandatoryMode: false
    // Message de raison affiché en haut quand le dialog s'ouvre en mode obligatoire.
    property string startupMessage: ""

    closePolicy: root.mandatoryMode ? Popup.NoAutoClose
                                    : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

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

    Component.onCompleted: {
        if (root.importBridge !== null && root.importBridge.defaultSessionPath) {
            var defaultPath = String(root.importBridge.defaultSessionPath())
            if (defaultPath.length > 0) {
                sessionTargetField.text = defaultPath
            }
        }
    }

    FileDialog {
        id: harFileDialog
        title: "Select HAR file"
        fileMode: FileDialog.OpenFile
        nameFilters: ["HAR files (*.har *.json)", "All files (*)"]
        onAccepted: {
            var localPath = root.localPathFromUrl(selectedFile)
            harFileField.text = localPath
            root.statusMessage = "Status: fichier HAR sélectionné."
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
            text: "Importer une session depuis un HAR et persister dans session.json"
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            radius: 12
            color: Theme.panel
            border.width: 1
            border.color: Theme.panelStroke

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "HAR file"; color: Theme.textPrimary; Layout.preferredWidth: 90 }
                    TextField {
                        id: harFileField
                        objectName: "harFileField"
                        Layout.fillWidth: true
                        placeholderText: "/path/to/session.har"
                    }
                    Button {
                        objectName: "harBrowseButton"
                        text: "Browse"
                        onClicked: harFileDialog.open()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Session target"; color: Theme.textPrimary; Layout.preferredWidth: 90 }
                    TextField {
                        id: sessionTargetField
                        objectName: "sessionTargetField"
                        Layout.fillWidth: true
                        text: "~/.config/accloud/session.json"
                    }
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
                    text: "Security reminders:\n- Keep HAR files encrypted at rest.\n- Remove signed URLs from shared logs.\n- Import merges headers and token fields found in payloads."
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

                    TextArea {
                        id: resultPanel
                        objectName: "harImportResultPanel"
                        anchors.fill: parent
                        anchors.margins: 8
                        readOnly: true
                        wrapMode: Text.Wrap
                        text: root.resultDetails
                        color: Theme.textPrimary
                        background: null
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

            // Le bouton Close est masqué en mode obligatoire
            Button {
                text: "Close"
                visible: !root.mandatoryMode
                enabled: !root.importInProgress
                onClicked: root.close()
            }

            Button {
                id: importButton
                objectName: "harImportButton"
                text: "Import HAR"
                font.bold: true
                enabled: !root.importInProgress
                         && harFileField.text.trim().length > 0
                onClicked: {
                    var harPath = harFileField.text.trim()
                    if (harPath.length === 0) {
                        root.statusMessage = "Status: chemin HAR requis."
                        return
                    }
                    if (root.importBridge === null) {
                        root.statusMessage = "Status: bridge backend indisponible."
                        root.resultDetails = "Impossible d'importer : sessionImportBridge non défini."
                        return
                    }

                    root.importInProgress = true
                    var targetPath = sessionTargetField.text.trim()
                    var response = root.importBridge.importHar(harPath, targetPath)
                    root.importInProgress = false

                    var ok = response.ok === true
                    var entriesVisited  = response.entriesVisited  !== undefined ? response.entriesVisited  : 0
                    var entriesAccepted = response.entriesAccepted !== undefined ? response.entriesAccepted : 0
                    var keys     = response.tokenKeys !== undefined ? response.tokenKeys : []
                    var keysText = (keys.length > 0) ? keys.join(", ") : "(aucun)"
                    var message  = response.message  !== undefined ? String(response.message) : "Aucun message"

                    root.resultDetails =
                        "Import: " + (ok ? "OK" : "ERREUR")
                        + "\nMessage: " + message
                        + "\nEntrées: " + entriesAccepted + " acceptées / " + entriesVisited + " visitées"
                        + "\nClés de tokens: " + keysText

                    if (!ok) {
                        root.statusMessage = "Status: import échoué."
                        return
                    }

                    if (typeof root.importBridge.checkStartup !== "function") {
                        root.statusMessage = "Status: import réussi."
                        root.importCompleted(message)
                        return
                    }

                    // Import réussi → vérifier la connexion cloud
                    root.statusMessage = "Status: import réussi. Vérification de la connexion…"
                    root.importInProgress = true
                    var check = root.importBridge.checkStartup()
                    root.importInProgress = false

                    var connOk  = check.connectionOk === true
                    var connMsg = check.message !== undefined ? String(check.message) : ""

                    root.resultDetails += "\n\nConnexion cloud: " + (connOk ? "OK" : "ECHEC — " + connMsg)

                    if (connOk) {
                        root.statusMessage = "Status: connexion validée."
                        root.importCompleted(message)
                    } else {
                        root.statusMessage = "Status: connexion échouée. Réessayez avec un HAR récent."
                    }
                }
                background: Rectangle {
                    radius: 8
                    color: parent.down ? Qt.darker(Theme.accent, 1.12) : Theme.accent
                }
                contentItem: Text {
                    text: parent.text
                    color: "#f8fffe"
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
