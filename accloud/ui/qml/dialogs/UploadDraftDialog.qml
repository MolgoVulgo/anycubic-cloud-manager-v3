import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Dialog {
    id: root
    title: qsTr("Upload .pwmb (Draft)")
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0
    readonly property real overlayWidth: (Overlay.overlay && Overlay.overlay.width > 0) ? Overlay.overlay.width : 1480
    readonly property real overlayHeight: (Overlay.overlay && Overlay.overlay.height > 0) ? Overlay.overlay.height : 920
    width: Math.min(1040, Math.max(820, overlayWidth * 0.78))
    height: Math.min(680, Math.max(520, overlayHeight * 0.78))

    background: Rectangle {
        radius: 14
        color: Theme.card
        border.width: 1
        border.color: Theme.panelStroke
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: root.title
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSectionPx
                font.bold: true
                elide: Text.ElideRight
            }

            AppButton {
                text: qsTr("X")
                compact: true
                variant: "secondary"
                onClicked: root.close()
            }
        }

        Text {
            text: qsTr("Draft workflow: upload + optional print actions")
            color: Theme.textSecondary
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: qsTr("File"); Layout.preferredWidth: 92 }
            AppTextField { Layout.fillWidth: true; placeholderText: qsTr("/path/model.pwmb") }
            AppButton { text: qsTr("Browse") }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: qsTr("Upload profile"); Layout.preferredWidth: 92 }
            AppComboBox { Layout.fillWidth: true; model: [qsTr("Default"), qsTr("High reliability"), qsTr("Fast")] }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: qsTr("Print target"); Layout.preferredWidth: 92 }
            AppComboBox { Layout.fillWidth: true; model: [qsTr("None"), "M7-Workshop-A", "M5S-Line-2"] }
        }

        AppCheckBox { text: qsTr("Print after upload") }
        AppCheckBox { text: qsTr("Delete after print") }
        AppCheckBox { text: qsTr("Keep signed URL snapshot") }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("Close"); onClicked: root.close() }
            AppButton {
                text: qsTr("Start upload")
                variant: "primary"
            }
        }
    }
}
