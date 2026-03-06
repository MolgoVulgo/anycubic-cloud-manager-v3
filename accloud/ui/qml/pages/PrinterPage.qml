import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Item {
    id: root
    objectName: "printerPage"
    Layout.fillWidth: true
    Layout.fillHeight: true

    property bool loading: false
    property string statusMsg: "Ready."
    property string statusSev: "info" // info | success | warn | error
    property string selectedPrinterId: ""
    property bool debugUi: false
    property bool showDebugLabels: debugUi
    property string printersEndpointPath: "/p/p/workbench/api/work/printer/getPrinters + /p/p/workbench/api/work/project/getProjects?printer_id=<id>&print_status=1"
    property string printersEndpointRawJson: ""

    property string remotePrinterId: ""
    property string selectedCloudFileId: ""
    property bool optionDeleteAfterPrint: false
    property bool optionDryRun: false
    property bool optionHighPriority: false
    property bool optionLiftCompensation: false
    property bool optionAutoResinCheck: true
    property bool remotePrintAllowed: true
    property string remotePrintBlockReason: ""
    property var selectedPrinterDetails: ({})
    property bool loadingPrinterDetails: false
    property bool loadingPrinterHistory: false
    property bool reasonCatalogLoaded: false
    property bool reasonCatalogLoading: false
    property var reasonCatalogByCode: ({})

    ListModel {
        id: printersModel
    }

    ListModel {
        id: printCloudFilesModel
    }

    ListModel {
        id: printerHistoryModel
    }

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

    function hasCloudBridge() {
        return (typeof cloudBridge !== "undefined")
                && cloudBridge !== null
                && typeof cloudBridge.fetchPrinters === "function"
                && typeof cloudBridge.fetchFiles === "function"
                && typeof cloudBridge.sendPrintOrder === "function"
    }

    function hasCompatibilityEndpoint() {
        return hasCloudBridge() && typeof cloudBridge.fetchCompatiblePrintersByExt === "function"
    }

    function hasCompatibilityByFileIdEndpoint() {
        return hasCloudBridge() && typeof cloudBridge.fetchCompatiblePrintersByFileId === "function"
    }

    function fileType(fileName) {
        var name = String(fileName || "")
        var dot = name.lastIndexOf(".")
        if (dot < 0 || dot + 1 >= name.length)
            return "other"
        return name.slice(dot + 1).toLowerCase()
    }

    function statusChipText(state) {
        var raw = String(state || "").toUpperCase()
        if (raw === "OFFLINE") return "Offline"
        if (raw === "PRINTING") return "Printing"
        if (raw === "ERROR") return "Error"
        return "Ready"
    }

    function progressText(progress) {
        var value = Number(progress)
        if (!isFinite(value) || value < 0)
            return "-"
        return Math.max(0, Math.min(100, Math.round(value))) + "%"
    }

    function timeText(seconds) {
        var sec = Number(seconds)
        if (!isFinite(sec) || sec < 0)
            return "-"
        var h = Math.floor(sec / 3600)
        var m = Math.floor((sec % 3600) / 60)
        if (h > 0)
            return h + "h " + m + "m"
        return m + "m"
    }

    function unixTimeText(epochSeconds) {
        var value = Number(epochSeconds)
        if (!isFinite(value) || value <= 0)
            return "-"
        var d = new Date(value * 1000)
        return Qt.formatDateTime(d, "yyyy-MM-dd hh:mm")
    }

    function printStatusText(printStatus) {
        var s = Number(printStatus)
        if (!isFinite(s))
            return "-"
        if (s === 1) return "Printing"
        if (s === 2) return "Finished"
        if (s === 3) return "Failed"
        if (s === 4) return "Canceled"
        return String(s)
    }

    function progressRatio(progress) {
        var value = Number(progress)
        if (!isFinite(value) || value < 0)
            return 0
        return Math.max(0, Math.min(100, value)) / 100.0
    }

    function hasPrinterJob(printer) {
        if (printer === null || printer === undefined)
            return false

        var state = String(printer.state || "").toUpperCase()
        if (state === "PRINTING")
            return true
        if (String(printer.currentFile || "").length > 0)
            return true

        var progress = Number(printer.progress)
        if (isFinite(progress) && progress >= 0)
            return true

        var elapsed = Number(printer.elapsedSec)
        if (isFinite(elapsed) && elapsed >= 0)
            return true

        var remaining = Number(printer.remainingSec)
        return isFinite(remaining) && remaining >= 0
    }

    function printerSecondaryText(printer) {
        var parts = []
        var modelText = String(printer && printer.model !== undefined ? printer.model : "").trim()
        var typeText = String(printer && printer.type !== undefined ? printer.type : "").trim()
        var lastSeenText = String(printer && printer.lastSeen !== undefined ? printer.lastSeen : "").trim()

        if (modelText.length > 0)
            parts.push(modelText)
        if (typeText.length > 0)
            parts.push(typeText)
        if (lastSeenText.length > 0)
            parts.push("Last seen: " + lastSeenText)

        return parts.length > 0 ? parts.join(" \u00b7 ") : "-"
    }

    function printerTabTitle(printer) {
        var name = String(printer && printer.name !== undefined ? printer.name : "-")
        var status = statusChipText(printer ? printer.state : "READY")
        return name + " | " + status
    }

    function selectedPrinterIndex() {
        if (selectedPrinterId.length === 0)
            return -1
        for (var i = 0; i < printersModel.count; ++i) {
            if (String(printersModel.get(i).id) === selectedPrinterId)
                return i
        }
        return -1
    }

    function normalizedCompatReason(reasonText) {
        var text = String(reasonText || "").trim()
        if (text.length === 0)
            return ""
        var prefix = "unavailable reason:"
        if (text.toLowerCase().indexOf(prefix) === 0)
            text = text.slice(prefix.length).trim()
        return text
    }

    function canStartFromPrinterState(printer) {
        if (!printer)
            return { "ok": false, "reason": "Select a printer first." }

        var state = String(printer.state || "").toUpperCase()
        if (state === "OFFLINE")
            return { "ok": false, "reason": "Printer offline." }
        if (state === "PRINTING")
            return { "ok": false, "reason": "Printer is currently printing." }
        if (state === "ERROR")
            return { "ok": false, "reason": (String(printer.reason || "").length > 0 ? displayReason(printer.reason) : "Printer reported an error.") }
        return { "ok": true, "reason": "" }
    }

    function evaluateRemotePrintGuard() {
        var printer = null
        for (var i = 0; i < printersModel.count; ++i) {
            var candidate = printersModel.get(i)
            if (String(candidate.id) === String(remotePrinterId)) {
                printer = candidate
                break
            }
        }

        var stateCheck = canStartFromPrinterState(printer)
        if (stateCheck.ok !== true)
            return stateCheck

        if (selectedCloudFileId.length === 0)
            return { "ok": false, "reason": "Select a cloud file first." }

        if (!hasCompatibilityByFileIdEndpoint())
            return { "ok": true, "reason": "" }

        var compat = cloudBridge.fetchCompatiblePrintersByFileId(selectedCloudFileId)
        if (compat.ok !== true)
            return { "ok": false, "reason": "Compatibility check failed: " + String(compat.message || "unknown error") }

        var list = compat.printers !== undefined ? compat.printers : []
        for (var j = 0; j < list.length; ++j) {
            var item = list[j]
            if (String(item.id) !== String(remotePrinterId))
                continue
            var available = Number(item.available)
            if (isFinite(available) && available > 0)
                return { "ok": true, "reason": "" }
            var reason = normalizedCompatReason(item.reason)
            return { "ok": false, "reason": reason.length > 0 ? reason : "Printer is not compatible with this file." }
        }

        return { "ok": false, "reason": "Selected printer not returned by compatibility check." }
    }

    function refreshRemotePrintGuard() {
        var result = evaluateRemotePrintGuard()
        remotePrintAllowed = (result.ok === true)
        remotePrintBlockReason = String(result.reason || "")
    }

    function prettyJson(rawPayload) {
        var text = String(rawPayload || "").trim()
        if (text.length === 0)
            return "{\n  \"message\": \"No endpoint response captured.\"\n}"
        try {
            return JSON.stringify(JSON.parse(text), null, 2)
        } catch (error) {
            return text
        }
    }

    function selectedPrinterData() {
        if (selectedPrinterId.length === 0)
            return null
        for (var i = 0; i < printersModel.count; ++i) {
            var p = printersModel.get(i)
            if (String(p.id) === selectedPrinterId)
                return p
        }
        return null
    }

    function selectedCloudFileData() {
        if (selectedCloudFileId.length === 0)
            return null
        for (var i = 0; i < printCloudFilesModel.count; ++i) {
            var f = printCloudFilesModel.get(i)
            if (String(f.fileId) === selectedCloudFileId)
                return f
        }
        return null
    }

    function choosePrinter(printerId) {
        selectedPrinterId = String(printerId || "")
        loadSelectedPrinterInsights()
    }

    function ensureReasonCatalogLoaded() {
        if (reasonCatalogLoaded || reasonCatalogLoading)
            return
        if (!hasCloudBridge() || typeof cloudBridge.fetchReasonCatalog !== "function")
            return

        reasonCatalogLoading = true
        var r = cloudBridge.fetchReasonCatalog()
        reasonCatalogLoading = false
        if (r.ok !== true) {
            statusMsg = "Reason catalog unavailable: " + String(r.message || "")
            statusSev = "warn"
            return
        }

        var map = {}
        var reasons = r.reasons !== undefined ? r.reasons : []
        for (var i = 0; i < reasons.length; ++i) {
            var entry = reasons[i]
            map[String(entry.reason)] = entry
        }
        reasonCatalogByCode = map
        reasonCatalogLoaded = true
    }

    function reasonEntryFromText(reasonText) {
        ensureReasonCatalogLoaded()
        var text = String(reasonText || "").trim()
        if (text.length === 0)
            return null

        var code = ""
        if (/^-?\d+$/.test(text)) {
            code = text
        } else {
            var m = text.match(/-?\d+/)
            if (m && m.length > 0)
                code = String(m[0])
        }
        if (code.length === 0)
            return null

        var entry = reasonCatalogByCode[code]
        return entry !== undefined ? entry : null
    }

    function displayReason(reasonText) {
        var text = String(reasonText || "").trim()
        if (text.length === 0)
            return ""

        var entry = reasonEntryFromText(text)
        if (!entry)
            return text

        var desc = String(entry.desc || "").trim()
        if (desc.length === 0)
            return text
        return desc + " (" + text + ")"
    }

    function reasonHelpUrl(reasonText) {
        var entry = reasonEntryFromText(reasonText)
        if (!entry)
            return ""
        return String(entry.helpUrl || "").trim()
    }

    function loadSelectedPrinterInsights() {
        selectedPrinterDetails = ({})
        printerHistoryModel.clear()

        if (selectedPrinterId.length === 0 || !hasCloudBridge())
            return

        if (typeof cloudBridge.fetchPrinterDetails === "function") {
            loadingPrinterDetails = true
            var detailsRes = cloudBridge.fetchPrinterDetails(selectedPrinterId)
            loadingPrinterDetails = false
            if (detailsRes.ok === true && detailsRes.details !== undefined)
                selectedPrinterDetails = detailsRes.details
        }

        if (typeof cloudBridge.fetchPrinterProjects === "function") {
            loadingPrinterHistory = true
            var historyRes = cloudBridge.fetchPrinterProjects(selectedPrinterId, 1, 10)
            loadingPrinterHistory = false
            if (historyRes.ok === true) {
                var projects = historyRes.projects !== undefined ? historyRes.projects : []
                for (var i = 0; i < projects.length; ++i)
                    printerHistoryModel.append(projects[i])
            }
        }
    }

    function loadMockPrinters() {
        printersEndpointPath = "demo://printers"
        printersEndpointRawJson = "{\n  \"mode\": \"demo\",\n  \"message\": \"Backend unavailable\"\n}"
        printersModel.clear()
        printersModel.append({
            "id": "demo-printer-1",
            "name": "M7-Workshop-A",
            "model": "Photon Mono M7",
            "type": "LCD",
            "state": "READY",
            "reason": "free",
            "available": 1,
            "progress": -1,
            "elapsedSec": -1,
            "remainingSec": -1,
            "currentFile": "",
            "lastSeen": "just now"
        })
        printersModel.append({
            "id": "demo-printer-2",
            "name": "M5S-Line-2",
            "model": "Photon Mono M5s",
            "type": "LCD",
            "state": "PRINTING",
            "reason": "printing",
            "available": 1,
            "progress": 43,
            "elapsedSec": 7800,
            "remainingSec": 10320,
            "currentFile": "atlas_plate_v12.pwmb",
            "lastSeen": "1 min ago"
        })
        printersModel.append({
            "id": "demo-printer-3",
            "name": "Backup-X2",
            "model": "Photon Mono X2",
            "type": "LCD",
            "state": "OFFLINE",
            "reason": "offline",
            "available": 0,
            "progress": -1,
            "elapsedSec": -1,
            "remainingSec": -1,
            "currentFile": "",
            "lastSeen": "23 min ago"
        })

        if (selectedPrinterId.length === 0 && printersModel.count > 0)
            selectedPrinterId = String(printersModel.get(0).id)

        statusMsg = "Demo mode (backend unavailable)."
        statusSev = "warn"
        loading = false
    }

    function loadPrinters() {
        if (loading)
            return

        loading = true
        statusMsg = "Refreshing printers..."
        statusSev = "info"

        if (!hasCloudBridge()) {
            loadMockPrinters()
            return
        }

        var r = cloudBridge.fetchPrinters()
        loading = false
        printersEndpointPath = String(r.endpoint || printersEndpointPath)
        printersEndpointRawJson = String(r.rawJson || "")

        if (r.ok !== true) {
            statusMsg = "Printer listing failed: " + String(r.message)
            statusSev = "error"
            return
        }

        printersModel.clear()
        var printers = r.printers !== undefined ? r.printers : []
        for (var i = 0; i < printers.length; ++i)
            printersModel.append(printers[i])

        if (printersModel.count > 0) {
            var keepSelection = false
            for (var j = 0; j < printersModel.count; ++j) {
                if (String(printersModel.get(j).id) === selectedPrinterId) {
                    keepSelection = true
                    break
                }
            }
            if (!keepSelection)
                selectedPrinterId = String(printersModel.get(0).id)
        } else {
            selectedPrinterId = ""
        }

        loadSelectedPrinterInsights()
        statusMsg = String(printersModel.count) + " printer(s) loaded"
        statusSev = "success"
    }

    function compatibilityAllowsPrinter(compatResult, printerId) {
        if (compatResult === null || compatResult === undefined)
            return true
        if (compatResult.ok !== true)
            return true

        var list = compatResult.printers !== undefined ? compatResult.printers : []
        for (var i = 0; i < list.length; ++i) {
            var item = list[i]
            if (String(item.id) !== String(printerId))
                continue
            var available = Number(item.available)
            return isFinite(available) ? available > 0 : true
        }
        return false
    }

    function loadCloudFilesForRemotePrint(printerId) {
        printCloudFilesModel.clear()
        selectedCloudFileId = ""

        var files = []
        if (hasCloudBridge()) {
            var listing = cloudBridge.fetchFiles(1, 100)
            if (listing.ok === true) {
                files = listing.files !== undefined ? listing.files : []
            } else {
                statusMsg = "Cannot load cloud files for print: " + String(listing.message)
                statusSev = "error"
                return
            }
        } else {
            files = [
                {
                    "fileId": "demo-001",
                    "fileName": "rook_plate_v12.pwmb",
                    "sizeText": "42.6 MB",
                    "status": "READY",
                    "printTime": "02h 15m",
                    "resinUsage": "67 ml"
                },
                {
                    "fileId": "demo-002",
                    "fileName": "calibration_tower.pws",
                    "sizeText": "11.8 MB",
                    "status": "READY",
                    "printTime": "00h 48m",
                    "resinUsage": "14 ml"
                }
            ]
        }

        var compatCache = {}
        var compatFailed = false

        for (var i = 0; i < files.length; ++i) {
            var file = files[i]
            var ext = fileType(file.fileName)
            var compat = null

            if (hasCompatibilityEndpoint()) {
                if (compatCache[ext] === undefined) {
                    compatCache[ext] = cloudBridge.fetchCompatiblePrintersByExt(ext)
                }
                compat = compatCache[ext]
                if (compat.ok !== true)
                    compatFailed = true
            }

            if (compatibilityAllowsPrinter(compat, printerId))
                printCloudFilesModel.append(file)
        }

        if (compatFailed) {
            statusMsg = "Compatibility endpoint partial failure. Showing best-effort list."
            statusSev = "warn"
        }

        if (printCloudFilesModel.count > 0)
            selectedCloudFileId = String(printCloudFilesModel.get(0).fileId)
    }

    function openSelectCloudFileDialog(printerId) {
        remotePrinterId = String(printerId || selectedPrinterId)
        loadCloudFilesForRemotePrint(remotePrinterId)
        selectCloudFileDialog.open()
    }

    function openRemotePrintConfig() {
        if (selectedCloudFileId.length === 0)
            return

        optionDeleteAfterPrint = false
        optionDryRun = false
        optionHighPriority = false
        remotePrintAllowed = true
        remotePrintBlockReason = ""
        refreshRemotePrintGuard()
        remotePrintConfigDialog.open()
    }

    function startRemotePrint() {
        if (remotePrinterId.length === 0) {
            statusMsg = "Select a printer first."
            statusSev = "warn"
            return
        }

        var fileData = selectedCloudFileData()
        if (!fileData) {
            statusMsg = "Select a cloud file first."
            statusSev = "warn"
            return
        }

        refreshRemotePrintGuard()
        if (!remotePrintAllowed) {
            statusMsg = remotePrintBlockReason.length > 0
                    ? ("Print blocked: " + remotePrintBlockReason)
                    : "Print blocked by compatibility checks."
            statusSev = "warn"
            return
        }

        if (!hasCloudBridge()) {
            statusMsg = "Demo: remote print payload prepared for " + String(fileData.fileName)
            statusSev = "warn"
            remotePrintConfigDialog.close()
            return
        }

        var r = cloudBridge.sendPrintOrder(remotePrinterId,
                                           String(fileData.fileId),
                                           optionDeleteAfterPrint,
                                           optionDryRun)
        if (r.ok === true) {
            var taskId = String(r.taskId || "")
            statusMsg = taskId.length > 0
                    ? ("Print order sent (task_id=" + taskId + ")")
                    : "Print order sent."
            statusSev = optionDryRun ? "warn" : "success"
            remotePrintConfigDialog.close()
            loadPrinters()
        } else {
            statusMsg = "Print order failed: " + String(r.message)
            statusSev = "error"
        }
    }

    Component.onCompleted: {
        ensureReasonCatalogLoaded()
        loadPrinters()
    }

    AppDialogFrame {
        id: selectCloudFileDialog
        title: "Select Cloud File"
        subtitle: "Compatible files for the selected printer"
        minimumWidth: 820
        maximumWidth: 980

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: Theme.bgSurface
            border.width: 0

            RowLayout {
                anchors.fill: parent
                spacing: 8

                Text { Layout.preferredWidth: 32; text: ""; color: Theme.fgSecondary }
                Text { Layout.fillWidth: true; text: "File name"; color: Theme.fgSecondary; font.pixelSize: Theme.fontCaptionPx }
                Text { Layout.preferredWidth: 80; text: "Type"; color: Theme.fgSecondary; font.pixelSize: Theme.fontCaptionPx }
                Text { Layout.preferredWidth: 90; text: "Size"; color: Theme.fgSecondary; font.pixelSize: Theme.fontCaptionPx }
                Text { Layout.preferredWidth: 86; text: "Status"; color: Theme.fgSecondary; font.pixelSize: Theme.fontCaptionPx }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderSubtle
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: printCloudFilesModel
            spacing: 0

            delegate: Rectangle {
                width: ListView.view.width
                height: 48
                color: selectedCloudFileId === String(model.fileId) ? Theme.selectionBg : Theme.bgSurface
                border.width: 0

                RowLayout {
                    anchors.fill: parent
                    spacing: 8

                    RadioButton {
                        Layout.preferredWidth: 32
                        checked: selectedCloudFileId === String(model.fileId)
                        onClicked: selectedCloudFileId = String(model.fileId)
                    }

                    Text {
                        Layout.fillWidth: true
                        text: String(model.fileName || "-")
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontBodyPx
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.preferredWidth: 80
                        text: fileType(model.fileName).toUpperCase()
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontBodyPx
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.preferredWidth: 90
                        text: String(model.sizeText || "-")
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontBodyPx
                        horizontalAlignment: Text.AlignRight
                    }

                    Text {
                        Layout.preferredWidth: 86
                        text: String(model.status || "-")
                        color: Theme.fgSecondary
                        font.pixelSize: Theme.fontBodyPx
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            footer: Text {
                width: parent ? parent.width : 0
                visible: printCloudFilesModel.count === 0
                text: "No compatible cloud file for this printer."
                color: Theme.fgSecondary
                font.pixelSize: Theme.fontBodyPx
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                padding: 18
            }
        }

        footerTrailingData: [
            AppButton {
                text: "Close"
                variant: "secondary"
                onClicked: selectCloudFileDialog.close()
            },
            AppButton {
                text: "Start Printing"
                variant: "primary"
                enabled: selectedCloudFileId.length > 0
                onClicked: {
                    selectCloudFileDialog.close()
                    openRemotePrintConfig()
                }
            }
        ]
    }

    AppDialogFrame {
        id: remotePrintConfigDialog
        title: "Remote Print Config"
        subtitle: "Review task, printer and options before start"
        minimumWidth: 760
        maximumWidth: 900

        onOpened: {
            for (var i = 0; i < printersModel.count; ++i) {
                if (String(printersModel.get(i).id) === remotePrinterId) {
                    remotePrinterCombo.currentIndex = i
                    break
                }
            }
            refreshRemotePrintGuard()
        }

        SectionHeader {
            Layout.fillWidth: true
            title: "Print Task"
            subtitle: "Selected cloud file summary"
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 86
            radius: Theme.radiusControl
            color: Theme.bgSurface
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                Text {
                    text: (selectedCloudFileData() ? String(selectedCloudFileData().fileName) : "-")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontSectionPx
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    text: "Printer: " + (selectedPrinterData() ? String(selectedPrinterData().name) : "-")
                          + " | Est: " + (selectedCloudFileData() ? String(selectedCloudFileData().printTime || "-") : "-")
                          + " | Resin: " + (selectedCloudFileData() ? String(selectedCloudFileData().resinUsage || "-") : "-")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontBodyPx
                    elide: Text.ElideRight
                }
            }
        }

        SectionHeader {
            Layout.fillWidth: true
            title: "Select Printer"
            subtitle: "Change target printer if needed"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            AppComboBox {
                id: remotePrinterCombo
                Layout.fillWidth: true
                model: printersModel
                textRole: "name"

                Component.onCompleted: {
                    for (var i = 0; i < printersModel.count; ++i) {
                        if (String(printersModel.get(i).id) === remotePrinterId) {
                            currentIndex = i
                            break
                        }
                    }
                }

                onActivated: {
                    if (currentIndex >= 0 && currentIndex < printersModel.count) {
                        remotePrinterId = String(printersModel.get(currentIndex).id)
                        refreshRemotePrintGuard()
                    }
                }
            }

            AppButton {
                text: "Change"
                variant: "secondary"
                onClicked: {
                    remotePrintConfigDialog.close()
                    openSelectCloudFileDialog(remotePrinterId)
                }
            }
        }

        SectionHeader {
            Layout.fillWidth: true
            title: "Options"
            subtitle: "Fast options before start"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            CheckBox {
                text: "High priority"
                checked: optionHighPriority
                onToggled: optionHighPriority = checked
            }

            CheckBox {
                text: "Delete file after print"
                checked: optionDeleteAfterPrint
                onToggled: optionDeleteAfterPrint = checked
            }

            CheckBox {
                text: "Dry-run"
                checked: optionDryRun
                onToggled: optionDryRun = checked
            }

            Item { Layout.fillWidth: true }

            AppButton {
                text: "More"
                variant: "secondary"
                onClicked: printConfigDialog.open()
            }
        }

        Text {
            Layout.fillWidth: true
            visible: !remotePrintAllowed
            text: remotePrintBlockReason.length > 0
                  ? ("Start blocked: " + remotePrintBlockReason)
                  : "Start blocked by compatibility checks."
            color: Theme.danger
            font.pixelSize: Theme.fontCaptionPx
            wrapMode: Text.WordWrap
        }

        footerTrailingData: [
            AppButton {
                text: "Close"
                variant: "secondary"
                onClicked: remotePrintConfigDialog.close()
            },
            AppButton {
                text: "Start Printing"
                variant: "primary"
                enabled: selectedCloudFileId.length > 0
                         && remotePrinterId.length > 0
                         && remotePrintAllowed
                onClicked: startRemotePrint()
            }
        ]
    }

    AppDialogFrame {
        id: printConfigDialog
        title: "Print Config"
        subtitle: "Advanced flags"
        minimumWidth: 620
        maximumWidth: 760

        CheckBox {
            text: "Lift compensation"
            checked: optionLiftCompensation
            onToggled: optionLiftCompensation = checked
        }

        Text {
            text: "Adds extra stabilization on Z lifts."
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontCaptionPx
            opacity: 0.9
        }

        CheckBox {
            text: "Auto resin check"
            checked: optionAutoResinCheck
            onToggled: optionAutoResinCheck = checked
        }

        Text {
            text: "Best-effort pre-check before sending order."
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontCaptionPx
            opacity: 0.9
        }

        footerTrailingData: [
            AppButton {
                text: "Close"
                variant: "secondary"
                onClicked: printConfigDialog.close()
            }
        ]
    }

    AppPageFrame {
        anchors.fill: parent

        RowLayout {
            objectName: "printerToolbar"
            Layout.fillWidth: true
            spacing: 8

            AppButton {
                objectName: "refreshPrintersButton"
                text: loading ? "Refreshing..." : "Refresh printers"
                variant: "secondary"
                enabled: !loading
                onClicked: loadPrinters()
            }

            CheckBox {
                objectName: "debugLabelsToggle"
                text: "Debug UI"
                checked: root.showDebugLabels
                onToggled: root.showDebugLabels = checked
            }

            Item { Layout.fillWidth: true }
        }

        InlineStatusBar {
            objectName: "printersStatusBar"
            Layout.fillWidth: true
            message: statusMsg
            operationId: "op_printer_refresh"
            severity: statusSev
        }

        Text {
            Layout.fillWidth: true
            visible: root.showDebugLabels
            text: "sections: printerToolbar | printersStatusBar | printersTabsBar | deviceDetailsPanel | endpointJsonPanel"
            color: Theme.warning
            font.pixelSize: Theme.fontCaptionPx
            elide: Text.ElideRight
        }

        Rectangle {
            objectName: "printersTabsBar"
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            radius: Theme.radiusControl
            color: Theme.bgSurface
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            TabBar {
                id: printersTabBar
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4
                clip: true

                Repeater {
                    model: printersModel

                    TabButton {
                        objectName: "printerTabButton"
                        required property int index
                        readonly property var printer: printersModel.get(index)
                        text: root.printerTabTitle(printer)
                        width: Math.min(340, Math.max(170, implicitWidth + 16))
                        checked: selectedPrinterId === String(printer.id || "")
                        onClicked: root.choosePrinter(printer.id)
                    }
                }
            }
        }

        Rectangle {
            objectName: "deviceDetailsPanel"
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusControl
            color: Theme.bgSurface
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                    Text {
                        text: "Device Details"
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.fontTitlePx
                        font.bold: true
                    }

                    Text {
                        visible: root.selectedPrinterData() === null
                        text: "Select a printer to view details and remote print entrypoints."
                        color: Theme.fgSecondary
                        font.pixelSize: Theme.fontBodyPx
                        wrapMode: Text.WordWrap
                    }

                    ColumnLayout {
                        visible: root.selectedPrinterData() !== null
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: String(root.selectedPrinterData() ? root.selectedPrinterData().name : "-")
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.fontSectionPx
                            font.bold: true
                        }

                        Text {
                            text: "Model: " + String(root.selectedPrinterData() ? root.selectedPrinterData().model : "-")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontBodyPx
                        }

                        Text {
                            text: "Firmware: "
                                  + (String(root.selectedPrinterDetails.firmwareVersion || "").length > 0
                                     ? String(root.selectedPrinterDetails.firmwareVersion)
                                     : "-")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontBodyPx
                        }

                        RowLayout {
                            spacing: 8

                            Text {
                                text: "Status:"
                                color: Theme.fgSecondary
                                font.pixelSize: Theme.fontBodyPx
                            }

                            StatusChip {
                                status: root.statusChipText(root.selectedPrinterData() ? root.selectedPrinterData().state : "READY")
                            }
                        }

                        Text {
                            visible: String(root.selectedPrinterData() ? root.selectedPrinterData().reason : "").length > 0
                            text: "Reason: " + root.displayReason(root.selectedPrinterData() ? root.selectedPrinterData().reason : "")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            visible: root.reasonHelpUrl(root.selectedPrinterData() ? root.selectedPrinterData().reason : "").length > 0
                            text: "Help: " + root.reasonHelpUrl(root.selectedPrinterData() ? root.selectedPrinterData().reason : "")
                            color: Theme.accent
                            font.pixelSize: Theme.fontCaptionPx
                            elide: Text.ElideRight
                        }

                        Text {
                            text: "Current job: "
                                  + (root.selectedPrinterData() && String(root.selectedPrinterData().currentFile || "").length > 0
                                     ? String(root.selectedPrinterData().currentFile)
                                     : "-")
                                  + " | Progress: " + root.progressText(root.selectedPrinterData() ? root.selectedPrinterData().progress : -1)
                                  + " | Elapsed: " + root.timeText(root.selectedPrinterData() ? root.selectedPrinterData().elapsedSec : -1)
                                  + " | Remaining: " + root.timeText(root.selectedPrinterData() ? root.selectedPrinterData().remainingSec : -1)
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            text: "Print count: "
                                  + (String(root.selectedPrinterDetails.printCount || "").length > 0
                                     ? String(root.selectedPrinterDetails.printCount)
                                     : "-")
                                  + " | Total print time: "
                                  + (String(root.selectedPrinterDetails.printTotalTime || "").length > 0
                                     ? String(root.selectedPrinterDetails.printTotalTime)
                                     : "-")
                                  + " | Material used: "
                                  + (String(root.selectedPrinterDetails.materialUsed || "").length > 0
                                     ? String(root.selectedPrinterDetails.materialUsed)
                                     : "-")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            visible: String(root.selectedPrinterDetails.helpUrl || "").length > 0
                            text: "Device help: " + String(root.selectedPrinterDetails.helpUrl || "")
                            color: Theme.accent
                            font.pixelSize: Theme.fontCaptionPx
                            elide: Text.ElideRight
                        }

                        Text {
                            visible: String(root.selectedPrinterDetails.quickStartUrl || "").length > 0
                            text: "Quick start: " + String(root.selectedPrinterDetails.quickStartUrl || "")
                            color: Theme.accent
                            font.pixelSize: Theme.fontCaptionPx
                            elide: Text.ElideRight
                        }

                        Text {
                            visible: Number(root.selectedPrinterDetails.tools ? root.selectedPrinterDetails.tools.length : 0) > 0
                            text: "Tools: "
                                  + (root.selectedPrinterDetails.tools
                                     ? root.selectedPrinterDetails.tools.slice(0, 6).join(", ")
                                     : "")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            visible: Number(root.selectedPrinterDetails.advances ? root.selectedPrinterDetails.advances.length : 0) > 0
                            text: "Advanced: "
                                  + (root.selectedPrinterDetails.advances
                                     ? root.selectedPrinterDetails.advances.slice(0, 4).join(", ")
                                     : "")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            AppButton {
                                text: "From Cloud File"
                                variant: "primary"
                                enabled: root.selectedPrinterData() !== null
                                onClicked: root.openSelectCloudFileDialog(root.selectedPrinterData().id)
                            }

                            AppButton {
                                text: "From Local File"
                                variant: "secondary"
                                onClicked: {
                                    root.statusMsg = "Local file remote print entrypoint is not implemented yet."
                                    root.statusSev = "warn"
                                }
                            }
                        }

                        SectionHeader {
                            Layout.fillWidth: true
                            title: "Recent Jobs"
                            subtitle: loadingPrinterHistory ? "Loading..." : "Latest projects for this printer"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 180
                            radius: Theme.radiusControl
                            color: Theme.bgWindow
                            border.width: Theme.borderWidth
                            border.color: Theme.borderSubtle

                            ListView {
                                anchors.fill: parent
                                anchors.margins: 8
                                clip: true
                                spacing: 6
                                model: printerHistoryModel

                                delegate: Rectangle {
                                    width: ListView.view.width
                                    height: 42
                                    color: "transparent"

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 2

                                        Text {
                                            Layout.fillWidth: true
                                            text: String(model.gcodeName || "-") + " • " + root.printStatusText(model.printStatus)
                                            color: Theme.fgPrimary
                                            font.pixelSize: Theme.fontCaptionPx
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "Task " + String(model.taskId || "-")
                                                  + " | Progress " + root.progressText(model.progress)
                                                  + " | Start " + root.unixTimeText(model.createTime)
                                                  + " | End " + root.unixTimeText(model.endTime)
                                            color: Theme.fgSecondary
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                        }
                                    }
                                }

                                footer: Text {
                                    width: parent ? parent.width : 0
                                    visible: printerHistoryModel.count === 0
                                    text: loadingPrinterHistory ? "Loading history..." : "No project history for this printer."
                                    color: Theme.fgSecondary
                                    font.pixelSize: Theme.fontCaptionPx
                                    horizontalAlignment: Text.AlignHCenter
                                    padding: 10
                                }
                            }
                        }
                    }

                    Rectangle {
                        objectName: "endpointJsonPanel"
                        visible: root.showDebugLabels
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 220
                        radius: Theme.radiusControl
                        color: Theme.bgWindow
                        border.width: Theme.borderWidth
                        border.color: Theme.borderSubtle

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 6

                            Text {
                                Layout.fillWidth: true
                                text: "Endpoint JSON: " + root.printersEndpointPath
                                color: Theme.warning
                                font.pixelSize: Theme.fontCaptionPx
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true

                                TextArea {
                                    readOnly: true
                                    text: root.prettyJson(root.printersEndpointRawJson)
                                    wrapMode: TextEdit.NoWrap
                                    color: Theme.fgPrimary
                                    font.family: "monospace"
                                    font.pixelSize: Theme.fontCaptionPx
                                    background: Rectangle {
                                        color: "transparent"
                                    }
                                }
                            }
                        }
                    }
                }

                DebugTag {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 8
                    anchors.topMargin: 8
                    label: "panel: deviceDetailsPanel"
                }
            }
        }
    }
