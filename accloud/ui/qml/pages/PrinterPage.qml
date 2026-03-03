import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Item {
    id: root
    objectName: "printerPage"
    Layout.fillWidth: true
    Layout.fillHeight: true

    ListModel {
        id: printersModel
        ListElement {
            name: "M7-Workshop-A"
            state: "ONLINE"
            modelName: "Photon Mono M7"
            materialState: "Resin OK"
            fileProgress: "idle"
            elapsedRemaining: "-"
            layers: "-"
        }
        ListElement {
            name: "M5S-Line-2"
            state: "PRINTING"
            modelName: "Photon Mono M5s"
            materialState: "Resin 63%"
            fileProgress: "atlas_plate_v12.pwmb (43%)"
            elapsedRemaining: "02h10 / 02h52"
            layers: "1120 / 2586"
        }
        ListElement {
            name: "Backup-X2"
            state: "OFFLINE"
            modelName: "Photon Mono X2"
            materialState: "N/A"
            fileProgress: "N/A"
            elapsedRemaining: "-"
            layers: "-"
        }
    }

    function badgeColor(state) {
        if (state === "ONLINE") {
            return Theme.ok
        }
        if (state === "PRINTING") {
            return Theme.warn
        }
        return Theme.danger
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Text {
            text: "Printers"
            color: Theme.textPrimary
            font.pixelSize: 26
            font.bold: true
        }

        Text {
            text: "Station board cloud: refresh, status, details, and print entrypoint."
            color: Theme.textSecondary
            font.pixelSize: 14
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                objectName: "refreshPrintersButton"
                text: "Refresh printers"
            }
            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Repeater {
                model: ["Online 1", "Offline 1", "Printing 1", "Jobs 24h 7"]
                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    radius: 10
                    color: Theme.card
                    border.width: 1
                    border.color: Theme.panelStroke

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: Theme.textPrimary
                        font.bold: true
                    }
                }
            }
        }

        ErrorBanner {
            Layout.fillWidth: true
            message: "Loaded 3 printers. Last poll 12s ago."
            operationId: "op_printer_refresh_09f1"
            severity: "warn"
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 3
                radius: 12
                color: Theme.panel
                border.width: 1
                border.color: Theme.panelStroke

                ListView {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8
                    model: printersModel

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 132
                        radius: 12
                        color: Theme.card
                        border.width: 1
                        border.color: Theme.panelStroke

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true

                                Text {
                                    Layout.fillWidth: true
                                    text: model.name
                                    color: Theme.textPrimary
                                    font.bold: true
                                    font.pixelSize: 16
                                }

                                Rectangle {
                                    radius: 7
                                    color: root.badgeColor(model.state)
                                    implicitWidth: 84
                                    implicitHeight: 26

                                    Text {
                                        anchors.centerIn: parent
                                        text: model.state
                                        color: "#f9fffa"
                                        font.pixelSize: 12
                                        font.bold: true
                                    }
                                }
                            }

                            Text { text: model.modelName + " | " + model.materialState; color: Theme.textSecondary }
                            Text { text: "File/progress: " + model.fileProgress; color: Theme.textSecondary }
                            Text { text: "Elapsed/remaining/layers: " + model.elapsedRemaining + " | " + model.layers; color: Theme.textSecondary }

                            RowLayout {
                                Layout.fillWidth: true
                                Button { text: "Details" }
                                Item { Layout.fillWidth: true }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 2
                radius: 12
                color: Theme.cardAlt
                border.width: 1
                border.color: Theme.panelStroke

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    Text {
                        text: "Preview Payload"
                        color: Theme.textPrimary
                        font.pixelSize: 16
                        font.bold: true
                    }

                    TextArea {
                        objectName: "printerPayloadPreview"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        readOnly: true
                        font.family: "JetBrains Mono"
                        color: Theme.mono
                        text: "{\n  \"printer\": \"M5S-Line-2\",\n  \"state\": \"PRINTING\",\n  \"file_id\": \"f-2d8e91\",\n  \"progress\": 43,\n  \"remaining_sec\": 10320\n}"
                        wrapMode: TextEdit.NoWrap
                    }
                }
            }
        }
    }
}
