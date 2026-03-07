import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Dialog {
    id: root

    property string subtitle: qsTr("")
    property bool allowScrimClose: true
    property bool showCloseButton: true
    property bool showHeaderDivider: true
    property bool showFooterDivider: true
    property int minimumWidth: 520
    property int maximumWidth: 980
    readonly property real overlayWidth: (Overlay.overlay && Overlay.overlay.width > 0) ? Overlay.overlay.width : 1280
    readonly property real overlayHeight: (Overlay.overlay && Overlay.overlay.height > 0) ? Overlay.overlay.height : 860
    default property alias bodyData: bodyColumn.data
    property alias headerActionsData: headerActions.data
    property alias footerLeadingData: footerLeading.data
    property alias footerTrailingData: footerTrailing.data

    modal: true
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    closePolicy: allowScrimClose
                 ? (Popup.CloseOnEscape | Popup.CloseOnPressOutside)
                 : Popup.CloseOnEscape
    padding: 0

    width: Math.min(maximumWidth, Math.max(minimumWidth, overlayWidth * 0.68))
    height: Math.min(overlayHeight * 0.9, contentLayout.implicitHeight)

    Overlay.modal: Rectangle {
        color: Theme.overlayScrim
        opacity: 1
    }

    background: Rectangle {
        radius: Theme.radiusDialog
        color: Theme.bgDialog
        border.width: Theme.borderWidth
        border.color: Theme.borderDefault
    }

    contentItem: ColumnLayout {
        id: contentLayout
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: headerLayout.implicitHeight + Theme.paddingDialog * 2

            RowLayout {
                id: headerLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingDialog
                spacing: Theme.gapRow

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: root.title
                        visible: text.length > 0
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontTitlePx
                        font.bold: true
                    }

                    Text {
                        text: root.subtitle
                        visible: text.length > 0
                        color: Theme.fgSecondary
                        opacity: 0.9
                        font.pixelSize: Theme.fontCaptionPx
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                RowLayout {
                    id: headerActions
                    spacing: 8
                }

                AppButton {
                    visible: root.showCloseButton
                    text: qsTr("Close")
                    variant: "secondary"
                    compact: true
                    onClicked: root.close()
                }
            }
        }

        Rectangle {
            visible: root.showHeaderDivider
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderSubtle
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: bodyColumn.implicitHeight + Theme.paddingDialog * 2

            ColumnLayout {
                id: bodyColumn
                anchors.fill: parent
                anchors.margins: Theme.paddingDialog
                spacing: Theme.gapSection
            }
        }

        Rectangle {
            visible: root.showFooterDivider && (footerLeading.children.length > 0 || footerTrailing.children.length > 0)
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderSubtle
        }

        Item {
            visible: footerLeading.children.length > 0 || footerTrailing.children.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: footerRow.implicitHeight + Theme.paddingDialog * 2

            RowLayout {
                id: footerRow
                anchors.fill: parent
                anchors.margins: Theme.paddingDialog
                spacing: Theme.gapRow

                RowLayout {
                    id: footerLeading
                    spacing: 8
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    id: footerTrailing
                    spacing: 8
                }
            }
        }
    }
}
