import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

AppDialogFrame {
    id: root
    title: qsTr("PWMB 3D Viewer")
    subtitle: qsTr("Build progressif contours/fill avec controle camera.")
    minimumWidth: 980
    maximumWidth: 1320
    minimumHeight: 640
    maximumHeight: 860
    dialogSize: "workspace"

    bodyData: [
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.gapRow

            Rectangle {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                radius: Theme.radiusDialog
                color: Theme.bgSurface
                border.width: Theme.borderWidth
                border.color: Theme.borderDefault

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.paddingDialog
                    spacing: Theme.gapRow

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
                radius: Theme.radiusDialog
                color: Theme.viewportBg
                border.width: Theme.borderWidth
                border.color: Theme.viewportBorder

                Text {
                    anchors.centerIn: parent
                    text: qsTr("OpenGL viewport placeholder")
                    color: Theme.viewportFg
                }
            }
        }
    ]

    footerLeadingData: [
        AppButton { text: qsTr("Retry") },
        AppButton { text: qsTr("Reset camera") },
        AppButton { text: qsTr("Export screenshot") }
    ]

    footerTrailingData: [
        AppButton { text: qsTr("Close"); onClicked: root.close() }
    ]
}
