import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme

Item {
    id: root
    objectName: "logPage"
    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Text {
            text: "Runtime Logs"
            color: Theme.textPrimary
            font.pixelSize: 26
            font.bold: true
        }

        Text {
            text: "Tail multi-sources, filtres op_id/component/event, recherche texte."
            color: Theme.textSecondary
            font.pixelSize: 14
        }

        Rectangle {
            objectName: "logFiltersPanel"
            Layout.fillWidth: true
            Layout.preferredHeight: 78
            radius: 12
            color: Theme.panel
            border.width: 1
            border.color: Theme.panelStroke

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                ComboBox {
                    objectName: "logLevelFilter"
                    model: ["ALL", "INFO+", "WARN+", "ERROR"]
                    Layout.preferredWidth: 100
                }

                ComboBox {
                    objectName: "logSourceFilter"
                    model: ["All sources", "app", "http", "fault", "render3d"]
                    Layout.preferredWidth: 140
                }

                ComboBox {
                    objectName: "logComponentFilter"
                    model: ["component:*", "cloud", "jobs", "photons", "viewer3d"]
                    Layout.preferredWidth: 150
                }

                TextField {
                    objectName: "logOpIdFilter"
                    placeholderText: "op_id exact"
                    Layout.preferredWidth: 150
                }

                TextField {
                    objectName: "logQueryFilter"
                    placeholderText: "query contains"
                    Layout.fillWidth: true
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

            TextArea {
                id: logsArea
                objectName: "logsTextArea"
                anchors.fill: parent
                anchors.margins: 10
                readOnly: true
                font.family: "JetBrains Mono"
                color: Theme.mono
                text: "2026-03-03T09:15:02+01:00 app INFO session.loaded op_id=op_boot_102f\n"
                    + "2026-03-03T09:15:07+01:00 http INFO files.list.success count=2 op_id=op_files_refresh_1da3\n"
                    + "2026-03-03T09:15:08+01:00 render3d WARN viewer.unavailable reason=no_context\n"
                    + "2026-03-03T09:15:12+01:00 app INFO printer.poll.loaded count=3 op_id=op_printer_refresh_09f1"
                wrapMode: TextEdit.NoWrap
            }
        }
    }
}
