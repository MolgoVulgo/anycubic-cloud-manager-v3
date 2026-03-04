import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme

Item {
    id: root
    objectName: "logPage"
    Layout.fillWidth: true
    Layout.fillHeight: true

    property var logBackend: (typeof logBridge !== "undefined") ? logBridge : null
    property var allEntries: []
    property int maxTailLines: 1000
    property int shownCount: 0
    property int totalCount: 0
    property string statusText: "Prêt."
    property bool demoMode: false
    property bool loading: false

    function hasLogBackend() {
        return logBackend !== null
                && typeof logBackend.fetchSnapshot === "function"
    }

    function levelRank(level) {
        var upper = String(level).toUpperCase()
        if (upper === "DEBUG") return 0
        if (upper === "INFO") return 1
        if (upper === "WARN") return 2
        if (upper === "ERROR") return 3
        if (upper === "FATAL") return 4
        return 1
    }

    function minLevelRank() {
        var levelFilter = String(logLevelFilter.currentText)
        if (levelFilter === "INFO+") return 1
        if (levelFilter === "WARN+") return 2
        if (levelFilter === "ERROR") return 3
        return 0
    }

    function normalizeEntry(entry) {
        var out = {}
        out.sink = entry.sink !== undefined ? String(entry.sink) : "app"
        out.ts = entry.ts !== undefined ? String(entry.ts) : ""
        out.level = entry.level !== undefined ? String(entry.level) : "INFO"
        out.source = entry.source !== undefined ? String(entry.source) : out.sink
        out.component = entry.component !== undefined ? String(entry.component) : ""
        out.event = entry.event !== undefined ? String(entry.event) : ""
        out.opId = entry.opId !== undefined ? String(entry.opId) : ""
        out.message = entry.message !== undefined ? String(entry.message) : ""
        out.formatted = entry.formatted !== undefined
                      ? String(entry.formatted)
                      : "[" + out.sink + "] " + out.level + " " + out.message
        return out
    }

    function mockSnapshot() {
        return {
            "ok": true,
            "message": "Mode demo",
            "sources": ["app", "cloud", "qt", "fault"],
            "components": ["bootstrap", "cloud_client", "har_importer", "default"],
            "events": ["startup", "fetch_files_network_error", "qt_warning", "session_load_success"],
            "entries": [
                {
                    "sink": "app",
                    "ts": "2026-03-04T09:01:02.120+01:00",
                    "level": "INFO",
                    "source": "app",
                    "component": "bootstrap",
                    "event": "startup",
                    "opId": "",
                    "message": "Application bootstrap started",
                    "formatted": "2026-03-04T09:01:02.120+01:00 [app] app INFO bootstrap.startup - Application bootstrap started"
                },
                {
                    "sink": "cloud",
                    "ts": "2026-03-04T09:01:03.210+01:00",
                    "level": "DEBUG",
                    "source": "cloud",
                    "component": "har_importer",
                    "event": "session_load_success",
                    "opId": "op_cloud_boot",
                    "message": "Session loaded",
                    "formatted": "2026-03-04T09:01:03.210+01:00 [cloud] cloud DEBUG har_importer.session_load_success - Session loaded op_id=op_cloud_boot"
                },
                {
                    "sink": "qt",
                    "ts": "2026-03-04T09:01:04.012+01:00",
                    "level": "WARN",
                    "source": "qt",
                    "component": "default",
                    "event": "qt_warning",
                    "opId": "",
                    "message": "QML warning sample",
                    "formatted": "2026-03-04T09:01:04.012+01:00 [qt] qt WARN default.qt_warning - QML warning sample"
                },
                {
                    "sink": "fault",
                    "ts": "2026-03-04T09:01:05.900+01:00",
                    "level": "ERROR",
                    "source": "app",
                    "component": "cloud_client",
                    "event": "fetch_files_network_error",
                    "opId": "op_files_refresh_42",
                    "message": "Erreur réseau listing",
                    "formatted": "2026-03-04T09:01:05.900+01:00 [fault] app ERROR cloud_client.fetch_files_network_error - Erreur réseau listing op_id=op_files_refresh_42"
                }
            ]
        }
    }

    function updateComboOptions(combo, options, fallbackLabel) {
        var previous = combo.currentText
        combo.model = options
        var index = combo.find(previous)
        if (index < 0 && fallbackLabel !== undefined) {
            index = combo.find(fallbackLabel)
        }
        combo.currentIndex = index >= 0 ? index : 0
    }

    function updateDynamicFilters(snapshot) {
        var sourceOptions = ["All sources"]
        if (snapshot.sources !== undefined) {
            for (var i = 0; i < snapshot.sources.length; ++i) {
                var sourceValue = String(snapshot.sources[i])
                if (sourceValue.length > 0) {
                    sourceOptions.push(sourceValue)
                }
            }
        }

        var componentOptions = ["component:*"]
        if (snapshot.components !== undefined) {
            for (var c = 0; c < snapshot.components.length; ++c) {
                var componentValue = String(snapshot.components[c])
                if (componentValue.length > 0) {
                    componentOptions.push(componentValue)
                }
            }
        }

        var eventOptions = ["event:*"]
        if (snapshot.events !== undefined) {
            for (var e = 0; e < snapshot.events.length; ++e) {
                var eventValue = String(snapshot.events[e])
                if (eventValue.length > 0) {
                    eventOptions.push(eventValue)
                }
            }
        }

        updateComboOptions(logSourceFilter, sourceOptions, "All sources")
        updateComboOptions(logComponentFilter, componentOptions, "component:*")
        updateComboOptions(logEventFilter, eventOptions, "event:*")
    }

    function refreshLogs() {
        if (root.loading) return
        root.loading = true

        var snapshot
        if (hasLogBackend()) {
            snapshot = logBackend.fetchSnapshot(root.maxTailLines)
            root.demoMode = false
        } else {
            snapshot = mockSnapshot()
            root.demoMode = true
        }

        if (snapshot.ok !== true) {
            root.loading = false
            root.statusText = "Erreur chargement logs : " + String(snapshot.message)
            return
        }

        var normalized = []
        var incoming = snapshot.entries !== undefined ? snapshot.entries : []
        for (var i = 0; i < incoming.length; ++i) {
            normalized.push(normalizeEntry(incoming[i]))
        }
        root.allEntries = normalized
        root.totalCount = normalized.length
        updateDynamicFilters(snapshot)
        applyFilters()
        root.loading = false
    }

    function applyFilters() {
        var selectedSource = String(logSourceFilter.currentText)
        var selectedComponent = String(logComponentFilter.currentText)
        var selectedEvent = String(logEventFilter.currentText)
        var exactOpId = logOpIdFilter.text.trim()
        var queryText = logQueryFilter.text.trim().toLowerCase()
        var requiredRank = minLevelRank()

        var rendered = []
        for (var i = 0; i < root.allEntries.length; ++i) {
            var entry = root.allEntries[i]
            if (levelRank(entry.level) < requiredRank) continue
            if (selectedSource !== "All sources" && entry.sink !== selectedSource) continue
            if (selectedComponent !== "component:*" && entry.component !== selectedComponent) continue
            if (selectedEvent !== "event:*" && entry.event !== selectedEvent) continue
            if (exactOpId.length > 0 && entry.opId !== exactOpId) continue

            if (queryText.length > 0) {
                var lineLower = String(entry.formatted).toLowerCase()
                if (lineLower.indexOf(queryText) === -1) continue
            }
            rendered.push(String(entry.formatted))
        }

        root.shownCount = rendered.length
        logsArea.text = rendered.join("\n")

        var modeLabel = root.demoMode ? "Mode démo" : "Mode live"
        root.statusText = modeLabel
                + " • " + root.shownCount + "/" + root.totalCount + " ligne(s)"
                + " • " + (logSourceFilter.count - 1) + " source(s)"
    }

    Component.onCompleted: refreshLogs()

    Timer {
        id: pollTimer
        interval: 1000
        repeat: true
        running: true
        onTriggered: root.refreshLogs()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Text {
            text: "Runtime Logs"
            color: Theme.textPrimary
            font.pixelSize: 26
            font.bold: true
        }

        Text {
            text: "Tail multi-sources, filtres level/source/component/event/op_id + recherche texte."
            color: Theme.textSecondary
            font.pixelSize: 14
        }

        Rectangle {
            objectName: "logFiltersPanel"
            Layout.fillWidth: true
            Layout.preferredHeight: 116
            radius: 12
            color: Theme.panel
            border.width: 1
            border.color: Theme.panelStroke

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ComboBox {
                        id: logLevelFilter
                        objectName: "logLevelFilter"
                        model: ["ALL", "INFO+", "WARN+", "ERROR"]
                        Layout.preferredWidth: 105
                        onCurrentTextChanged: root.applyFilters()
                    }

                    ComboBox {
                        id: logSourceFilter
                        objectName: "logSourceFilter"
                        model: ["All sources"]
                        Layout.preferredWidth: 150
                        onCurrentTextChanged: root.applyFilters()
                    }

                    ComboBox {
                        id: logComponentFilter
                        objectName: "logComponentFilter"
                        model: ["component:*"]
                        Layout.preferredWidth: 170
                        onCurrentTextChanged: root.applyFilters()
                    }

                    ComboBox {
                        id: logEventFilter
                        objectName: "logEventFilter"
                        model: ["event:*"]
                        Layout.preferredWidth: 170
                        onCurrentTextChanged: root.applyFilters()
                    }

                    TextField {
                        id: logOpIdFilter
                        objectName: "logOpIdFilter"
                        placeholderText: "op_id exact"
                        Layout.preferredWidth: 160
                        onTextChanged: root.applyFilters()
                    }

                    TextField {
                        id: logQueryFilter
                        objectName: "logQueryFilter"
                        placeholderText: "query contains"
                        Layout.fillWidth: true
                        onTextChanged: root.applyFilters()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        id: logRefreshButton
                        objectName: "logRefreshButton"
                        text: root.loading ? "Refresh…" : "Refresh"
                        enabled: !root.loading
                        onClicked: root.refreshLogs()
                    }

                    Button {
                        id: logClearFiltersButton
                        objectName: "logClearFiltersButton"
                        text: "Reset filters"
                        onClicked: {
                            logLevelFilter.currentIndex = 0
                            logSourceFilter.currentIndex = 0
                            logComponentFilter.currentIndex = 0
                            logEventFilter.currentIndex = 0
                            logOpIdFilter.clear()
                            logQueryFilter.clear()
                            root.applyFilters()
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        id: logStatusLabel
                        objectName: "logStatusLabel"
                        text: root.statusText
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12
            color: Theme.cardAlt
            border.width: 1
            border.color: Theme.panelStroke

            ScrollView {
                id: logsScrollView
                objectName: "logsScrollView"
                anchors.fill: parent
                anchors.margins: 10
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AlwaysOn
                    active: true
                }
                ScrollBar.horizontal: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    active: true
                }

                TextArea {
                    id: logsArea
                    objectName: "logsTextArea"
                    width: Math.max(logsScrollView.availableWidth, implicitWidth)
                    height: Math.max(logsScrollView.availableHeight, implicitHeight)
                    readOnly: true
                    font.family: "JetBrains Mono"
                    color: Theme.mono
                    wrapMode: TextEdit.NoWrap
                    selectByMouse: true
                    placeholderText: "Aucune ligne de log pour les filtres actuels."
                    background: null
                }
            }
        }
    }
}
