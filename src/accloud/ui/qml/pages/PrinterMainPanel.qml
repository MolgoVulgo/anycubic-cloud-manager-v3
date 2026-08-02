import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

AppPageFrame {
    id: root

    property bool loading: false
    property bool developmentBuild: false
    property bool localFilePrintEnabled: true
    property string localFilePrintBlockReason: ""
    property var printersModel: null
    property string selectedPrinterId: ""
    property var tabTitleProvider: null
    property var selectedPrinter: null
    property var selectedPrinterDetails: ({})
    property var selectedLiveJobData: ({})
    property bool feedingOperationActive: false
    property int feedingOperationType: 0
    property bool feedingStopInProgress: false
    property bool loadingPrinterHistory: false
    property var printerHistoryModel: null
    property var statusChipTextProvider: null
    property var progressTextProvider: null
    property var timeTextProvider: null
    property var unixTimeTextProvider: null
    property var localizedTextProvider: null
    property int fleetRevision: 0

    readonly property var fleetCounts: {
        root.fleetRevision
        return root.calculateFleetCounts()
    }

    signal refreshRequested()
    signal printerSelected(string printerId)
    signal cloudFileRequested(string printerId)
    signal localFileRequested(string printerId)
    signal resinFeedRequested(string printerId, int feedType)
    signal resinFeedStopRequested(string printerId, int feedType)

    function printerIsPrinting(printer) {
        if (!printer)
            return false
        if (String(printer.state || "").toUpperCase() === "PRINTING")
            return true
        var state = String(printer.mqttPrintState || "").toLowerCase()
        if (state.length <= 0 && printer.details)
            state = String(printer.details.mqttPrintState || "").toLowerCase()
        return state === "printing"
                || state === "monitoring"
                || state === "preheating"
                || state === "paused"
                || state === "resuming"
                || state === "resumed"
    }

    function printerIsOffline(printer) {
        if (!printer)
            return true
        var state = String(printer.state || "").toUpperCase()
        if (state === "OFFLINE")
            return true
        var available = Number(printer.available)
        return isFinite(available) && available === 0
    }

    function calculateFleetCounts() {
        var counts = { total: 0, online: 0, printing: 0, offline: 0 }
        if (!root.printersModel || root.printersModel.count === undefined
                || typeof root.printersModel.get !== "function")
            return counts

        counts.total = Number(root.printersModel.count)
        if (!isFinite(counts.total) || counts.total < 0)
            counts.total = 0

        for (var i = 0; i < counts.total; ++i) {
            var printer = root.printersModel.get(i)
            if (root.printerIsOffline(printer)) {
                counts.offline += 1
                continue
            }
            counts.online += 1
            if (root.printerIsPrinting(printer))
                counts.printing += 1
        }
        return counts
    }

    anchors.fill: parent

    Connections {
        target: root.printersModel
        ignoreUnknownSignals: true
        function onDataChanged() { root.fleetRevision += 1 }
        function onModelReset() { root.fleetRevision += 1 }
        function onRowsInserted() { root.fleetRevision += 1 }
        function onRowsRemoved() { root.fleetRevision += 1 }
        function onCountChanged() { root.fleetRevision += 1 }
    }

    PrinterToolbar {
        loading: root.loading
        totalCount: Number(root.fleetCounts.total || 0)
        onlineCount: Number(root.fleetCounts.online || 0)
        printingCount: Number(root.fleetCounts.printing || 0)
        offlineCount: Number(root.fleetCounts.offline || 0)
        onRefreshRequested: root.refreshRequested()
    }

    Rectangle {
        id: printerTabsContainer
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: Theme.radiusControl
        color: Theme.bgSurface
        border.width: Theme.borderWidth
        border.color: Theme.borderDefault

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            PrintersTabsBar {
                Layout.fillWidth: true
                embeddedInTabsContainer: true
                printersModel: root.printersModel
                selectedPrinterId: root.selectedPrinterId
                tabTitleProvider: root.tabTitleProvider
                onPrinterSelected: function(printerId) { root.printerSelected(printerId) }
            }

            PrinterDetailPanel {
                id: printerDetailPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                embeddedInTabsContainer: true
                developmentBuild: root.developmentBuild
                selectedPrinter: root.selectedPrinter
                selectedPrinterId: root.selectedPrinterId
                selectedPrinterDetails: root.selectedPrinterDetails
                selectedLiveJobData: root.selectedLiveJobData
                feedingOperationActive: root.feedingOperationActive
                feedingOperationType: root.feedingOperationType
                feedingStopInProgress: root.feedingStopInProgress
                loadingPrinterHistory: root.loadingPrinterHistory
                localFilePrintEnabled: root.localFilePrintEnabled
                localFilePrintBlockReason: root.localFilePrintBlockReason
                printerHistoryModel: root.printerHistoryModel
                statusChipTextProvider: root.statusChipTextProvider
                progressTextProvider: root.progressTextProvider
                timeTextProvider: root.timeTextProvider
                unixTimeTextProvider: root.unixTimeTextProvider
                localizedTextProvider: root.localizedTextProvider
                onCloudFileRequested: function(printerId) { root.cloudFileRequested(printerId) }
                onLocalFileRequested: function(printerId) { root.localFileRequested(printerId) }
                onResinFeedRequested: function(printerId, feedType) { root.resinFeedRequested(printerId, feedType) }
                onResinFeedStopRequested: function(printerId, feedType) { root.resinFeedStopRequested(printerId, feedType) }
            }
        }
    }
}
