import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Dialog {
    id: root

    property string subtitle: qsTr("")
    property bool allowScrimClose: true
    property bool allowEscapeClose: true
    property bool showCloseButton: true
    property bool showHeaderDivider: true
    property bool showFooterDivider: true
    property int framePadding: Theme.paddingDialog
    property var requestCloseCallback: null
    property string dialogSize: "medium" // "small" | "medium" | "large" | "workspace"
    property int minimumWidth: 520
    property int maximumWidth: 980
    property int minimumHeight: 0
    property int maximumHeight: 980
    readonly property color headerFooterBg: Qt.darker(Theme.bgDialog, 1.05)
    readonly property real overlayWidth: (Overlay.overlay && Overlay.overlay.width > 0) ? Overlay.overlay.width : 1280
    readonly property real overlayHeight: (Overlay.overlay && Overlay.overlay.height > 0) ? Overlay.overlay.height : 860
    readonly property real widthRatio: dialogSize === "small"
                                     ? 0.46
                                     : dialogSize === "large"
                                       ? 0.82
                                       : dialogSize === "workspace"
                                         ? 0.9
                                         : 0.68
    default property alias bodyData: bodyColumn.data
    property alias headerActionsData: headerActions.data
    property alias footerLeadingData: footerLeading.data
    property alias footerTrailingData: footerTrailing.data

    function requestClose() {
        if (requestCloseCallback !== null && typeof requestCloseCallback === "function") {
            requestCloseCallback()
            return
        }
        root.close()
    }

    modal: true
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    header: null
    closePolicy: allowScrimClose && allowEscapeClose
                 ? (Popup.CloseOnEscape | Popup.CloseOnPressOutside)
                 : allowScrimClose
                   ? Popup.CloseOnPressOutside
                   : allowEscapeClose
                     ? Popup.CloseOnEscape
                     : Popup.NoAutoClose
    padding: 0

    width: Math.min(maximumWidth, Math.max(minimumWidth, overlayWidth * widthRatio))
    height: Math.min(overlayHeight * 0.9,
                     Math.min(maximumHeight,
                              Math.max(minimumHeight, contentLayout.implicitHeight)))

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

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: headerLayout.implicitHeight + root.framePadding * 2
            color: root.headerFooterBg

            RowLayout {
                id: headerLayout
                anchors.fill: parent
                anchors.margins: root.framePadding
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
                    text: qsTr("X")
                    variant: "secondary"
                    compact: true
                    onClicked: root.requestClose()
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
            Layout.preferredHeight: bodyColumn.implicitHeight + root.framePadding * 2

            ColumnLayout {
                id: bodyColumn
                anchors.fill: parent
                anchors.margins: root.framePadding
                spacing: Theme.gapSection
            }
        }

        Rectangle {
            visible: root.showFooterDivider && (footerLeading.children.length > 0 || footerTrailing.children.length > 0)
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderSubtle
        }

        Rectangle {
            visible: footerLeading.children.length > 0 || footerTrailing.children.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: footerRow.implicitHeight + root.framePadding * 2
            color: root.headerFooterBg

            RowLayout {
                id: footerRow
                anchors.fill: parent
                anchors.margins: root.framePadding
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
