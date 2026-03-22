import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import "../components/Theme.js" as Theme
import "../components"

AppDialogFrame {
    id: root
    title: qsTr("Session Settings")
    subtitle: qsTr("Import a session from a HAR file. Analysis runs automatically when a file is selected.")
    minimumWidth: 860
    maximumWidth: 1120
    minimumHeight: 560
    maximumHeight: 740
    dialogSize: "large"
    property var importBridge: (typeof sessionImportBridge !== "undefined") ? sessionImportBridge : null
    property bool importInProgress: false
    property string statusMessage: qsTr("Status: ready for HAR analysis.")
    property string resultDetails: qsTr("No analysis executed.")
    property string sessionTargetPath: "~/.config/accloud/session.json"
    property bool pendingValid: false
    property string analyzedHarPath: ""

    // When true: the dialog cannot be closed until a valid analysis is saved.
    property bool mandatoryMode: false
    // Reason message shown at the top when the dialog opens in mandatory mode.
    property string startupMessage: ""

    allowScrimClose: false
    allowEscapeClose: false
    requestCloseCallback: requestClose

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

    function backendStatusDetail(rawMessage, fallbackMessage) {
        var text = String(rawMessage || "").trim()
        if (text.length === 0)
            return String(fallbackMessage || qsTr("No message"))
        if (/[\u4e00-\u9fff]/.test(text))
            text = text.replace(/[\u4e00-\u9fff]+/g, qsTr("localized backend message"))
        return text
    }

    function runAnalyzeForPath(harPath) {
        var trimmedHar = String(harPath).trim()
        if (trimmedHar.length === 0) {
            root.statusMessage = qsTr("Status: HAR path required.")
            root.pendingValid = false
            return
        }
        if (root.importBridge === null || typeof root.importBridge.analyzeHar !== "function") {
            root.statusMessage = qsTr("Status: backend bridge unavailable.")
            root.resultDetails = qsTr("Cannot analyze: sessionImportBridge is undefined.")
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
        var keysText = (keys.length > 0) ? keys.join(", ") : qsTr("(none)")
        var message = backendStatusDetail(response.message, qsTr("No message"))
        var targetPath = response.sessionPath !== undefined ? String(response.sessionPath) : root.sessionTargetPath

        root.pendingValid = ok
        root.analyzedHarPath = trimmedHar
        root.resultDetails =
            qsTr("HAR analysis: %1").arg(ok ? qsTr("VALID") : qsTr("ERROR"))
            + qsTr("\nHAR: %1").arg(trimmedHar)
            + qsTr("\nSession target: %1").arg(targetPath)
            + qsTr("\nMessage: %1").arg(message)
            + qsTr("\nEntries: %1 accepted / %2 visited").arg(entriesAccepted).arg(entriesVisited)
            + qsTr("\nToken keys: %1").arg(keysText)
            + qsTr("\n\nThe session will be saved when this window is closed.")

        if (ok) {
            root.statusMessage = qsTr("Status: valid analysis. Close to save.")
        } else {
            root.statusMessage = qsTr("Status: invalid analysis.")
        }
    }

    function requestClose() {
        if (root.importInProgress) {
            return
        }

        if (root.mandatoryMode && !root.pendingValid) {
            root.statusMessage = qsTr("Status: valid HAR import required before closing.")
            return
        }

        if (root.pendingValid) {
            if (root.importBridge === null || typeof root.importBridge.commitPendingSession !== "function") {
                root.statusMessage = qsTr("Status: commit unavailable (bridge unavailable).")
                return
            }

            root.importInProgress = true
            var commit = root.importBridge.commitPendingSession(root.sessionTargetPath)
            root.importInProgress = false

            var commitOk = commit.ok === true
            var commitMsg = backendStatusDetail(commit.message, qsTr("No message"))
            if (!commitOk) {
                root.statusMessage = qsTr("Status: save failed.")
                root.resultDetails += qsTr("\n\nSave: FAILED - %1").arg(commitMsg)
                return
            }

            var connOk = commit.connectionOk === true
            var connMsg = backendStatusDetail(commit.connectionMessage, commitMsg)
            root.statusMessage = qsTr("Status: session saved.")
            root.resultDetails += qsTr("\n\nSave: OK\nCloud connection: ")
                    + (connOk ? qsTr("OK") : qsTr("FAILED - %1").arg(connMsg))
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
        root.statusMessage = qsTr("Status: ready for HAR analysis.")
        root.resultDetails = qsTr("No analysis executed.")
    }

    FileDialog {
        id: harFileDialog
        title: qsTr("Select HAR file")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("HAR files (*.har *.json)"), qsTr("All files (*)")]
        onAccepted: {
            var localPath = root.localPathFromUrl(selectedFile)
            harFileField.text = localPath
            root.runAnalyzeForPath(localPath)
        }
    }

    bodyData: [
        // Reason banner shown only in mandatory mode.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: startupReasonText.implicitHeight + 16
            visible: root.mandatoryMode && root.startupMessage.length > 0
            radius: Theme.radiusControl
            color: Theme.danger
            opacity: 0.85

            Text {
                id: startupReasonText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: Theme.paddingDialog
                text: root.startupMessage
                color: Theme.fgOnDanger
                wrapMode: Text.WordWrap
                font.bold: true
            }
        },
        Text {
            text: qsTr("Session target (Settings > Session): ") + root.sessionTargetPath
            color: Theme.fgSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        },
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            radius: Theme.radiusDialog
            color: Theme.bgSurface
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingDialog
                spacing: Theme.gapRow

                FormRow {
                    Layout.fillWidth: true
                    labelText: qsTr("HAR file")
                    labelWidth: 90

                    AppTextField {
                        id: harFileField
                        objectName: "harFileField"
                        Layout.fillWidth: true
                        placeholderText: qsTr("/path/to/session.har")
                        onAccepted: root.runAnalyzeForPath(harFileField.text)
                    }
                    AppButton {
                        objectName: "harBrowseButton"
                        text: qsTr("Browse")
                        onClicked: harFileDialog.open()
                    }
                }
            }
        },
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusDialog
            color: Theme.bgDialog
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingDialog
                spacing: Theme.gapRow

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Security reminders:\n- Keep HAR files encrypted at rest.\n- Remove signed URLs from shared logs.\n- Session is saved only when closing after a valid analysis.")
                    color: Theme.fgSecondary
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusControl
                    color: Theme.bgSurface
                    border.width: Theme.borderWidth
                    border.color: Theme.borderDefault

                    ScrollView {
                        id: resultPanelScroll
                        anchors.fill: parent
                        anchors.margins: Theme.paddingDialog
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
                            color: Theme.fgPrimary
                            background: null
                        }
                    }
                }
            }
        },
        Text {
            id: statusLabel
            objectName: "harImportStatusLabel"
            Layout.fillWidth: true
            text: root.statusMessage
            color: Theme.fgSecondary
            wrapMode: Text.WordWrap
        }
    ]

    footerTrailingData: [
        AppButton {
            id: closeButton
            objectName: "harImportCloseButton"
            text: root.pendingValid ? qsTr("Close and Save") : qsTr("Close")
            enabled: !root.importInProgress
                     && (!root.mandatoryMode || root.pendingValid)
            onClicked: root.requestClose()
        }
    ]
}
