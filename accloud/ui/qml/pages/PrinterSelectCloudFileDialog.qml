import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

AppDialogFrame {
    id: root
    title: qsTr("Select Cloud File")
    subtitle: qsTr("Compatible files for the selected printer")
    minimumWidth: 820
    maximumWidth: 980

    property var filesModel: null
    property string selectedFileId: ""
    property var fileTypeProvider: null

    signal selectedFileChanged(string fileId)
    signal closeRequested()
    signal startRequested()

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 36
        color: Theme.bgSurface
        border.width: 0

        RowLayout {
            anchors.fill: parent
            spacing: 8

            Text { Layout.preferredWidth: 32; text: qsTr(""); color: Theme.fgSecondary }
            Text { Layout.fillWidth: true; text: qsTr("File name"); color: Theme.fgSecondary; font.pixelSize: Theme.fontCaptionPx }
            Text { Layout.preferredWidth: 80; text: qsTr("Type"); color: Theme.fgSecondary; font.pixelSize: Theme.fontCaptionPx }
            Text { Layout.preferredWidth: 90; text: qsTr("Size"); color: Theme.fgSecondary; font.pixelSize: Theme.fontCaptionPx }
            Text { Layout.preferredWidth: 86; text: qsTr("Status"); color: Theme.fgSecondary; font.pixelSize: Theme.fontCaptionPx }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Theme.borderSubtle
    }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: root.filesModel
        spacing: 0

        delegate: Rectangle {
            width: ListView.view.width
            height: 48
            color: root.selectedFileId === String(model.fileId) ? Theme.selectionBg : Theme.bgSurface
            border.width: 0

            RowLayout {
                anchors.fill: parent
                spacing: 8

                RadioButton {
                    Layout.preferredWidth: 32
                    checked: root.selectedFileId === String(model.fileId)
                    onClicked: root.selectedFileChanged(String(model.fileId))
                }

                Text {
                    Layout.fillWidth: true
                    text: String(model.fileName || "-")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                    elide: Text.ElideRight
                }

                Text {
                    Layout.preferredWidth: 80
                    text: typeof root.fileTypeProvider === "function"
                          ? String(root.fileTypeProvider(model.fileName)).toUpperCase()
                          : "-"
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    Layout.preferredWidth: 90
                    text: String(model.sizeText || "-")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                    horizontalAlignment: Text.AlignRight
                }

                Text {
                    Layout.preferredWidth: 86
                    text: String(model.status || "-")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontBodyPx
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        footer: Text {
            width: parent ? parent.width : 0
            visible: root.filesModel && root.filesModel.count === 0
            text: qsTr("No compatible cloud file for this printer.")
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontBodyPx
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            padding: 18
        }
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
            enabled: root.selectedFileId.length > 0
            onClicked: root.startRequested()
        }
    ]
}
