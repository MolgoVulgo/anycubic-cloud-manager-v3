import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

AppDialogFrame {
    id: root
    title: qsTr("Remote Print Config")
    subtitle: qsTr("Review task, printer and options before start")
    minimumWidth: 760
    maximumWidth: 900

    property var printersModel: null
    property string remotePrinterId: ""
    property string selectedCloudFileId: ""
    property string selectedFileName: "-"
    property string selectedPrinterName: "-"
    property string selectedPrintTime: "-"
    property string selectedResinUsage: "-"
    property bool optionHighPriority: false
    property bool optionDeleteAfterPrint: false
    property bool optionDryRun: false
    property bool remotePrintAllowed: true
    property string remotePrintBlockReason: ""
    property var translateLocalizedTextProvider: null

    signal remotePrinterChanged(string printerId)
    signal optionHighPriorityToggled(bool checked)
    signal optionDeleteAfterPrintToggled(bool checked)
    signal optionDryRunToggled(bool checked)
    signal refreshGuardRequested()
    signal changePrinterRequested()
    signal moreRequested()
    signal closeRequested()
    signal startRequested()

    onOpened: {
        for (var i = 0; i < printersModel.count; ++i) {
            if (String(printersModel.get(i).id) === remotePrinterId) {
                remotePrinterCombo.currentIndex = i
                break
            }
        }
        refreshGuardRequested()
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Print Task")
        subtitle: qsTr("Selected cloud file summary")
    }

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
                text: qsTr("Printer: %1 | Est: %2 | Resin: %3")
                        .arg(root.selectedPrinterName)
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
        title: qsTr("Select Printer")
        subtitle: qsTr("Change target printer if needed")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        AppComboBox {
            id: remotePrinterCombo
            Layout.fillWidth: true
            model: root.printersModel
            textRole: "name"

            Component.onCompleted: {
                for (var i = 0; i < printersModel.count; ++i) {
                    if (String(printersModel.get(i).id) === remotePrinterId) {
                        currentIndex = i
                        break
                    }
                }
            }

            onActivated: {
                if (currentIndex >= 0 && currentIndex < printersModel.count) {
                    root.remotePrinterChanged(String(printersModel.get(currentIndex).id))
                    root.refreshGuardRequested()
                }
            }
        }

        AppButton {
            text: qsTr("Change")
            variant: "secondary"
            onClicked: root.changePrinterRequested()
        }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: qsTr("Options")
        subtitle: qsTr("Fast options before start")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        AppCheckBox {
            text: qsTr("High priority")
            checked: root.optionHighPriority
            onToggled: root.optionHighPriorityToggled(checked)
        }

        AppCheckBox {
            text: qsTr("Delete file after print")
            checked: root.optionDeleteAfterPrint
            onToggled: root.optionDeleteAfterPrintToggled(checked)
        }

        AppCheckBox {
            text: qsTr("Dry-run")
            checked: root.optionDryRun
            onToggled: root.optionDryRunToggled(checked)
        }

        Item { Layout.fillWidth: true }

        AppButton {
            text: qsTr("More")
            variant: "secondary"
            onClicked: root.moreRequested()
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
