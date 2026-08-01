import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

AppDialogFrame {
    id: root
    objectName: "cloudFileDetailsDialog"

    property var fileData: ({})
    property bool buildDebugEnabled: false
    property bool showAdvancedDetails: false
    property var fileTypeLabelProvider: null
    property var fileNameWithoutExtensionProvider: null
    property var displayDateProvider: null

    signal deleteRequested(string fileId, string fileName)
    signal downloadRequested(string fileId, string fileName)
    signal printRequested(string fileId, string fileName)
    signal closeRequested()

    function providerText(provider, arg, fallback) {
        return typeof provider === "function" ? String(provider(arg)) : fallback
    }

    function valueText(value) {
        if (value === undefined || value === null)
            return "-"
        var text = String(value).trim()
        return text.length > 0 ? text : "-"
    }

    function statusText() {
        var status = String(fileData.status || "").trim().toUpperCase()
        var code = Number(fileData.statusCode)
        if (code === 1 || status === "READY")
            return qsTr("Ready")
        if (code === 2 || status === "PROCESSING")
            return qsTr("Processing")
        if (status === "FAILED" || status === "ERROR")
            return qsTr("Error")
        return qsTr("Unknown")
    }

    function thumbnailSource() {
        var source = valueText(fileData.thumbnailUrl)
        return source === "-" ? "" : source
    }

    function summaryText() {
        return qsTr("%1 • %2 • Uploaded on %3 • %4")
                .arg(providerText(fileTypeLabelProvider, fileData.fileName, "-"))
                .arg(valueText(fileData.sizeText))
                .arg(providerText(displayDateProvider, fileData.uploadTime, "-"))
                .arg(statusText())
    }

    onShowAdvancedDetailsChanged: {
        if (!showAdvancedDetails && detailsTabBar.currentIndex === 2)
            detailsTabBar.currentIndex = 0
    }

    onBuildDebugEnabledChanged: {
        if (!buildDebugEnabled && detailsTabBar.currentIndex === 3)
            detailsTabBar.currentIndex = 0
    }

    component DetailRow: RowLayout {
        id: detailRow
        property string labelText: ""
        property string valueText: "-"
        property int labelWidth: 132
        Layout.fillWidth: true
        Layout.fillHeight: false
        Layout.minimumHeight: implicitHeight
        Layout.preferredHeight: implicitHeight
        Layout.maximumHeight: implicitHeight
        Layout.alignment: Qt.AlignTop
        spacing: 8

        Text {
            Layout.preferredWidth: detailRow.labelWidth
            Layout.alignment: Qt.AlignTop
            text: detailRow.labelText
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontBodyPx
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            text: detailRow.valueText
            color: Theme.fgPrimary
            font.pixelSize: Theme.fontBodyPx
            wrapMode: Text.WrapAnywhere
            textFormat: Text.PlainText
        }
    }

    component SummaryField: RowLayout {
        id: summaryField
        property string labelText: ""
        property string valueText: "-"
        Layout.fillWidth: true
        Layout.fillHeight: false
        Layout.minimumHeight: implicitHeight
        Layout.preferredHeight: implicitHeight
        Layout.maximumHeight: implicitHeight
        Layout.alignment: Qt.AlignTop
        spacing: 8

        Text {
            Layout.preferredWidth: 108
            Layout.alignment: Qt.AlignTop
            text: summaryField.labelText
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontBodyPx
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            text: summaryField.valueText
            color: Theme.fgPrimary
            font.pixelSize: Theme.fontBodyPx
            wrapMode: Text.WrapAnywhere
            textFormat: Text.PlainText
        }
    }

    component DetailsCard: Rectangle {
        id: detailsCard
        property string heading: ""
        default property alias contentData: detailsBody.data
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 1
        implicitHeight: detailsColumn.implicitHeight + 28
        radius: Theme.radiusControl
        color: Theme.bgSurface
        border.width: Theme.borderWidth
        border.color: Theme.borderSubtle

        ColumnLayout {
            id: detailsColumn
            anchors.fill: parent
            anchors.margins: 14
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: detailsCard.heading
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontSectionPx
                font.bold: true
                wrapMode: Text.WordWrap
            }

            ColumnLayout {
                id: detailsBody
                objectName: "cloudFileDetailsCardBody"
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.minimumHeight: implicitHeight
                Layout.preferredHeight: implicitHeight
                Layout.maximumHeight: implicitHeight
                Layout.alignment: Qt.AlignTop
                spacing: 6
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }

    component DetailsPanel: Flickable {
        id: panel
        default property alias cardData: cardsRow.data
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: width
        contentHeight: Math.max(height,
                                cardsRow.implicitHeight + Theme.paddingDialog * 2)
        ScrollBar.vertical: ScrollBar {}

        RowLayout {
            id: cardsRow
            x: Theme.paddingDialog
            y: Theme.paddingDialog
            width: Math.max(0, panel.width - Theme.paddingDialog * 2)
            height: Math.max(implicitHeight,
                             panel.height - Theme.paddingDialog * 2)
            spacing: Theme.gapRow
        }
    }

    title: providerText(fileNameWithoutExtensionProvider, fileData.fileName, "-")
    subtitle: summaryText()
    dialogSize: "large"
    minimumWidth: 940
    maximumWidth: 1180
    minimumHeight: 700
    maximumHeight: 900

    RowLayout {
        id: summaryLayout
        objectName: "cloudFileDetailsSummary"
        Layout.fillWidth: true
        Layout.minimumHeight: 170
        Layout.preferredHeight: 170
        Layout.maximumHeight: 170
        spacing: Theme.gapSection

        Rectangle {
            id: thumbnailCard
            objectName: "cloudFileDetailsThumbnailCard"
            Layout.preferredWidth: 170
            Layout.minimumWidth: 170
            Layout.maximumWidth: 170
            Layout.preferredHeight: 170
            Layout.minimumHeight: 170
            Layout.maximumHeight: 170
            radius: Theme.radiusControl
            color: Theme.bgCardSubtle
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault
            clip: true

            Image {
                id: detailsThumbnail
                objectName: "cloudFileDetailsThumbnail"
                anchors.fill: parent
                anchors.margins: 8
                source: root.thumbnailSource()
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: true
            }

            Column {
                objectName: "cloudFileDetailsThumbnailFallback"
                anchors.centerIn: parent
                spacing: 6
                visible: detailsThumbnail.status !== Image.Ready

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.providerText(root.fileTypeLabelProvider,
                                            root.fileData.fileName,
                                            "-")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontTitlePx
                    font.bold: true
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Preview unavailable")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontCaptionPx
                }
            }
        }

        DetailsCard {
            objectName: "cloudFileDetailsCompactSummaryCard"
            heading: qsTr("Information")
            Layout.fillWidth: true
            Layout.fillHeight: true

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 20
                rowSpacing: 7

                SummaryField {
                    objectName: "cloudFileDetailsSummaryFileNameField"
                    labelText: qsTr("File name")
                    valueText: root.valueText(root.fileData.fileName)
                }
                SummaryField {
                    labelText: qsTr("Machine")
                    valueText: root.valueText(root.fileData.machine)
                }
                SummaryField {
                    labelText: qsTr("Status")
                    valueText: root.statusText()
                }
                SummaryField {
                    labelText: qsTr("Material")
                    valueText: root.valueText(root.fileData.material)
                }
                SummaryField {
                    labelText: qsTr("Type / size")
                    valueText: qsTr("%1 • %2")
                            .arg(root.providerText(root.fileTypeLabelProvider,
                                                   root.fileData.fileName,
                                                   "-"))
                            .arg(root.valueText(root.fileData.sizeText))
                }
                SummaryField {
                    labelText: qsTr("Print time")
                    valueText: root.valueText(root.fileData.printTime)
                }
                SummaryField {
                    labelText: qsTr("Uploaded")
                    valueText: root.providerText(root.displayDateProvider,
                                                 root.fileData.uploadTime,
                                                 "-")
                }
                SummaryField {
                    labelText: qsTr("Layers")
                    valueText: root.valueText(root.fileData.layers)
                }
            }
        }
    }

    Rectangle {
        id: detailsTabsContainer
        objectName: "cloudFileDetailsTabsContainer"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 300
        Layout.preferredHeight: 320
        radius: Theme.radiusControl
        color: Theme.bgDialog
        border.width: Theme.borderWidth
        border.color: Theme.borderDefault
        clip: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            AppTabBar {
                id: detailsTabBar
                objectName: "cloudFileDetailsTabs"
                Layout.fillWidth: true
                tabVariant: "local"
                tabLook: "classic"
                tabSizingMode: "content"
                connectActiveToPanel: true
                panelColor: Theme.bgDialog
                stripColor: Theme.bgDialog
                tabTopCornerRadius: detailsTabsContainer.radius

                AppTabButton {
                    objectName: "cloudFileDetailsOverviewTab"
                    text: qsTr("Information")
                }
                AppTabButton {
                    objectName: "cloudFileDetailsPrintSettingsTab"
                    text: qsTr("Print Settings")
                }
                AppTabButton {
                    objectName: "cloudFileDetailsTechnicalTab"
                    text: qsTr("Technical Details")
                    visible: root.showAdvancedDetails
                }
                AppTabButton {
                    objectName: "cloudFileDetailsCloudMetadataTab"
                    text: qsTr("Cloud Metadata")
                    visible: root.buildDebugEnabled
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: detailsTabBar.currentIndex

                DetailsPanel {
                    id: overviewPanel
                    objectName: "cloudFileDetailsOverviewPanel"

                    DetailsCard {
                        objectName: "cloudFileDetailsOverviewFileCard"
                        heading: qsTr("File")

                        DetailRow {
                            objectName: "cloudFileDetailsOverviewFileNameRow"
                            labelText: qsTr("File name")
                            valueText: root.valueText(root.fileData.fileName)
                        }
                        DetailRow {
                            objectName: "cloudFileDetailsOverviewTypeRow"
                            labelText: qsTr("Type")
                            valueText: root.providerText(root.fileTypeLabelProvider,
                                                         root.fileData.fileName,
                                                         "-")
                        }
                        DetailRow {
                            labelText: qsTr("Size")
                            valueText: root.valueText(root.fileData.sizeText)
                        }
                        DetailRow {
                            objectName: "cloudFileDetailsOverviewUploadedRow"
                            labelText: qsTr("Uploaded")
                            valueText: root.providerText(root.displayDateProvider,
                                                         root.fileData.uploadTime,
                                                         "-")
                        }
                    }

                    DetailsCard {
                        objectName: "cloudFileDetailsOverviewCompatibilityCard"
                        heading: qsTr("Compatibility")

                        DetailRow {
                            labelText: qsTr("Status")
                            valueText: root.statusText()
                        }
                        DetailRow {
                            labelText: qsTr("Machine")
                            valueText: root.valueText(root.fileData.machine)
                        }
                        DetailRow {
                            labelText: qsTr("Material")
                            valueText: root.valueText(root.fileData.material)
                        }
                        DetailRow {
                            labelText: qsTr("Printers")
                            valueText: root.valueText(root.fileData.printers)
                        }
                    }
                }

                DetailsPanel {
                    id: printSettingsPanel
                    objectName: "cloudFileDetailsPrintSettingsPanel"

                    DetailsCard {
                        objectName: "cloudFileDetailsLayerProfileCard"
                        heading: qsTr("Layer profile")

                        DetailRow {
                            labelText: qsTr("Layer thickness")
                            valueText: root.valueText(root.fileData.layerThickness)
                        }
                        DetailRow {
                            labelText: qsTr("Layers")
                            valueText: root.valueText(root.fileData.layers)
                        }
                        DetailRow {
                            labelText: qsTr("Bottom layers")
                            valueText: root.valueText(root.fileData.bottomLayers)
                        }
                        DetailRow {
                            labelText: qsTr("Dimensions")
                            valueText: root.valueText(root.fileData.dimensions)
                        }
                    }

                    DetailsCard {
                        objectName: "cloudFileDetailsExposureMaterialCard"
                        heading: qsTr("Exposure and material")

                        DetailRow {
                            labelText: qsTr("Exposure time")
                            valueText: root.valueText(root.fileData.exposureTime)
                        }
                        DetailRow {
                            labelText: qsTr("Off time")
                            valueText: root.valueText(root.fileData.offTime)
                        }
                        DetailRow {
                            labelText: qsTr("Resin usage")
                            valueText: root.valueText(root.fileData.resinUsage)
                        }
                        DetailRow {
                            labelText: qsTr("Print time")
                            valueText: root.valueText(root.fileData.printTime)
                        }
                    }
                }

                DetailsPanel {
                    id: technicalPanel
                    objectName: "cloudFileDetailsTechnicalPanel"
                    visible: root.showAdvancedDetails

                    DetailsCard {
                        objectName: "cloudFileDetailsTechnicalIdentityCard"
                        heading: qsTr("Cloud identity")

                        DetailRow {
                            labelText: qsTr("File ID")
                            valueText: root.valueText(root.fileData.fileId)
                        }
                        DetailRow {
                            labelText: qsTr("G-code ID")
                            valueText: root.valueText(root.fileData.gcodeId)
                        }
                        DetailRow {
                            labelText: qsTr("Status code")
                            valueText: root.valueText(root.fileData.statusCode)
                        }
                        DetailRow {
                            labelText: qsTr("Slice MD5")
                            valueText: root.valueText(root.fileData.md5)
                        }
                    }

                    DetailsCard {
                        objectName: "cloudFileDetailsTechnicalObjectCard"
                        heading: qsTr("Cloud object")

                        DetailRow {
                            labelText: qsTr("Created")
                            valueText: root.providerText(root.displayDateProvider,
                                                         root.fileData.createTime,
                                                         "-")
                        }
                        DetailRow {
                            labelText: qsTr("Updated")
                            valueText: root.providerText(root.displayDateProvider,
                                                         root.fileData.updateTime,
                                                         "-")
                        }
                        DetailRow {
                            labelText: qsTr("Region")
                            valueText: root.valueText(root.fileData.region)
                        }
                    }
                }

                DetailsPanel {
                    id: cloudMetadataPanel
                    objectName: "cloudFileDetailsCloudMetadataPanel"
                    visible: root.buildDebugEnabled

                    DetailsCard {
                        objectName: "cloudFileDetailsCloudIdentityCard"
                        heading: qsTr("Cloud identity")

                        DetailRow {
                            labelText: qsTr("File ID")
                            valueText: root.valueText(root.fileData.fileId)
                        }
                        DetailRow {
                            labelText: qsTr("G-code ID")
                            valueText: root.valueText(root.fileData.gcodeId)
                        }
                        DetailRow {
                            labelText: qsTr("Status code")
                            valueText: root.valueText(root.fileData.statusCode)
                        }
                        DetailRow {
                            labelText: qsTr("Region")
                            valueText: root.valueText(root.fileData.region)
                        }
                    }

                    DetailsCard {
                        objectName: "cloudFileDetailsCloudObjectCard"
                        heading: qsTr("Cloud object")

                        DetailRow {
                            labelText: qsTr("Uploaded")
                            valueText: root.providerText(root.displayDateProvider,
                                                         root.fileData.uploadTime,
                                                         "-")
                        }
                        DetailRow {
                            labelText: qsTr("Created")
                            valueText: root.providerText(root.displayDateProvider,
                                                         root.fileData.createTime,
                                                         "-")
                        }
                        DetailRow {
                            labelText: qsTr("Updated")
                            valueText: root.providerText(root.displayDateProvider,
                                                         root.fileData.updateTime,
                                                         "-")
                        }
                        DetailRow {
                            labelText: qsTr("Bucket")
                            valueText: root.valueText(root.fileData.bucket)
                        }
                        DetailRow {
                            labelText: qsTr("Path")
                            valueText: root.valueText(root.fileData.path)
                        }
                        DetailRow {
                            labelText: qsTr("Slice MD5")
                            valueText: root.valueText(root.fileData.md5)
                        }
                    }
                }
            }
        }
    }

    footerLeadingData: [
        AppButton {
            objectName: "cloudFileDetailsDeleteButton"
            text: qsTr("Delete")
            variant: "danger"
            Layout.minimumWidth: 112
            onClicked: root.deleteRequested(String(root.fileData.fileId || ""),
                                            String(root.fileData.fileName || ""))
        }
    ]

    footerTrailingData: [
        AppButton {
            objectName: "cloudFileDetailsCloseButton"
            text: qsTr("Close")
            variant: "secondary"
            Layout.minimumWidth: 112
            onClicked: root.closeRequested()
        },
        AppButton {
            objectName: "cloudFileDetailsDownloadButton"
            text: qsTr("Download")
            variant: "secondary"
            Layout.minimumWidth: 112
            onClicked: root.downloadRequested(String(root.fileData.fileId || ""),
                                              String(root.fileData.fileName || ""))
        },
        AppButton {
            objectName: "cloudFileDetailsPrintButton"
            text: qsTr("Print")
            variant: "primary"
            Layout.minimumWidth: 112
            enabled: String(root.fileData.fileId || "").length > 0
            ToolTip.visible: hovered && enabled
            ToolTip.delay: 350
            ToolTip.text: qsTr("Remote print via Printers workflow")
            onClicked: root.printRequested(String(root.fileData.fileId || ""),
                                           String(root.fileData.fileName || ""))
        }
    ]
}
