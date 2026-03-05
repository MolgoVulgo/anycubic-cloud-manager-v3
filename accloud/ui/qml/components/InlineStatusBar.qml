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
        if (severity === "success") {
            return isDarkTheme()
                    ? { "bg": "#213328", "border": "#30553b", "dot": Theme.success }
                    : { "bg": "#e8f5ec", "border": "#b8dcc4", "dot": Theme.success }
        }
        if (severity === "warn") {
            return isDarkTheme()
                    ? { "bg": "#3a3121", "border": "#5f4e2a", "dot": Theme.warning }
                    : { "bg": "#faefd8", "border": "#e7c995", "dot": Theme.warning }
        }
        if (severity === "error") {
            return isDarkTheme()
                    ? { "bg": "#3a2426", "border": "#684245", "dot": Theme.danger }
                    : { "bg": "#f9e4e4", "border": "#e8bcbc", "dot": Theme.danger }
        }
        return isDarkTheme()
                ? { "bg": "#25313a", "border": "#345061", "dot": Theme.accent }
                : { "bg": "#e5f3f7", "border": "#bbdce6", "dot": Theme.accent }
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
