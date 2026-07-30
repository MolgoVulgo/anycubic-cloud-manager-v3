import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

AppDialogFrame {
    id: root
    objectName: "remotePrintConfigDialog"
    title: qsTr("Remote Print Config")
    subtitle: qsTr("Review task, printer and options before start")
    dialogSize: "large"
    minimumWidth: 720
    maximumWidth: 900

    property var printersModel: null
    property var compatiblePrintersModel: null
    property string remotePrinterId: ""
    property string selectedCloudFileId: ""
    property string selectedFileName: "-"
    property string selectedPrinterName: "-"
    property string selectedPrintTime: "-"
    property string selectedResinUsage: "-"
    property bool optionDeleteAfterPrint: false
    property bool optionLiftCompensation: false
    property bool optionAutoResinCheck: true
    property bool remotePrintAllowed: true
    property string remotePrintBlockReason: ""
    property bool remotePrintPreparing: false
    property string remotePrintPrepareMessage: ""
    property var translateLocalizedTextProvider: null

    signal remotePrinterChanged(string printerId)
    signal optionDeleteAfterPrintToggled(bool checked)
    signal optionLiftCompensationToggled(bool checked)
    signal optionAutoResinCheckToggled(bool checked)
    signal refreshGuardRequested()
    signal closeRequested()
    signal startRequested()

    function modelCount(modelRef) {
        return modelRef !== null
                && modelRef !== undefined
                && modelRef.count !== undefined
                ? Number(modelRef.count)
                : 0
    }

    function activePrintersModel() {
        if (modelCount(compatiblePrintersModel) > 0)
            return compatiblePrintersModel
        return printersModel
    }

    function printerLabelInModel(modelRef, printerId) {
        var targetId = String(printerId || "")
        if (targetId.length === 0 || modelCount(modelRef) <= 0)
            return ""
        for (var i = 0; i < modelRef.count; ++i) {
            var row = modelRef.get(i)
            if (String(row.id || "") !== targetId)
                continue
            var label = String(row.name || "").trim()
            return label.length > 0 ? label : targetId
        }
        return ""
    }

    function printerDisplayText() {
        var label = printerLabelInModel(activePrintersModel(), remotePrinterId)
        if (label.length === 0)
            label = printerLabelInModel(printersModel, remotePrinterId)
        if (label.length === 0) {
            var fallback = String(selectedPrinterName || "").trim()
            if (fallback.length > 0 && fallback !== "-")
                label = fallback
        }
        if (label.length > 0)
            return label
        return remotePrintPreparing
                ? qsTr("Checking printer compatibility...")
                : qsTr("No printer available for remote print.")
    }

    function metricText(value) {
        var text = String(value || "").trim()
        return text.length > 0 ? text : "-"
    }

    function schedulePrinterSync() {
        printerModelSyncTimer.restart()
    }

    onOpened: {
        schedulePrinterSync()
        refreshGuardRequested()
    }

    onRemotePrinterIdChanged: schedulePrinterSync()
    onCompatiblePrintersModelChanged: schedulePrinterSync()
    onPrintersModelChanged: schedulePrinterSync()

    Timer {
        id: printerModelSyncTimer
        interval: 0
        repeat: false
        onTriggered: remotePrinterCombo.syncCurrentIndex()
    }

    Connections {
        target: root.compatiblePrintersModel
        ignoreUnknownSignals: true
        function onCountChanged() { root.schedulePrinterSync() }
        function onDataChanged() { root.schedulePrinterSync() }
        function onModelReset() { root.schedulePrinterSync() }
        function onRowsInserted() { root.schedulePrinterSync() }
        function onRowsRemoved() { root.schedulePrinterSync() }
    }

    Connections {
        target: root.printersModel
        ignoreUnknownSignals: true
        function onCountChanged() { root.schedulePrinterSync() }
        function onDataChanged() { root.schedulePrinterSync() }
        function onModelReset() { root.schedulePrinterSync() }
        function onRowsInserted() { root.schedulePrinterSync() }
        function onRowsRemoved() { root.schedulePrinterSync() }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: printTaskLayout.implicitHeight + 24
        radius: Theme.radiusDialog
        color: Theme.bgSurface
        border.width: Theme.borderWidth
        border.color: Theme.borderDefault

        ColumnLayout {
            id: printTaskLayout
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Text {
                text: qsTr("Print Task")
                color: Theme.fgSecondary
                font.pixelSize: Theme.fontCaptionPx
                font.bold: true
            }

            Text {
                id: selectedFileNameText
                objectName: "remotePrintFileName"
                Layout.fillWidth: true
                text: root.selectedFileName
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontSectionPx
                font.bold: true
                elide: Text.ElideMiddle
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    radius: Theme.radiusControl
                    color: Theme.bgCardSubtle
                    border.width: Theme.borderWidth
                    border.color: Theme.borderSubtle

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 2

                        Text {
                            text: qsTr("Print time")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }

                        Text {
                            id: estimatedTimeText
                            objectName: "remotePrintEstimatedTime"
                            text: root.metricText(root.selectedPrintTime)
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.fontBodyPx
                            font.bold: true
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    radius: Theme.radiusControl
                    color: Theme.bgCardSubtle
                    border.width: Theme.borderWidth
                    border.color: Theme.borderSubtle

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 2

                        Text {
                            text: qsTr("Resin usage")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }

                        Text {
                            id: resinUsageText
                            objectName: "remotePrintResinUsage"
                            text: root.metricText(root.selectedResinUsage)
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.fontBodyPx
                            font.bold: true
                        }
                    }
                }
            }
        }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Select Printer")
        subtitle: qsTr("Compatible target printer")
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: printerPanelLayout.implicitHeight + 20
        radius: Theme.radiusControl
        color: Theme.bgSurface
        border.width: Theme.borderWidth
        border.color: Theme.borderDefault

        ColumnLayout {
            id: printerPanelLayout
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            AppComboBox {
                id: remotePrinterCombo
                objectName: "remotePrinterCombo"
                Layout.fillWidth: true
                model: root.activePrintersModel()
                textRole: "name"
                displayText: root.printerDisplayText()
                enabled: !root.remotePrintPreparing && root.modelCount(model) > 0

                function syncCurrentIndex() {
                    var modelRef = root.activePrintersModel()
                    var nextIndex = -1
                    if (root.modelCount(modelRef) > 0) {
                        for (var i = 0; i < modelRef.count; ++i) {
                            if (String(modelRef.get(i).id || "") === root.remotePrinterId) {
                                nextIndex = i
                                break
                            }
                        }
                    }
                    currentIndex = nextIndex
                }

                Component.onCompleted: root.schedulePrinterSync()
                onModelChanged: root.schedulePrinterSync()

                onActivated: {
                    var modelRef = root.activePrintersModel()
                    if (root.modelCount(modelRef) > 0
                            && currentIndex >= 0
                            && currentIndex < modelRef.count) {
                        root.remotePrinterChanged(String(modelRef.get(currentIndex).id || ""))
                        root.refreshGuardRequested()
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.remotePrintPreparing
                      ? qsTr("Checking printer compatibility...")
                      : root.printerDisplayText()
                color: Theme.fgSecondary
                font.pixelSize: Theme.fontCaptionPx
                elide: Text.ElideRight
            }
        }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Options")
        subtitle: qsTr("Fast options before start")
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: optionsLayout.implicitHeight + 20
        radius: Theme.radiusControl
        color: Theme.bgSurface
        border.width: Theme.borderWidth
        border.color: Theme.borderDefault

        ColumnLayout {
            id: optionsLayout
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            AppCheckBox {
                text: qsTr("Delete file after print")
                checked: root.optionDeleteAfterPrint
                onToggled: root.optionDeleteAfterPrintToggled(checked)
            }

            AppCheckBox {
                text: qsTr("Lift compensation")
                checked: root.optionLiftCompensation
                onToggled: root.optionLiftCompensationToggled(checked)
            }

            AppCheckBox {
                text: qsTr("Auto resin check")
                checked: root.optionAutoResinCheck
                onToggled: root.optionAutoResinCheckToggled(checked)
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: statusRow.implicitHeight + 16
        visible: root.remotePrintPreparing || !root.remotePrintAllowed
        radius: Theme.radiusControl
        color: root.remotePrintPreparing ? Theme.statusInfoBg : Theme.statusErrorBg
        border.width: Theme.borderWidth
        border.color: root.remotePrintPreparing ? Theme.accentSoft : Theme.danger

        RowLayout {
            id: statusRow
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8

            Rectangle {
                Layout.preferredWidth: 8
                Layout.preferredHeight: 8
                radius: 4
                color: root.remotePrintPreparing ? Theme.accent : Theme.danger
            }

            Text {
                Layout.fillWidth: true
                text: root.remotePrintPreparing
                      ? (root.remotePrintPrepareMessage.length > 0
                         ? root.remotePrintPrepareMessage
                         : qsTr("Checking printer compatibility..."))
                      : root.remotePrintBlockReason.length > 0
                      ? (qsTr("Start blocked: %1").arg(
                             typeof root.translateLocalizedTextProvider === "function"
                             ? String(root.translateLocalizedTextProvider(root.remotePrintBlockReason))
                             : root.remotePrintBlockReason))
                      : qsTr("Start blocked by compatibility checks.")
                color: root.remotePrintPreparing ? Theme.fgPrimary : Theme.danger
                font.pixelSize: Theme.fontCaptionPx
                wrapMode: Text.WordWrap
            }
        }
    }

    footerTrailingData: [
        AppButton {
            text: qsTr("Close")
            variant: "secondary"
            onClicked: root.closeRequested()
        },
        AppButton {
            objectName: "remotePrintStartButton"
            text: qsTr("Start Printing")
            variant: "primary"
            enabled: !root.remotePrintPreparing
                     && root.selectedCloudFileId.length > 0
                     && root.remotePrinterId.length > 0
                     && root.remotePrintAllowed
            onClicked: root.startRequested()
        }
    ]
}
