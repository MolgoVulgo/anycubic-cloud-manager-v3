import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Rectangle {
    id: root

    property string message: ""
    property string operationId: ""
    property string severity: "info" // info | success | warn | error
    property bool showOperationId: false

    function colorSet() {
        if (severity === "success")
            return { "bg": Theme.statusSuccessBg, "border": Theme.stateSuccess, "dot": Theme.stateSuccess }
        if (severity === "warn")
            return { "bg": Theme.statusWarningBg, "border": Theme.stateWarning, "dot": Theme.stateWarning }
        if (severity === "error")
            return { "bg": Theme.statusErrorBg, "border": Theme.stateError, "dot": Theme.stateError }
        return { "bg": Theme.statusInfoBg, "border": Theme.stateRunning, "dot": Theme.stateRunning }
    }

    readonly property var tones: colorSet()

    radius: Theme.radiusControl
    implicitHeight: Math.max(32, Theme.controlHeight - 2)
    color: tones.bg
    border.width: Theme.borderWidth
    border.color: tones.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        Rectangle {
            Layout.preferredWidth: 10
            Layout.preferredHeight: 10
            radius: 5
            color: root.tones.dot
        }

        Text {
            Layout.fillWidth: true
            text: root.message
            color: Theme.fgPrimary
            font.pixelSize: Theme.fontBodyPx
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            objectName: "inlineStatusOperationId"
            visible: root.showOperationId && root.operationId.length > 0
            text: root.operationId
            color: Theme.textMuted
            opacity: 0.9
            font.family: "JetBrains Mono"
            font.pixelSize: Theme.fontCaptionPx
            verticalAlignment: Text.AlignVCenter
        }
    }
}
