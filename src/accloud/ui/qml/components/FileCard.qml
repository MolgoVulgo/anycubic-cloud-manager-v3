import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Item {
    id: root
    objectName: "fileCard"
    property string fileName: qsTr("Untitled.pwmb")
    property string fileId: "file-unknown"
    property string status: "READY"
    property string sizeText: qsTr("0 MB")
    property string machine: qsTr("Unknown")
    property string material: qsTr("Unknown")
    property string uploadTime: "-"
    property string printTime: "-"
    property string layerThickness: "-"
    property int layers: 0
    property bool isPwmb: false
    property string resinUsage: "-"
    property string dimensions: "-"
    property string thumbnailUrl: ""

    signal deleteRequested(string fileId)
    signal downloadRequested(string fileId)

    implicitHeight: 220

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusDialog
        color: Theme.bgDialog
        border.width: Theme.borderWidth
        border.color: Theme.borderDefault

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
                clip: true

                Image {
                    id: cardThumbnailImage
                    anchors.fill: parent
                    source: String(root.thumbnailUrl || "")
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    visible: source.length > 0 && status === Image.Ready
                }

                Text {
                    anchors.centerIn: parent
                    visible: !cardThumbnailImage.visible
                    text: qsTr("100x100")
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
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontTitlePx
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    AppButton {
                        objectName: "deleteButton"
                        text: qsTr("Delete")
                        variant: "danger"
                        onClicked: root.deleteRequested(root.fileId)
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 18
                    rowSpacing: 4

                    Text { text: qsTr("Layers: ") + root.layers; color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: qsTr("Print time: ") + root.printTime; color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: qsTr("Upload time: ") + root.uploadTime; color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: qsTr("Layer thickness: ") + root.layerThickness; color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: qsTr("Machine: ") + root.machine; color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: qsTr("Material: ") + root.material; color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: qsTr("Resin usage: ") + root.resinUsage; color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: qsTr("Size XYZ: ") + root.dimensions; color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                }

                Text {
                    text: qsTr("Size %1 | ID %2 | Status %3").arg(root.sizeText).arg(root.fileId).arg(root.status)
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    AppButton {
                        text: qsTr("Details")
                    }
                    AppButton {
                        text: qsTr("Print")
                    }
                    AppButton {
                        text: qsTr("Download")
                        onClicked: root.downloadRequested(root.fileId)
                    }

                    AppButton {
                        id: openViewerButton
                        objectName: "openViewerButton"
                        text: qsTr("Open 3D Viewer")
                        visible: root.isPwmb
                        enabled: root.isPwmb
                        variant: "primary"
                    }
                }
            }
        }
    }
}
