import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Item {
    id: root
    objectName: "fileCard"
    property string fileName: "Untitled.pwmb"
    property string fileId: "file-unknown"
    property string status: "READY"
    property string sizeText: "0 MB"
    property string machine: "Unknown"
    property string material: "Unknown"
    property string uploadTime: "-"
    property string printTime: "-"
    property string layerThickness: "-"
    property int layers: 0
    property bool isPwmb: false
    property string resinUsage: "-"
    property string dimensions: "-"

    signal deleteRequested(string fileId)
    signal downloadRequested(string fileId)

    implicitHeight: 220

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: Theme.cardAlt
        border.width: 1
        border.color: Theme.panelStroke

        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 14

            Rectangle {
                Layout.preferredWidth: 100
                Layout.preferredHeight: 100
                radius: 10
                border.width: 1
                border.color: Qt.darker(Theme.thumbEnd, 1.15)
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.thumbStart }
                    GradientStop { position: 1.0; color: Theme.thumbEnd }
                }

                Text {
                    anchors.centerIn: parent
                    text: "100x100"
                    color: Theme.accentFg
                    font.bold: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        objectName: "fileNameLabel"
                        Layout.fillWidth: true
                        text: root.fileName
                        color: Theme.textPrimary
                        font.pixelSize: 18
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    AppButton {
                        objectName: "deleteButton"
                        text: "Delete"
                        variant: "danger"
                        onClicked: root.deleteRequested(root.fileId)
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 18
                    rowSpacing: 4

                    Text { text: "Layers: " + root.layers; color: Theme.textSecondary }
                    Text { text: "Print time: " + root.printTime; color: Theme.textSecondary }
                    Text { text: "Upload time: " + root.uploadTime; color: Theme.textSecondary }
                    Text { text: "Layer thickness: " + root.layerThickness; color: Theme.textSecondary }
                    Text { text: "Machine: " + root.machine; color: Theme.textSecondary }
                    Text { text: "Material: " + root.material; color: Theme.textSecondary }
                    Text { text: "Resin usage: " + root.resinUsage; color: Theme.textSecondary }
                    Text { text: "Size XYZ: " + root.dimensions; color: Theme.textSecondary }
                }

                Text {
                    text: "Size " + root.sizeText + "  |  ID " + root.fileId + "  |  Status " + root.status
                    color: Theme.textPrimary
                    font.pixelSize: 13
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    AppButton {
                        text: "Details"
                    }
                    AppButton {
                        text: "Print"
                    }
                    AppButton {
                        text: "Download"
                        onClicked: root.downloadRequested(root.fileId)
                    }

                    AppButton {
                        id: openViewerButton
                        objectName: "openViewerButton"
                        text: "Open 3D Viewer"
                        visible: root.isPwmb
                        enabled: root.isPwmb
                        variant: "primary"
                    }
                }
            }
        }
    }
}
