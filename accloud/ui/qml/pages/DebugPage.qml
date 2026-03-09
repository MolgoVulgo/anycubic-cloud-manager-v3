import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Item {
    anchors.fill: parent

    AppPageFrame {
        anchors.fill: parent
        sectionTitle: qsTr("Debug")
        sectionSubtitle: qsTr("Diagnostic actions and log preview")

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.gapRow

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gapRow

                AppButton { text: qsTr("Open app log") }
                AppButton { text: qsTr("Open http log") }
                AppButton { text: qsTr("Open render3d log") }
                Item { Layout.fillWidth: true }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusControl
                color: Theme.bgWindow
                border.width: Theme.borderWidth
                border.color: Theme.borderSubtle

                ScrollView {
                    id: debugLogScroll
                    anchors.fill: parent
                    anchors.margins: 10
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        active: true
                    }
                    ScrollBar.horizontal: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        active: true
                    }

                    TextArea {
                        width: Math.max(debugLogScroll.availableWidth, implicitWidth)
                        height: Math.max(debugLogScroll.availableHeight, implicitHeight)
                        readOnly: true
                        placeholderText: qsTr("Structured logs preview")
                        color: Theme.mono
                        font.family: "JetBrains Mono"
                        background: null
                    }
                }
            }
        }
    }
}
