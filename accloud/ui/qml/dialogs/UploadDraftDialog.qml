import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Dialog {
    id: root
    title: "Upload .pwmb (Draft)"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
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
        anchors.margins: 14
        spacing: 10

        Text {
            text: "Draft workflow: upload + optional print actions"
            color: Theme.textSecondary
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "File"; Layout.preferredWidth: 92 }
            AppTextField { Layout.fillWidth: true; placeholderText: "/path/model.pwmb" }
            AppButton { text: "Browse" }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "Upload profile"; Layout.preferredWidth: 92 }
            AppComboBox { Layout.fillWidth: true; model: ["Default", "High reliability", "Fast"] }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "Print target"; Layout.preferredWidth: 92 }
            AppComboBox { Layout.fillWidth: true; model: ["None", "M7-Workshop-A", "M5S-Line-2"] }
        }

        AppCheckBox { text: "Print after upload" }
        AppCheckBox { text: "Delete after print" }
        AppCheckBox { text: "Keep signed URL snapshot" }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            AppButton { text: "Close"; onClicked: root.close() }
            AppButton {
                text: "Start upload"
                variant: "primary"
            }
        }
    }
}
