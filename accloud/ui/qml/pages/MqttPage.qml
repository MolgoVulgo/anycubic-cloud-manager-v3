import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Item {
    id: root
    property bool embeddedInTabsContainer: false
    property string statusText: (typeof mqttBridge !== "undefined" && mqttBridge !== null)
                                ? String(mqttBridge.status || "idle")
                                : "mqtt_unavailable"
    property string topicFilter: ""
    property string selectedTopic: ""
    property var topicSelectorModel: [qsTr("All topics")]

    function filteredRawStream() {
        var raw = (typeof mqttBridge !== "undefined" && mqttBridge !== null)
                ? String(mqttBridge.rawBuffer || "")
                : ""
        var filter = String(root.topicFilter || "").trim().toLowerCase()
        if (filter.length === 0)
            return raw

        var lines = raw.split("\n")
        var out = []
        var currentBlock = []
        var currentBlockMatches = false

        function flushCurrentBlock() {
            if (currentBlock.length === 0)
                return
            if (currentBlockMatches)
                out.push(currentBlock.join("\n"))
            currentBlock = []
            currentBlockMatches = false
        }

        for (var i = 0; i < lines.length; ++i) {
            var line = String(lines[i] || "")
            var lowered = line.toLowerCase()
            var hasTopicHeader = lowered.indexOf("topic=") !== -1

            if (hasTopicHeader) {
                flushCurrentBlock()
                currentBlock = [line]
                currentBlockMatches = (lowered.indexOf("topic=" + filter) !== -1
                                       || lowered.indexOf(filter) !== -1)
                continue
            }

            if (currentBlock.length > 0) {
                currentBlock.push(line)
                continue
            }

            if (lowered.indexOf(filter) !== -1)
                out.push(line)
        }
        flushCurrentBlock()
        return out.join("\n")
    }

    function rebuildTopicSelectorModel() {
        var previous = String(root.selectedTopic || "")
        var model = [qsTr("All topics")]
        if (typeof mqttBridge !== "undefined" && mqttBridge !== null) {
            var topics = mqttBridge.receivedTopics || []
            for (var i = 0; i < topics.length; ++i) {
                model.push(String(topics[i]))
            }
        }
        root.topicSelectorModel = model

        var index = 0
        if (previous.length > 0) {
            for (var j = 1; j < model.length; ++j) {
                if (String(model[j]) === previous) {
                    index = j
                    break
                }
            }
        }

        root.selectedTopic = index > 0 ? String(model[index]) : ""
        if (topicPicker.currentIndex !== index)
            topicPicker.currentIndex = index
    }

    function selectedTopicMessages() {
        if (typeof mqttBridge === "undefined" || mqttBridge === null)
            return ""
        var _tick = mqttBridge.messageTick
        return String(mqttBridge.messagesForTopic(root.selectedTopic))
    }

    AppPageFrame {
        anchors.fill: parent
        embeddedInTabsContainer: root.embeddedInTabsContainer
        sectionTitle: qsTr("MQTT")
        sectionSubtitle: qsTr("Raw realtime stream")

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.gapRow

            SectionHeader {
                Layout.fillWidth: true
                title: qsTr("Auth Mode")
                subtitle: qsTr("Slicer is enforced in runtime")
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                radius: Theme.radiusControl
                color: Theme.bgSurface
                border.width: Theme.borderWidth
                border.color: Theme.borderDefault

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: Theme.gapRow

                    Text {
                        text: qsTr("Current mode:")
                        color: Theme.fgSecondary
                        font.pixelSize: Theme.fontBodyPx
                    }

                    StatusChip {
                        status: qsTr("Slicer")
                    }

                    Item { Layout.fillWidth: true }

                    StatusChip {
                        status: root.statusText
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gapRow

                AppButton {
                    text: qsTr("Connect")
                    variant: "primary"
                    onClicked: {
                        if (typeof mqttBridge === "undefined" || mqttBridge === null)
                            return
                        mqttBridge.connectRaw("", 0, "", "", "", "", true)
                    }
                }

                AppButton {
                    text: qsTr("Disconnect")
                    variant: "secondary"
                    onClicked: {
                        if (typeof mqttBridge === "undefined" || mqttBridge === null)
                            return
                        mqttBridge.disconnectRaw()
                    }
                }

                AppButton {
                    text: qsTr("Clear")
                    variant: "secondary"
                    onClicked: {
                        if (typeof mqttBridge === "undefined" || mqttBridge === null)
                            return
                        mqttBridge.clearRaw()
                    }
                }

                Item { Layout.fillWidth: true }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Subscribed topics")
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontSectionPx
                font.bold: true
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 110
                clip: true
                TextArea {
                    readOnly: true
                    wrapMode: TextEdit.NoWrap
                    text: (typeof mqttBridge !== "undefined" && mqttBridge !== null)
                          ? String(mqttBridge.subscribedTopics || "")
                          : ""
                    placeholderText: qsTr("No topic subscribed yet")
                    color: Theme.fgPrimary
                    background: Rectangle {
                        color: Theme.bgSurface
                        border.width: Theme.borderWidth
                        border.color: Theme.borderDefault
                        radius: Theme.radiusControl
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Messages by topic")
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontSectionPx
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gapRow

                AppComboBox {
                    id: topicPicker
                    Layout.fillWidth: true
                    model: root.topicSelectorModel
                    onActivated: {
                        if (currentIndex <= 0) {
                            root.selectedTopic = ""
                        } else {
                            root.selectedTopic = String(root.topicSelectorModel[currentIndex] || "")
                        }
                    }
                }

                Text {
                    text: qsTr("Topics: %1").arg(Math.max(0, root.topicSelectorModel.length - 1))
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontBodyPx
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                clip: true
                TextArea {
                    readOnly: true
                    wrapMode: TextEdit.NoWrap
                    text: root.selectedTopicMessages()
                    placeholderText: root.selectedTopic.length > 0
                                     ? qsTr("No message received yet for this topic")
                                     : qsTr("No message received yet")
                    color: Theme.fgPrimary
                    background: Rectangle {
                        color: Theme.bgSurface
                        border.width: Theme.borderWidth
                        border.color: Theme.borderDefault
                        radius: Theme.radiusControl
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Telemetry")
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontSectionPx
                font.bold: true
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 140
                clip: true
                TextArea {
                    readOnly: true
                    wrapMode: TextEdit.NoWrap
                    text: (typeof mqttBridge !== "undefined" && mqttBridge !== null)
                          ? String(mqttBridge.telemetrySnapshot || "")
                          : ""
                    color: Theme.fgPrimary
                    background: Rectangle {
                        color: Theme.bgSurface
                        border.width: Theme.borderWidth
                        border.color: Theme.borderDefault
                        radius: Theme.radiusControl
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gapRow

                Text {
                    text: qsTr("Raw stream")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontSectionPx
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                AppTextField {
                    id: topicFilterField
                    Layout.preferredWidth: 360
                    placeholderText: qsTr("Filter by topic")
                    text: root.topicFilter
                    onTextChanged: root.topicFilter = text
                }

                AppButton {
                    text: qsTr("Reset filter")
                    variant: "secondary"
                    onClicked: root.topicFilter = ""
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                TextArea {
                    readOnly: true
                    wrapMode: TextEdit.NoWrap
                    text: root.filteredRawStream()
                    color: Theme.fgPrimary
                    background: Rectangle {
                        color: Theme.bgSurface
                        border.width: Theme.borderWidth
                        border.color: Theme.borderDefault
                        radius: Theme.radiusControl
                    }
                }
            }
        }
    }

    Connections {
        target: (typeof mqttBridge !== "undefined") ? mqttBridge : null
        function onStatusChanged() {
            root.statusText = String(mqttBridge.status || "idle")
        }
        function onReceivedTopicsChanged() {
            root.rebuildTopicSelectorModel()
        }
        function onMessageTickChanged() {
            if (root.selectedTopic.length > 0) {
                root.rebuildTopicSelectorModel()
            }
        }
    }

    Component.onCompleted: root.rebuildTopicSelectorModel()
}
