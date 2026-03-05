import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Rectangle {
    id: root

    property string status: "Offline"

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
        var state = normalized()
        if (state === "offline")
            return Theme.themeName === "Dark" ? "#343a43" : "#ece7dd"
        if (state === "ready" || state === "free" || state === "online")
            return Theme.themeName === "Dark" ? "#223429" : "#e7f4ea"
        if (state === "printing")
            return Theme.themeName === "Dark" ? "#203741" : "#e3f1f4"
        if (state === "error")
            return Theme.themeName === "Dark" ? "#3a2426" : "#f8e3e3"
        return Theme.themeName === "Dark" ? "#343a43" : "#ece7dd"
    }

    radius: Theme.radiusControl
    implicitHeight: 26
    implicitWidth: chipText.implicitWidth + 20
    color: chipBackground()
    border.width: Theme.borderWidth
    border.color: toneColor()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 6

        Rectangle {
            Layout.preferredWidth: 8
            Layout.preferredHeight: 8
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
