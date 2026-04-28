import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

AppDialogFrame {
    id: root
    objectName: "remotePrintConfigDialog"
    title: qsTr("Tache d'impression")
    subtitle: qsTr("Confirm printer and options before start")
    minimumWidth: 760
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
    property var translateLocalizedTextProvider: null

    signal remotePrinterChanged(string printerId)
    signal optionDeleteAfterPrintToggled(bool checked)
    signal optionLiftCompensationToggled(bool checked)
    signal optionAutoResinCheckToggled(bool checked)
    signal refreshGuardRequested()
    signal changePrinterRequested()
    signal closeRequested()
    signal startRequested()

    function activePrintersModel() {
        if (compatiblePrintersModel !== null
                && compatiblePrintersModel !== undefined
                && compatiblePrintersModel.count !== undefined) {
            return compatiblePrintersModel
        }
        return printersModel
    }

    onOpened: {
        var modelRef = activePrintersModel()
        if (modelRef && modelRef.count !== undefined) {
            for (var i = 0; i < modelRef.count; ++i) {
                if (String(modelRef.get(i).id) === remotePrinterId) {
                    remotePrinterCombo.currentIndex = i
                    break
                }
            }
        }
        refreshGuardRequested()
    }

    onRemotePrinterIdChanged: remotePrinterCombo.syncCurrentIndex()

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 86
        radius: Theme.radiusControl
        color: Theme.bgSurface
        border.width: Theme.borderWidth
        border.color: Theme.borderDefault

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            Text {
                text: root.selectedFileName
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontSectionPx
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                text: qsTr("Est: %1 | Resin: %2")
                        .arg(root.selectedPrintTime)
                        .arg(root.selectedResinUsage)
                color: Theme.fgSecondary
                font.pixelSize: Theme.fontBodyPx
                elide: Text.ElideRight
            }
        }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Printer")
        subtitle: qsTr("Compatible target printer")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        AppComboBox {
            id: remotePrinterCombo
            Layout.fillWidth: true
            model: root.activePrintersModel()
            textRole: "name"

            function syncCurrentIndex() {
                var modelRef = root.activePrintersModel()
                currentIndex = -1
                if (!modelRef || modelRef.count === undefined)
                    return
                for (var i = 0; i < modelRef.count; ++i) {
                    if (String(modelRef.get(i).id) === root.remotePrinterId) {
                        currentIndex = i
                        break
                    }
                }
            }

            Component.onCompleted: {
                syncCurrentIndex()
            }

            onModelChanged: syncCurrentIndex()

            onActivated: {
                var modelRef = root.activePrintersModel()
                if (modelRef
                        && modelRef.count !== undefined
                        && currentIndex >= 0
                        && currentIndex < modelRef.count) {
                    root.remotePrinterChanged(String(modelRef.get(currentIndex).id))
                    root.refreshGuardRequested()
                }
            }
        }

        AppButton {
            text: qsTr("Choose File")
            variant: "secondary"
            onClicked: root.changePrinterRequested()
        }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Options")
        subtitle: qsTr("Task options")
    }

    ColumnLayout {
        Layout.fillWidth: true
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

    Text {
        Layout.fillWidth: true
        visible: !root.remotePrintAllowed
        text: root.remotePrintBlockReason.length > 0
              ? (qsTr("Start blocked: %1").arg(
                     typeof root.translateLocalizedTextProvider === "function"
                     ? String(root.translateLocalizedTextProvider(root.remotePrintBlockReason))
                     : root.remotePrintBlockReason))
              : qsTr("Start blocked by compatibility checks.")
        color: Theme.danger
        font.pixelSize: Theme.fontCaptionPx
        wrapMode: Text.WordWrap
    }

    footerTrailingData: [
        AppButton {
            text: qsTr("Close")
            variant: "secondary"
            onClicked: root.closeRequested()
        },
        AppButton {
            text: qsTr("Start Printing")
            variant: "primary"
            enabled: root.selectedCloudFileId.length > 0
                     && root.remotePrinterId.length > 0
                     && root.remotePrintAllowed
            onClicked: root.startRequested()
        }
    ]
}
