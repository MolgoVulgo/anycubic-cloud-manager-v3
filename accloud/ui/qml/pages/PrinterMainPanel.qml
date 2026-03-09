import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

AppPageFrame {
    id: root

    property bool loading: false
    property bool showDebugLabels: false
    property var printersModel: null
    property string selectedPrinterId: ""
    property var tabTitleProvider: null
    property var selectedPrinter: null
    property var selectedPrinterDetails: ({})
    property bool loadingPrinterHistory: false
    property var printerHistoryModel: null
    property string printersEndpointPath: ""
    property string printersEndpointRawJson: ""
    property string selectedPrinterHelpUrlText: ""
    property var statusChipTextProvider: null
    property var progressTextProvider: null
    property var timeTextProvider: null
    property var unixTimeTextProvider: null
    property var printStatusTextProvider: null
    property var prettyJsonProvider: null

    signal refreshRequested()
    signal debugToggled(bool checked)
    signal printerSelected(string printerId)
    signal cloudFileRequested(string printerId)
    signal localFileRequested()

    component DebugTag: Rectangle {
        property string label: ""
        visible: root.showDebugLabels
        z: 200
        radius: 4
        color: Qt.rgba(1.0, 0.95, 0.82, 0.95)
        border.width: 1
        border.color: Theme.warning
        implicitWidth: debugTagText.implicitWidth + 10
        implicitHeight: debugTagText.implicitHeight + 6

        Text {
            id: debugTagText
            anchors.centerIn: parent
            text: parent.label
            color: Theme.warning
            font.pixelSize: 10
            font.bold: true
        }
    }

    anchors.fill: parent

    PrinterToolbar {
        loading: root.loading
        debugChecked: root.showDebugLabels
        onRefreshRequested: root.refreshRequested()
        onDebugToggled: function(checked) { root.debugToggled(checked) }
    }

    Text {
        Layout.fillWidth: true
        visible: root.showDebugLabels
        text: qsTr("sections: printerToolbar | printersTabsBar | deviceDetailsPanel | endpointJsonPanel")
        color: Theme.warning
        font.pixelSize: Theme.fontCaptionPx
        elide: Text.ElideRight
    }

    PrintersTabsBar {
        printersModel: root.printersModel
        selectedPrinterId: root.selectedPrinterId
        tabTitleProvider: root.tabTitleProvider
        onPrinterSelected: function(printerId) { root.printerSelected(printerId) }
    }

    PrinterDetailPanel {
        id: printerDetailPanel
        selectedPrinter: root.selectedPrinter
        selectedPrinterDetails: root.selectedPrinterDetails
        loadingPrinterHistory: root.loadingPrinterHistory
        printerHistoryModel: root.printerHistoryModel
        showDebugLabels: root.showDebugLabels
        printersEndpointPath: root.printersEndpointPath
        printersEndpointRawJson: root.printersEndpointRawJson
        selectedPrinterHelpUrlText: root.selectedPrinterHelpUrlText
        statusChipTextProvider: root.statusChipTextProvider
        progressTextProvider: root.progressTextProvider
        timeTextProvider: root.timeTextProvider
        unixTimeTextProvider: root.unixTimeTextProvider
        printStatusTextProvider: root.printStatusTextProvider
        prettyJsonProvider: root.prettyJsonProvider
        onCloudFileRequested: function(printerId) { root.cloudFileRequested(printerId) }
        onLocalFileRequested: root.localFileRequested()
    }

    DebugTag {
        anchors.left: printerDetailPanel.left
        anchors.top: printerDetailPanel.top
        anchors.leftMargin: 8
        anchors.topMargin: 8
        label: "panel: deviceDetailsPanel"
    }
}
