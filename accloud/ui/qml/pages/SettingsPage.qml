import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Item {
    anchors.fill: parent

    AppPageFrame {
        anchors.fill: parent
        sectionTitle: "Settings"
        sectionSubtitle: "Local UI settings"

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.gapRow

            AppCheckBox {
                text: "Enable debug logs"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gapRow
                Text {
                    text: "Render stride"
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontBodyPx
                    Layout.preferredWidth: 140
                }
                AppSpinBox {
                    from: 1
                    to: 8
                    value: 1
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gapRow
                Text {
                    text: "Render LOD"
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontBodyPx
                    Layout.preferredWidth: 140
                }
                AppSpinBox {
                    from: 0
                    to: 5
                    value: 0
                }
                Item { Layout.fillWidth: true }
            }
        }
    }
}
