import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

AppDialogFrame {
    id: root

    property var fileData: ({})
    property bool buildDebugEnabled: false
    property var fileTypeLabelProvider: null
    property var fileNameWithoutExtensionProvider: null
    property var displayDateProvider: null

    signal renameRequested(string fileId, string fileName)
    signal deleteRequested(string fileId, string fileName)
    signal downloadRequested(string fileId, string fileName)
    signal printRequested(string fileId, string fileName)
    signal closeRequested()

    function providerText(provider, arg, fallback) {
        return typeof provider === "function" ? String(provider(arg)) : fallback
    }

    title: providerText(fileNameWithoutExtensionProvider, fileData.fileName, "-")
    subtitle: qsTr("ID: %1 | status_code: %2 | gcode_id: %3")
                  .arg(String(fileData.fileId || "-"))
                  .arg(String(fileData.statusCode || "-"))
                  .arg(String(fileData.gcodeId || "-"))
    minimumWidth: 860
    maximumWidth: 1020
    minimumHeight: 620
    maximumHeight: 920

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.gapRow

        Rectangle {
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
            radius: Theme.radiusControl
            color: Theme.accentSoft
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            Text {
                anchors.centerIn: parent
                text: root.providerText(root.fileTypeLabelProvider, root.fileData.fileName, "-")
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontCaptionPx
                font.bold: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: String(root.fileData.fileName || "-")
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontTitlePx
                font.bold: true
                elide: Text.ElideRight
            }
        }

        AppButton {
            text: qsTr("Rename")
            variant: "secondary"
            onClicked: root.renameRequested(String(root.fileData.fileId || ""),
                                            String(root.fileData.fileName || ""))
        }
    }

    AppTabBar {
        id: detailsTabBar
        Layout.fillWidth: true
        tabVariant: "local"
        tabLook: "classic"
        tabSizingMode: "content"
        connectActiveToPanel: true
        panelColor: Theme.bgDialog

        AppTabButton { text: qsTr("Basic Information") }
        AppTabButton { text: qsTr("Slice Settings") }
        AppTabButton { text: qsTr("Cloud Metadata"); visible: root.buildDebugEnabled }
    }

    StackLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: detailsTabBar.currentIndex

        Flickable {
            clip: true
            contentWidth: width
            contentHeight: basicInfoColumn.implicitHeight

            ColumnLayout {
                id: basicInfoColumn
                width: parent.width
                spacing: 8

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 20
                    rowSpacing: 8

                    Text { text: qsTr("File name"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text {
                        text: String(root.fileData.fileName || "-")
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontBodyPx
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text { text: qsTr("Type"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: root.providerText(root.fileTypeLabelProvider, root.fileData.fileName, "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Size"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.sizeText || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Date"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: root.providerText(root.displayDateProvider, root.fileData.uploadTime, "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("status_code"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.statusCode || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("gcode_id"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.gcodeId || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }
                }
            }
        }

        Flickable {
            clip: true
            contentWidth: width
            contentHeight: sliceColumn.implicitHeight

            ColumnLayout {
                id: sliceColumn
                width: parent.width
                spacing: 8

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 20
                    rowSpacing: 8

                    Text { text: qsTr("Machine"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.machine || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Material"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.material || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Print time"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.printTime || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Layer thickness"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.layerThickness || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Layers"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.layers || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Resin usage"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.resinUsage || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Dimensions"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.dimensions || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Bottom layers"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.bottomLayers || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Exposure time"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.exposureTime || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Off time"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.offTime || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Printers"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text {
                        text: String(root.fileData.printers || "-")
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontBodyPx
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Text { text: qsTr("Slice md5"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text {
                        text: String(root.fileData.md5 || "-")
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontBodyPx
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Flickable {
            visible: root.buildDebugEnabled
            clip: true
            contentWidth: width
            contentHeight: cloudColumn.implicitHeight

            ColumnLayout {
                id: cloudColumn
                width: parent.width
                spacing: 8

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 20
                    rowSpacing: 8

                    Text { text: qsTr("Uploaded"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: root.providerText(root.displayDateProvider, root.fileData.uploadTime, "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Created"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: root.providerText(root.displayDateProvider, root.fileData.createTime, "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Updated"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: root.providerText(root.displayDateProvider, root.fileData.updateTime, "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Thumbnail URL"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text {
                        text: String(root.fileData.thumbnailUrl || "-")
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontBodyPx
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }

                    Text { text: qsTr("Download URL"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text {
                        text: String(root.fileData.downloadUrl || "-")
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontBodyPx
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }

                    Text { text: qsTr("Region"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.region || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Bucket"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text { text: String(root.fileData.bucket || "-"); color: Theme.fgPrimary; font.pixelSize: Theme.fontBodyPx }

                    Text { text: qsTr("Path"); color: Theme.fgSecondary; font.pixelSize: Theme.fontBodyPx }
                    Text {
                        text: String(root.fileData.path || "-")
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontBodyPx
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }

    footerLeadingData: [
        AppButton {
            text: qsTr("Delete")
            variant: "danger"
            onClicked: root.deleteRequested(String(root.fileData.fileId || ""),
                                            String(root.fileData.fileName || ""))
        }
    ]

    footerTrailingData: [
        AppButton {
            text: qsTr("Download")
            variant: "secondary"
            onClicked: root.downloadRequested(String(root.fileData.fileId || ""),
                                              String(root.fileData.fileName || ""))
        },
        AppButton {
            text: qsTr("Print")
            variant: "primary"
            onClicked: root.printRequested(String(root.fileData.fileId || ""),
                                           String(root.fileData.fileName || ""))
        },
        AppButton {
            text: qsTr("Close")
            variant: "secondary"
            onClicked: root.closeRequested()
        }
    ]
}
