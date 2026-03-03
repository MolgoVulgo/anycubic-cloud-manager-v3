import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme

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
            TextField { Layout.fillWidth: true; placeholderText: "/path/model.pwmb" }
            Button { text: "Browse" }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "Upload profile"; Layout.preferredWidth: 92 }
            ComboBox { Layout.fillWidth: true; model: ["Default", "High reliability", "Fast"] }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "Print target"; Layout.preferredWidth: 92 }
            ComboBox { Layout.fillWidth: true; model: ["None", "M7-Workshop-A", "M5S-Line-2"] }
        }

        CheckBox { text: "Print after upload" }
        CheckBox { text: "Delete after print" }
        CheckBox { text: "Keep signed URL snapshot" }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { text: "Close"; onClicked: root.close() }
            Button {
                text: "Start upload"
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
