import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

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
    property string statusText: qsTr("Ready.")
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
        var levelFilter = comboSelectedValue(logLevelFilter, "all")
        if (levelFilter === "info_plus") return 1
        if (levelFilter === "warn_plus") return 2
        if (levelFilter === "error") return 3
        return 0
    }

    function comboSelectedValue(combo, fallbackValue) {
        if (!combo || combo.currentIndex < 0 || combo.model === undefined || combo.model === null)
            return String(fallbackValue || "")
        if (combo.currentIndex >= combo.model.length)
            return String(fallbackValue || "")
        var entry = combo.model[combo.currentIndex]
        if (entry !== null && entry !== undefined && entry.value !== undefined)
            return String(entry.value)
        return String(entry !== undefined ? entry : (fallbackValue || ""))
    }

    function comboIndexForValue(model, valueToFind) {
        var target = String(valueToFind || "")
        for (var i = 0; i < model.length; ++i) {
            var entry = model[i]
            var value = (entry !== null && entry !== undefined && entry.value !== undefined)
                    ? String(entry.value)
                    : String(entry)
            if (value === target)
                return i
        }
        return -1
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
        out.logicalSource = normalizedSourceLabel(out)
        return out
    }

    function normalizedSourceLabel(entry) {
        var sink = String(entry && entry.sink !== undefined ? entry.sink : "").toLowerCase().trim()
        var source = String(entry && entry.source !== undefined ? entry.source : sink).toLowerCase().trim()
        var component = String(entry && entry.component !== undefined ? entry.component : "").toLowerCase().trim()
        var eventName = String(entry && entry.event !== undefined ? entry.event : "").toLowerCase().trim()
        if (sink === "mqtt" || source === "mqtt")
            return "mqtt"
        if (component.indexOf("mqtt") === 0 || eventName.indexOf("mqtt") === 0)
            return "mqtt"
        if (component.indexOf("mqtt_") >= 0 || eventName.indexOf("mqtt_") >= 0)
            return "mqtt"
        if (source.length > 0)
            return source
        if (sink.length > 0)
            return sink
        return "app"
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
                    "message": "Network error while listing files",
                    "formatted": "2026-03-04T09:01:05.900+01:00 [fault] app ERROR cloud_client.fetch_files_network_error - Network error while listing files op_id=op_files_refresh_42"
                }
            ]
        }
    }

    function updateComboOptions(combo, options, fallbackLabel) {
        var previousValue = comboSelectedValue(combo, "")
        combo.model = options
        var index = comboIndexForValue(options, previousValue)
        if (index < 0)
            index = comboIndexForValue(options, fallbackLabel !== undefined ? fallbackLabel : "")
        combo.currentIndex = index >= 0 ? index : 0
    }

    function updateDynamicFilters(snapshot) {
        var sourceSet = {}
        sourceSet["mqtt"] = true
        if (snapshot.sources !== undefined) {
            for (var i = 0; i < snapshot.sources.length; ++i) {
                var sourceValue = String(snapshot.sources[i]).toLowerCase().trim()
                if (sourceValue.length > 0)
                    sourceSet[sourceValue] = true
            }
        }
        var entries = snapshot.entries !== undefined ? snapshot.entries : []
        for (var s = 0; s < entries.length; ++s) {
            var normalized = normalizeEntry(entries[s])
            var logical = String(normalized.logicalSource || "").toLowerCase().trim()
            if (logical.length > 0)
                sourceSet[logical] = true
        }
        var sourceOptions = [{ "value": "__all__", "label": qsTr("All sources") }]
        for (var key in sourceSet) {
            if (Object.prototype.hasOwnProperty.call(sourceSet, key))
                sourceOptions.push({ "value": key, "label": key })
        }
        sourceOptions.sort(function(a, b) {
            if (String(a.value) === "__all__")
                return -1
            if (String(b.value) === "__all__")
                return 1
            return String(a.label).localeCompare(String(b.label))
        })

        var componentOptions = [{ "value": "__all__", "label": "component:*" }]
        if (snapshot.components !== undefined) {
            for (var c = 0; c < snapshot.components.length; ++c) {
                var componentValue = String(snapshot.components[c])
                if (componentValue.length > 0) {
                    componentOptions.push({ "value": componentValue, "label": componentValue })
                }
            }
        }

        var eventOptions = [{ "value": "__all__", "label": "event:*" }]
        if (snapshot.events !== undefined) {
            for (var e = 0; e < snapshot.events.length; ++e) {
                var eventValue = String(snapshot.events[e])
                if (eventValue.length > 0) {
                    eventOptions.push({ "value": eventValue, "label": eventValue })
                }
            }
        }

        updateComboOptions(logSourceFilter, sourceOptions, "__all__")
        updateComboOptions(logComponentFilter, componentOptions, "__all__")
        updateComboOptions(logEventFilter, eventOptions, "__all__")
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
            root.statusText = qsTr("Log loading error: ") + String(snapshot.message)
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
        var selectedSource = comboSelectedValue(logSourceFilter, "__all__")
        var selectedComponent = comboSelectedValue(logComponentFilter, "__all__")
        var selectedEvent = comboSelectedValue(logEventFilter, "__all__")
        var exactOpId = logOpIdFilter.text.trim()
        var queryText = logQueryFilter.text.trim().toLowerCase()
        var requiredRank = minLevelRank()

        var rendered = []
        for (var i = 0; i < root.allEntries.length; ++i) {
            var entry = root.allEntries[i]
            if (levelRank(entry.level) < requiredRank) continue
            if (selectedSource !== "__all__"
                    && String(entry.logicalSource || "") !== selectedSource) continue
            if (selectedComponent !== "__all__" && entry.component !== selectedComponent) continue
            if (selectedEvent !== "__all__" && entry.event !== selectedEvent) continue
            if (exactOpId.length > 0 && entry.opId !== exactOpId) continue

            if (queryText.length > 0) {
                var lineLower = String(entry.formatted).toLowerCase()
                if (lineLower.indexOf(queryText) === -1) continue
            }
            rendered.push(String(entry.formatted))
        }

        root.shownCount = rendered.length
        logsArea.text = rendered.join("\n")

        var modeLabel = root.demoMode ? qsTr("Demo mode") : qsTr("Live mode")
        root.statusText = modeLabel
                 + qsTr(" • ") + root.shownCount + "/" + root.totalCount + qsTr(" line(s)")
                 + qsTr(" • ") + (logSourceFilter.count - 1) + qsTr(" source(s)")
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
            text: qsTr("Runtime Logs")
            color: Theme.fgPrimary
            font.pixelSize: 26
            font.bold: true
        }

        Text {
            text: qsTr("Multi-source tail with level/source/component/event/op_id filters + text search.")
            color: Theme.fgSecondary
            font.pixelSize: 14
        }

        Rectangle {
            objectName: "logFiltersPanel"
            Layout.fillWidth: true
            Layout.preferredHeight: 116
            radius: Theme.radiusDialog
            color: Theme.bgSurface
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    AppComboBox {
                        id: logLevelFilter
                        objectName: "logLevelFilter"
                        textRole: "label"
                        model: [
                            { "value": "all", "label": "ALL" },
                            { "value": "info_plus", "label": "INFO+" },
                            { "value": "warn_plus", "label": "WARN+" },
                            { "value": "error", "label": "ERROR" }
                        ]
                        Layout.preferredWidth: 105
                        onCurrentIndexChanged: root.applyFilters()
                    }

                    AppComboBox {
                        id: logSourceFilter
                        objectName: "logSourceFilter"
                        textRole: "label"
                        model: [{ "value": "__all__", "label": qsTr("All sources") }]
                        Layout.preferredWidth: 150
                        onCurrentIndexChanged: root.applyFilters()
                    }

                    AppComboBox {
                        id: logComponentFilter
                        objectName: "logComponentFilter"
                        textRole: "label"
                        model: [{ "value": "__all__", "label": "component:*" }]
                        Layout.preferredWidth: 170
                        onCurrentIndexChanged: root.applyFilters()
                    }

                    AppComboBox {
                        id: logEventFilter
                        objectName: "logEventFilter"
                        textRole: "label"
                        model: [{ "value": "__all__", "label": "event:*" }]
                        Layout.preferredWidth: 170
                        onCurrentIndexChanged: root.applyFilters()
                    }

                    AppTextField {
                        id: logOpIdFilter
                        objectName: "logOpIdFilter"
                        placeholderText: qsTr("op_id exact")
                        Layout.preferredWidth: 160
                        onTextChanged: root.applyFilters()
                    }

                    AppTextField {
                        id: logQueryFilter
                        objectName: "logQueryFilter"
                        placeholderText: qsTr("query contains")
                        Layout.fillWidth: true
                        onTextChanged: root.applyFilters()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    AppButton {
                        id: logRefreshButton
                        objectName: "logRefreshButton"
                        text: root.loading ? qsTr("Refresh…") : qsTr("Refresh")
                        enabled: !root.loading
                        onClicked: root.refreshLogs()
                    }

                    AppButton {
                        id: logClearFiltersButton
                        objectName: "logClearFiltersButton"
                        text: qsTr("Reset filters")
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
                        color: Theme.fgSecondary
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
            radius: Theme.radiusDialog
            color: Theme.bgDialog
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

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
                    placeholderText: qsTr("No log lines for current filters.")
                    background: null
                }
            }
        }
    }
}
