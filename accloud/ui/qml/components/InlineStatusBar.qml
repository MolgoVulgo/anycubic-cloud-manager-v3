import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Rectangle {
    id: root

    property string message: ""
    property string operationId: ""
    property string severity: "info" // info | success | warn | error

    function isDarkTheme() {
        return Theme.themeName === "Dark"
    }

    function colorSet() {
        function toned(baseColor) {
            if (isDarkTheme()) {
                return {
                    "bg": Qt.darker(baseColor, 2.8),
                    "border": Qt.darker(baseColor, 1.9)
                }
            }
            return {
                "bg": Qt.lighter(baseColor, 1.85),
                "border": Qt.lighter(baseColor, 1.45)
            }
        }

        if (severity === "success") {
            var successTones = toned(Theme.success)
            return { "bg": successTones.bg, "border": successTones.border, "dot": Theme.success }
        }
        if (severity === "warn") {
            var warningTones = toned(Theme.warning)
            return { "bg": warningTones.bg, "border": warningTones.border, "dot": Theme.warning }
        }
        if (severity === "error") {
            var dangerTones = toned(Theme.danger)
            return { "bg": dangerTones.bg, "border": dangerTones.border, "dot": Theme.danger }
        }
        var infoTones = toned(Theme.accent)
        return { "bg": infoTones.bg, "border": infoTones.border, "dot": Theme.accent }
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
            visible: root.operationId.length > 0
            text: root.operationId
            color: Theme.fgSecondary
            opacity: 0.9
            font.family: "JetBrains Mono"
            font.pixelSize: Theme.fontCaptionPx
            verticalAlignment: Text.AlignVCenter
        }
    }
}
