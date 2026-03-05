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

    property string remotePrinterId: ""
    property string selectedCloudFileId: ""
    property bool optionDeleteAfterPrint: false
    property bool optionDryRun: false
    property bool optionHighPriority: false
    property bool optionLiftCompensation: false
    property bool optionAutoResinCheck: true

    ListModel {
        id: printersModel
    }

    ListModel {
        id: printCloudFilesModel
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
    }

    function loadMockPrinters() {
        printersModel.clear()
        printersModel.append({
            "id": "demo-printer-1",
            "name": "M7-Workshop-A",
            "model": "Photon Mono M7",
            "state": "READY",
            "reason": "free",
            "available": 1,
            "progress": -1,
            "elapsedSec": -1,
            "remainingSec": -1,
            "currentFile": ""
        })
        printersModel.append({
            "id": "demo-printer-2",
            "name": "M5S-Line-2",
            "model": "Photon Mono M5s",
            "state": "PRINTING",
            "reason": "printing",
            "available": 1,
            "progress": 43,
            "elapsedSec": 7800,
            "remainingSec": 10320,
            "currentFile": "atlas_plate_v12.pwmb"
        })
        printersModel.append({
            "id": "demo-printer-3",
            "name": "Backup-X2",
            "model": "Photon Mono X2",
            "state": "OFFLINE",
            "reason": "offline",
            "available": 0,
            "progress": -1,
            "elapsedSec": -1,
            "remainingSec": -1,
            "currentFile": ""
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

        statusMsg = String(printersModel.count) + " printer(s) loaded"
        statusSev = "success"
    }

    function countPrintersByState(state) {
        var wanted = String(state || "").toUpperCase()
        var count = 0
        for (var i = 0; i < printersModel.count; ++i) {
            var p = printersModel.get(i)
            if (String(p.state || "").toUpperCase() === wanted)
                count += 1
        }
        return count
    }

    function estimatedJobsCount() {
        return countPrintersByState("PRINTING")
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

    Component.onCompleted: loadPrinters()

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
                    if (currentIndex >= 0 && currentIndex < printersModel.count)
                        remotePrinterId = String(printersModel.get(currentIndex).id)
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

        footerTrailingData: [
            AppButton {
                text: "Close"
                variant: "secondary"
                onClicked: remotePrintConfigDialog.close()
            },
            AppButton {
                text: "Start Printing"
                variant: "primary"
                enabled: selectedCloudFileId.length > 0 && remotePrinterId.length > 0
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
            Layout.fillWidth: true
            spacing: 8

            AppButton {
                objectName: "refreshPrintersButton"
                text: loading ? "Refreshing..." : "Refresh printers"
                variant: "secondary"
                enabled: !loading
                onClicked: loadPrinters()
            }

            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Repeater {
                model: [
                    { "label": "Online",   "value": countPrintersByState("READY") },
                    { "label": "Offline",  "value": countPrintersByState("OFFLINE") },
                    { "label": "Printing", "value": countPrintersByState("PRINTING") },
                    { "label": "Jobs",     "value": estimatedJobsCount() }
                ]

                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    radius: Theme.radiusControl
                    color: Theme.bgSurface
                    border.width: Theme.borderWidth
                    border.color: Theme.borderDefault

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10

                        Text {
                            text: modelData.label
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.fontCaptionPx
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: String(modelData.value)
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.fontSectionPx
                            font.bold: true
                        }
                    }
                }
            }
        }

        InlineStatusBar {
            Layout.fillWidth: true
            message: statusMsg
            operationId: "op_printer_refresh"
            severity: statusSev
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 3
                radius: Theme.radiusControl
                color: Theme.bgSurface
                border.width: Theme.borderWidth
                border.color: Theme.borderDefault

                ListView {
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    spacing: 8
                    model: printersModel

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 118
                        radius: Theme.radiusControl
                        color: selectedPrinterId === String(model.id) ? Theme.selectionBg : Theme.bgSurface
                        border.width: Theme.borderWidth
                        border.color: Theme.borderDefault

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true

                                Text {
                                    Layout.fillWidth: true
                                    text: String(model.name || "-")
                                    color: Theme.fgPrimary
                                    font.pixelSize: Theme.fontSectionPx
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                StatusChip {
                                    status: root.statusChipText(model.state)
                                }
                            }

                            Text {
                                text: String(model.model || "-")
                                color: Theme.fgSecondary
                                font.pixelSize: Theme.fontBodyPx
                                elide: Text.ElideRight
                            }

                            Text {
                                text: "Job: " + (String(model.currentFile || "").length > 0 ? String(model.currentFile) : "-")
                                      + " | Progress: " + root.progressText(model.progress)
                                      + " | Remaining: " + root.timeText(model.remainingSec)
                                color: Theme.fgSecondary
                                font.pixelSize: Theme.fontCaptionPx
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                Layout.fillWidth: true

                                AppButton {
                                    text: "Details"
                                    variant: "secondary"
                                    onClicked: root.choosePrinter(model.id)
                                }

                                Item { Layout.fillWidth: true }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 2
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
                    }
                }
            }
        }
    }
}
