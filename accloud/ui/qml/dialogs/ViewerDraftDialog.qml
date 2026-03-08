import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Dialog {
    id: root
    title: qsTr("PWMB 3D Viewer")
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0
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
            text: qsTr("Build progressif contours/fill avec controle camera.")
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
                    anchors.margins: 15
                    spacing: 8

                    AppTextField { placeholderText: qsTr("Source PWMB path"); Layout.fillWidth: true }
                    AppSlider { from: 0; to: 100; value: 100; Layout.fillWidth: true }
                    AppComboBox { Layout.fillWidth: true; model: [qsTr("Quality 33"), qsTr("Quality 66"), qsTr("Quality 100")] }
                    AppComboBox { Layout.fillWidth: true; model: [qsTr("Palette Steel"), qsTr("Palette Resin"), qsTr("Palette Heat")] }
                    AppCheckBox { text: qsTr("Contour only") }
                    ProgressBar { from: 0; to: 100; value: 46; Layout.fillWidth: true }
                    Item { Layout.fillHeight: true }
                    AppButton { text: qsTr("Rebuild"); Layout.fillWidth: true; variant: "primary" }
                    AppButton { text: qsTr("Cancel"); Layout.fillWidth: true; variant: "danger" }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 12
                color: Theme.viewportBg
                border.width: 1
                border.color: Theme.viewportBorder

                Text {
                    anchors.centerIn: parent
                    text: qsTr("OpenGL viewport placeholder")
                    color: Theme.viewportFg
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            AppButton { text: qsTr("Retry") }
            AppButton { text: qsTr("Reset camera") }
            AppButton { text: qsTr("Export screenshot") }
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("Close"); onClicked: root.close() }
        }
    }
}
