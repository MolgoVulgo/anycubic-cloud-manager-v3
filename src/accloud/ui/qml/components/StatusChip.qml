import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Rectangle {
    id: root

    property string status: "Offline"
    property int horizontalPadding: 10
    property int dotSize: 8
    property int spacingValue: 6

    function normalized() {
        return String(status).toLowerCase()
    }

    function toneColor() {
        var state = normalized()
        if (state === "offline")
            return Theme.fgSecondary
        if (state === "ready" || state === "free" || state === "online")
            return Theme.success
        if (state === "printing")
            return Theme.accent
        if (state === "error")
            return Theme.danger
        return Theme.fgSecondary
    }

    function chipBackground() {
        function tone(baseColor) {
            return Theme.themeName === "Dark"
                    ? Qt.darker(baseColor, 2.6)
                    : Qt.lighter(baseColor, 1.9)
        }

        var state = normalized()
        if (state === "offline")
            return tone(Theme.fgSecondary)
        if (state === "ready" || state === "free" || state === "online")
            return tone(Theme.success)
        if (state === "printing")
            return tone(Theme.accent)
        if (state === "error")
            return tone(Theme.danger)
        return tone(Theme.fgSecondary)
    }

    radius: Theme.radiusControl
    implicitHeight: Math.max(26, chipText.implicitHeight + 10)
    implicitWidth: chipText.implicitWidth + horizontalPadding * 2 + dotSize + spacingValue
    color: chipBackground()
    border.width: Theme.borderWidth
    border.color: toneColor()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.horizontalPadding
        anchors.rightMargin: root.horizontalPadding
        spacing: root.spacingValue

        Rectangle {
            Layout.preferredWidth: root.dotSize
            Layout.preferredHeight: root.dotSize
            radius: 4
            color: root.toneColor()
        }

        Text {
            id: chipText
            text: root.status
            color: root.toneColor()
            font.pixelSize: Theme.fontCaptionPx
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }
    }
}
