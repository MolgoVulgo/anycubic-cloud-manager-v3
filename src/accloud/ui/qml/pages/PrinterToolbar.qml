import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

RowLayout {
    id: root
    objectName: "printerToolbar"

    property bool loading: false
    property int totalCount: 0
    property int onlineCount: 0
    property int printingCount: 0
    property int offlineCount: 0

    signal refreshRequested()

    Layout.fillWidth: true
    spacing: Theme.gapRow

    AppButton {
        objectName: "refreshPrintersButton"
        text: root.loading ? qsTr("Refreshing...") : qsTr("Refresh printers")
        variant: "secondary"
        compact: true
        enabled: !root.loading
        onClicked: root.refreshRequested()
    }

    Item { Layout.fillWidth: true }

    Repeater {
        model: [
            { objectName: "printerFleetTotal", label: qsTr("Printers"), value: root.totalCount, tone: Theme.fgPrimary },
            { objectName: "printerFleetOnline", label: qsTr("Online"), value: root.onlineCount, tone: Theme.stateSuccess },
            { objectName: "printerFleetPrinting", label: qsTr("Printing"), value: root.printingCount, tone: Theme.stateRunning },
            { objectName: "printerFleetOffline", label: qsTr("Offline"), value: root.offlineCount, tone: Theme.fgSecondary }
        ]

        Rectangle {
            objectName: String(modelData.objectName)
            Layout.preferredHeight: 32
            Layout.preferredWidth: Math.max(108, fleetMetricContent.implicitWidth + 20)
            radius: Theme.radiusControl
            color: Theme.bgCardSubtle
            border.width: Theme.borderWidth
            border.color: Theme.borderSubtle

            RowLayout {
                id: fleetMetricContent
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 6

                Text {
                    text: String(modelData.label)
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontCaptionPx
                }

                Text {
                    text: String(modelData.value)
                    color: modelData.tone
                    font.pixelSize: Theme.fontBodyPx
                    font.bold: true
                }
            }
        }
    }
}
