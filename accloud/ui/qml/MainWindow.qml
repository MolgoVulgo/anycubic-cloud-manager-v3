import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "components/Theme.js" as Theme
import "pages" as Pages
import "dialogs" as Dialogs

ApplicationWindow {
    id: root
    objectName: "controlRoomWindow"
    width: 1480
    height: 920
    visible: true
    title: "Anycubic Cloud Control Room"
    property string statusText: "Vérification de la session en cours…"

    Component.onCompleted: {
        Qt.callLater(function() {
            if (typeof sessionImportBridge === "undefined"
                    || sessionImportBridge === null
                    || typeof sessionImportBridge.checkStartup !== "function") {
                root.statusText = "Mode interface: backend indisponible."
                return
            }

            var check = sessionImportBridge.checkStartup()
            if (check.sessionExists === true && check.connectionOk === true) {
                root.statusText = "Session active. Auto-refresh toutes les 30s."
            } else {
                root.statusText = String(check.message)
                sessionDialog.startupMessage = String(check.message)
                sessionDialog.mandatoryMode = true
                sessionDialog.open()
            }
        })
    }

    component HeaderActionButton: Button {
        property color baseColor: Theme.panel
        property color borderColor: Theme.panelStroke
        property color textColor: Theme.textPrimary

        font.pixelSize: 14
        font.bold: true
        padding: 12

        background: Rectangle {
            radius: 10
            color: parent.down ? Qt.darker(parent.baseColor, 1.08) : (parent.hovered ? Qt.lighter(parent.baseColor, 1.03) : parent.baseColor)
            border.width: 1
            border.color: parent.borderColor
        }

        contentItem: Text {
            text: parent.text
            color: parent.textColor
            font: parent.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.rootGradientStart }
            GradientStop { position: 1.0; color: Theme.rootGradientEnd }
        }
    }

    Dialogs.SessionSettingsDialog {
        id: sessionDialog
        objectName: "sessionSettingsDialog"
        onImportCompleted: function(message) {
            root.statusText = "Session active. " + message
            sessionDialog.mandatoryMode = false
            sessionDialog.close()
        }
    }

    Dialogs.UploadDraftDialog {
        id: uploadDialog
        objectName: "uploadDraftDialog"
    }

    Dialogs.PrintDraftDialog {
        id: printDialog
        objectName: "printDraftDialog"
    }

    Dialogs.ViewerDraftDialog {
        id: viewerDialog
        objectName: "viewerDraftDialog"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Rectangle {
            id: controlRoomHeader
            objectName: "controlRoomHeader"
            Layout.fillWidth: true
            Layout.preferredHeight: 118
            radius: 16
            color: Theme.panel
            border.width: 1
            border.color: Theme.panelStroke

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        id: titleLabel
                        objectName: "controlRoomTitle"
                        text: "Anycubic Cloud Control Room"
                        color: Theme.textPrimary
                        font.pixelSize: 30
                        font.bold: true
                    }

                    Text {
                        id: subtitleLabel
                        objectName: "controlRoomSubtitle"
                        text: root.statusText
                        color: Theme.textSecondary
                        font.pixelSize: 14
                    }
                }

                RowLayout {
                    spacing: 8

                    HeaderActionButton {
                        id: sessionSettingsButton
                        objectName: "sessionSettingsButton"
                        text: "Session Settings"
                        onClicked: sessionDialog.open()
                    }

                    HeaderActionButton {
                        id: printDialogButton
                        objectName: "printDialogButton"
                        text: "Print Dialog"
                        onClicked: printDialog.open()
                    }

                    HeaderActionButton {
                        id: viewerDialogButton
                        objectName: "viewerDialogButton"
                        text: "3D Viewer Dialog"
                        onClicked: viewerDialog.open()
                    }

                    HeaderActionButton {
                        id: uploadDialogButton
                        objectName: "uploadDialogButton"
                        text: "Upload Dialog"
                        baseColor: Theme.accent
                        borderColor: Theme.accentStrong
                        textColor: "#f8fffe"
                        onClicked: uploadDialog.open()
                    }
                }
            }
        }

        Rectangle {
            objectName: "tabsPanel"
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 16
            color: Theme.card
            border.width: 1
            border.color: Theme.panelStroke

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                TabBar {
                    id: controlTabs
                    objectName: "controlRoomTabs"
                    Layout.fillWidth: true

                    TabButton {
                        objectName: "filesTabButton"
                        text: "Files"
                    }

                    TabButton {
                        objectName: "printerTabButton"
                        text: "Printer"
                    }

                    TabButton {
                        objectName: "logTabButton"
                        text: "Log"
                    }
                }

                StackLayout {
                    objectName: "controlRoomStack"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: controlTabs.currentIndex

                    Pages.CloudFilesPage {
                        objectName: "cloudFilesPage"
                    }

                    Pages.PrinterPage {
                        objectName: "printerPage"
                    }

                    Pages.LogPage {
                        objectName: "logPage"
                    }
                }
            }
        }
    }
}
