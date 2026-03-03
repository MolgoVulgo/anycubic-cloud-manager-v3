import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme

Dialog {
    id: root
    title: "PWMB 3D Viewer"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    readonly property real overlayWidth: (Overlay.overlay && Overlay.overlay.width > 0) ? Overlay.overlay.width : 1480
    readonly property real overlayHeight: (Overlay.overlay && Overlay.overlay.height > 0) ? Overlay.overlay.height : 920
    width: Math.min(1320, Math.max(980, overlayWidth * 0.9))
    height: Math.min(860, Math.max(640, overlayHeight * 0.88))

    background: Rectangle {
        radius: 14
        color: Theme.card
        border.width: 1
        border.color: Theme.panelStroke
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Text {
            text: "Build progressif contours/fill avec controle camera."
            color: Theme.textSecondary
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                radius: 12
                color: Theme.panel
                border.width: 1
                border.color: Theme.panelStroke

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    TextField { placeholderText: "Source PWMB path"; Layout.fillWidth: true }
                    Slider { from: 0; to: 100; value: 100; Layout.fillWidth: true }
                    ComboBox { Layout.fillWidth: true; model: ["Quality 33", "Quality 66", "Quality 100"] }
                    ComboBox { Layout.fillWidth: true; model: ["Palette Steel", "Palette Resin", "Palette Heat"] }
                    CheckBox { text: "Contour only" }
                    ProgressBar { from: 0; to: 100; value: 46; Layout.fillWidth: true }
                    Item { Layout.fillHeight: true }
                    Button { text: "Rebuild"; Layout.fillWidth: true }
                    Button { text: "Cancel"; Layout.fillWidth: true }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 12
                color: "#1d1f23"
                border.width: 1
                border.color: "#3e4351"

                Text {
                    anchors.centerIn: parent
                    text: "OpenGL viewport placeholder"
                    color: "#d9dde6"
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button { text: "Retry" }
            Button { text: "Reset camera" }
            Button { text: "Export screenshot" }
            Item { Layout.fillWidth: true }
            Button { text: "Close"; onClicked: root.close() }
        }
    }
}
