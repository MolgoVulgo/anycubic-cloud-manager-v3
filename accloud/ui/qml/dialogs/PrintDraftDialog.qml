import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme

Dialog {
    id: root
    title: "Send Print Order (Draft)"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    readonly property real overlayWidth: (Overlay.overlay && Overlay.overlay.width > 0) ? Overlay.overlay.width : 1480
    readonly property real overlayHeight: (Overlay.overlay && Overlay.overlay.height > 0) ? Overlay.overlay.height : 920
    width: Math.min(1040, Math.max(820, overlayWidth * 0.78))
    height: Math.min(640, Math.max(500, overlayHeight * 0.74))

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
            text: "Draft direct print order dialog"
            color: Theme.textSecondary
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "Printer"; Layout.preferredWidth: 92 }
            ComboBox { Layout.fillWidth: true; model: ["M7-Workshop-A", "M5S-Line-2", "Backup-X2"] }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "file_id"; Layout.preferredWidth: 92 }
            TextField { Layout.fillWidth: true; placeholderText: "f-2d8e91" }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "Copies"; Layout.preferredWidth: 92 }
            SpinBox { from: 1; to: 10; value: 1 }
            Text { text: "Priority" }
            ComboBox { model: ["normal", "high"] }
            CheckBox { text: "Dry-run" }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { text: "Preview payload" }
            Button { text: "Close"; onClicked: root.close() }
            Button {
                text: "Send order"
                font.bold: true
                background: Rectangle {
                    radius: 8
                    color: parent.down ? Qt.darker(Theme.accent, 1.12) : Theme.accent
                }
                contentItem: Text {
                    text: parent.text
                    color: "#f8fffe"
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
