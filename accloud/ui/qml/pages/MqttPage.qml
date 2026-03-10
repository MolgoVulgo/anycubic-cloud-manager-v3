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
    property string prefillInfo: ""
    property bool prefillApplied: false
    property var missingFields: []

    function applySuggestedConnection(force) {
        if (typeof mqttBridge === "undefined" || mqttBridge === null)
            return
        if (typeof mqttBridge.suggestedConnection !== "function")
            return
        var suggested = mqttBridge.suggestedConnection()
        prefillInfo = String(suggested.message || suggested.code || "")
        missingFields = suggested.missingFields !== undefined ? suggested.missingFields : []
        emailValue.text = String(suggested.email || "-")
        userIdValue.text = String(suggested.userId || "-")
        tokenValue.text = String(suggested.authToken || "-")
        caPathValue.text = String(suggested.caPath || "-")
        clientCertPathValue.text = String(suggested.clientCertPath || "-")
        clientKeyPathValue.text = String(suggested.clientKeyPath || "-")

        var hasHost = String(hostField.text || "").trim().length > 0
        var hasTopics = String(topicsField.text || "").trim().length > 0
        var shouldApply = force === true || !hasHost || !hasTopics
        if (!shouldApply)
            return

        hostField.text = String(suggested.host || "mqtt-universe.anycubic.com")
        var portValue = Number(suggested.port)
        portField.value = isFinite(portValue) && portValue > 0 ? Math.round(portValue) : 8883
        tlsCheck.checked = Boolean(suggested.useTls !== false)
        topicsField.text = String(suggested.topics || "anycubic/anycubicCloud/v1/#")
        prefillApplied = true
    }

    function mqttModeLabel() {
        if (typeof uiSettingsBridge === "undefined" || uiSettingsBridge === null)
            return qsTr("Slicer")
        if (typeof uiSettingsBridge.getString !== "function")
            return qsTr("Slicer")
        var mode = String(uiSettingsBridge.getString("mqtt.authMode", "slicer")).toLowerCase().trim()
        return mode === "android" ? qsTr("Android") : qsTr("Slicer")
    }

    AppPageFrame {
        anchors.fill: parent
        embeddedInTabsContainer: root.embeddedInTabsContainer
        sectionTitle: qsTr("MQTT")
        sectionSubtitle: qsTr("Raw realtime stream")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.gapRow

            SectionHeader {
                Layout.fillWidth: true
                title: qsTr("Auth Mode")
                subtitle: qsTr("Default is Slicer; change from Settings menu")
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
                        status: root.mqttModeLabel()
                    }

                    Item { Layout.fillWidth: true }

                    StatusChip {
                        status: root.statusText
                    }
                }
            }

            SectionHeader {
                Layout.fillWidth: true
                title: qsTr("Raw Connection")
                subtitle: qsTr("Fields are auto-prefilled from current session before connect")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gapRow

                AppTextField {
                    id: hostField
                    objectName: "mqttHostField"
                    Layout.fillWidth: true
                    text: "mqtt-universe.anycubic.com"
                    placeholderText: qsTr("Host")
                }
                AppSpinBox {
                    id: portField
                    objectName: "mqttPortField"
                    from: 1
                    to: 65535
                    value: 8883
                }
                AppCheckBox {
                    id: tlsCheck
                    objectName: "mqttTlsCheck"
                    text: qsTr("TLS")
                    checked: true
                }
            }

            AppTextField {
                id: topicsField
                objectName: "mqttTopicsField"
                Layout.fillWidth: true
                text: "anycubic/anycubicCloud/v1/#"
                placeholderText: qsTr("Topics (comma separated)")
            }

            Text {
                Layout.fillWidth: true
                text: root.prefillInfo.length > 0
                      ? qsTr("Prefill: %1").arg(root.prefillInfo)
                      : qsTr("Prefill: waiting")
                color: Theme.fgSecondary
                font.pixelSize: Theme.fontBodyPx
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: root.missingFields && root.missingFields.length > 0
                text: qsTr("Missing fields: %1").arg(String(root.missingFields.join(", ")))
                color: Theme.warning
                font.pixelSize: Theme.fontBodyPx
                wrapMode: Text.WordWrap
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.gapRow
                rowSpacing: 4

                Text {
                    text: qsTr("Email")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontBodyPx
                }
                Text {
                    id: emailValue
                    text: "-"
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Text {
                    text: qsTr("User ID")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontBodyPx
                }
                Text {
                    id: userIdValue
                    text: "-"
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Text {
                    text: qsTr("Token")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontBodyPx
                }
                Text {
                    id: tokenValue
                    text: "-"
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Text {
                    text: qsTr("CA cert")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontBodyPx
                }
                Text {
                    id: caPathValue
                    text: "-"
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Text {
                    text: qsTr("Client cert")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontBodyPx
                }
                Text {
                    id: clientCertPathValue
                    text: "-"
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Text {
                    text: qsTr("Client key")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontBodyPx
                }
                Text {
                    id: clientKeyPathValue
                    text: "-"
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
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
                        if (!root.prefillApplied)
                            root.applySuggestedConnection(true)
                        mqttBridge.connectRaw(hostField.text,
                                              portField.value,
                                              "",
                                              "",
                                              "",
                                              topicsField.text,
                                              tlsCheck.checked)
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
                text: qsTr("Raw stream")
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontSectionPx
                font.bold: true
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

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                TextArea {
                    readOnly: true
                    wrapMode: TextEdit.NoWrap
                    text: (typeof mqttBridge !== "undefined" && mqttBridge !== null)
                          ? String(mqttBridge.rawBuffer || "")
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
        }
    }

    Connections {
        target: (typeof mqttBridge !== "undefined") ? mqttBridge : null
        function onStatusChanged() {
            root.statusText = String(mqttBridge.status || "idle")
            if (!root.prefillApplied)
                root.applySuggestedConnection(false)
        }
    }

    Component.onCompleted: root.applySuggestedConnection(false)
}
