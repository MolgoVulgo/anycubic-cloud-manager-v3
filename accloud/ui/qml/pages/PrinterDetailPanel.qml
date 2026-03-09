import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Rectangle {
    id: root
    objectName: "deviceDetailsPanel"

    property var selectedPrinter: null
    property var selectedPrinterDetails: ({})
    property bool loadingPrinterHistory: false
    property var printerHistoryModel: null
    property bool showDebugLabels: false
    property string printersEndpointPath: ""
    property string printersEndpointRawJson: ""
    property string selectedPrinterHelpUrlText: ""

    property var statusChipTextProvider: null
    property var progressTextProvider: null
    property var timeTextProvider: null
    property var unixTimeTextProvider: null
    property var printStatusTextProvider: null
    property var prettyJsonProvider: null
    property bool embeddedInTabsContainer: false

    signal cloudFileRequested(string printerId)
    signal localFileRequested()

    function providerText(provider, arg, fallback) {
        return typeof provider === "function" ? String(provider(arg)) : fallback
    }

    Layout.fillWidth: true
    Layout.fillHeight: true
    radius: embeddedInTabsContainer ? 0 : Theme.radiusControl
    color: Theme.bgSurface
    border.width: embeddedInTabsContainer ? 0 : Theme.borderWidth
    border.color: Theme.borderDefault

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Text {
            text: qsTr("Device Details")
            color: Theme.fgPrimary
            font.pixelSize: Theme.fontTitlePx
            font.bold: true
        }

        Text {
            visible: root.selectedPrinter === null
            text: qsTr("Select a printer to view details and remote print entrypoints.")
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontBodyPx
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            visible: root.selectedPrinter !== null
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusControl
                    color: Theme.bgWindow
                    border.width: Theme.borderWidth
                    border.color: Theme.borderSubtle

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Text {
                            text: String(root.selectedPrinter ? root.selectedPrinter.name : "-")
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.fontTitlePx
                            font.bold: true
                        }

                        Text {
                            text: qsTr("Model: ") + String(root.selectedPrinter ? root.selectedPrinter.model : "-")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontBodyPx
                        }

                        Text {
                            text: qsTr("Firmware: ")
                                  + (String(root.selectedPrinterDetails.firmwareVersion || "").length > 0
                                     ? String(root.selectedPrinterDetails.firmwareVersion)
                                     : "-")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontBodyPx
                        }

                        RowLayout {
                            spacing: 8

                            Text {
                                text: qsTr("Status:")
                                color: Theme.fgSecondary
                                font.pixelSize: Theme.fontBodyPx
                            }

                            StatusChip {
                                status: root.providerText(root.statusChipTextProvider,
                                                          root.selectedPrinter ? root.selectedPrinter.state : "READY",
                                                          qsTr("Ready"))
                            }
                        }

                        Text {
                            visible: root.selectedPrinterHelpUrlText.length > 0
                            text: qsTr("Help: ") + root.selectedPrinterHelpUrlText
                            color: Theme.accent
                            font.pixelSize: Theme.fontCaptionPx
                            elide: Text.ElideRight
                        }

                        Text {
                            text: qsTr("Current job: %1 | Progress: %2 | Elapsed: %3 | Remaining: %4")
                                .arg(root.selectedPrinter && String(root.selectedPrinter.currentFile || "").length > 0
                                     ? String(root.selectedPrinter.currentFile)
                                     : "-")
                                .arg(root.providerText(root.progressTextProvider,
                                                       root.selectedPrinter ? root.selectedPrinter.progress : -1,
                                                       "-"))
                                .arg(root.providerText(root.timeTextProvider,
                                                       root.selectedPrinter ? root.selectedPrinter.elapsedSec : -1,
                                                       "-"))
                                .arg(root.providerText(root.timeTextProvider,
                                                       root.selectedPrinter ? root.selectedPrinter.remainingSec : -1,
                                                       "-"))
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            text: qsTr("Print count: %1 | Total print time: %2 | Material used: %3")
                                .arg(String(root.selectedPrinterDetails.printCount || "").length > 0
                                     ? String(root.selectedPrinterDetails.printCount)
                                     : "-")
                                .arg(String(root.selectedPrinterDetails.printTotalTime || "").length > 0
                                     ? String(root.selectedPrinterDetails.printTotalTime)
                                     : "-")
                                .arg(String(root.selectedPrinterDetails.materialUsed || "").length > 0
                                     ? String(root.selectedPrinterDetails.materialUsed)
                                     : "-")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            visible: String(root.selectedPrinterDetails.helpUrl || "").length > 0
                            text: qsTr("Device help: ") + String(root.selectedPrinterDetails.helpUrl || "")
                            color: Theme.accent
                            font.pixelSize: Theme.fontCaptionPx
                            elide: Text.ElideRight
                        }

                        Text {
                            visible: String(root.selectedPrinterDetails.quickStartUrl || "").length > 0
                            text: qsTr("Quick start: ") + String(root.selectedPrinterDetails.quickStartUrl || "")
                            color: Theme.accent
                            font.pixelSize: Theme.fontCaptionPx
                            elide: Text.ElideRight
                        }

                        Text {
                            visible: Number(root.selectedPrinterDetails.tools ? root.selectedPrinterDetails.tools.length : 0) > 0
                            text: qsTr("Tools: ")
                                  + (root.selectedPrinterDetails.tools
                                     ? root.selectedPrinterDetails.tools.slice(0, 6).join(", ")
                                     : "")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            visible: Number(root.selectedPrinterDetails.advances ? root.selectedPrinterDetails.advances.length : 0) > 0
                            text: qsTr("Advanced: ")
                                  + (root.selectedPrinterDetails.advances
                                     ? root.selectedPrinterDetails.advances.slice(0, 4).join(", ")
                                     : "")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            wrapMode: Text.WordWrap
                        }

                        Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            AppButton {
                                text: qsTr("From Cloud File")
                                variant: "primary"
                                enabled: root.selectedPrinter !== null
                                onClicked: root.cloudFileRequested(String(root.selectedPrinter.id || ""))
                            }

                            AppButton {
                                text: qsTr("From Local File")
                                variant: "secondary"
                                onClicked: root.localFileRequested()
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusControl
                    color: Theme.bgWindow
                    border.width: Theme.borderWidth
                    border.color: Theme.borderSubtle

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("Recent Jobs")
                            subtitle: root.loadingPrinterHistory ? qsTr("Loading...") : qsTr("Latest projects for this printer")
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 6
                            model: root.printerHistoryModel
                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                                active: true
                            }

                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 42
                                color: "transparent"

                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 2

                                    Text {
                                        Layout.fillWidth: true
                                        text: String(model.gcodeName || "-") + " • "
                                            + root.providerText(root.printStatusTextProvider, model.printStatus, "-")
                                        color: Theme.fgPrimary
                                        font.pixelSize: Theme.fontCaptionPx
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("Task %1 | Progress %2 | Start %3 | End %4")
                                            .arg(String(model.taskId || "-"))
                                            .arg(root.providerText(root.progressTextProvider, model.progress, "-"))
                                            .arg(root.providerText(root.unixTimeTextProvider, model.createTime, "-"))
                                            .arg(root.providerText(root.unixTimeTextProvider, model.endTime, "-"))
                                        color: Theme.fgSecondary
                                        font.pixelSize: Theme.fontCaptionPx
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            footer: Text {
                                width: parent ? parent.width : 0
                                visible: root.printerHistoryModel && root.printerHistoryModel.count === 0
                                text: root.loadingPrinterHistory
                                      ? qsTr("Loading history...")
                                      : qsTr("No project history for this printer.")
                                color: Theme.fgSecondary
                                font.pixelSize: Theme.fontCaptionPx
                                horizontalAlignment: Text.AlignHCenter
                                padding: 10
                            }
                        }
                    }
                }
            }

            Rectangle {
                objectName: "endpointJsonPanel"
                visible: root.showDebugLabels
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                radius: Theme.radiusControl
                color: Theme.bgWindow
                border.width: Theme.borderWidth
                border.color: Theme.borderSubtle

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Endpoint JSON: ") + root.printersEndpointPath
                        color: Theme.warning
                        font.pixelSize: Theme.fontCaptionPx
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        TextArea {
                            readOnly: true
                            text: typeof root.prettyJsonProvider === "function"
                                  ? String(root.prettyJsonProvider(root.printersEndpointRawJson))
                                  : String(root.printersEndpointRawJson || "")
                            wrapMode: TextEdit.NoWrap
                            color: Theme.fgPrimary
                            font.family: "monospace"
                            font.pixelSize: Theme.fontCaptionPx
                            background: Rectangle {
                                color: "transparent"
                            }
                        }
                    }
                }
            }
        }
    }
}
