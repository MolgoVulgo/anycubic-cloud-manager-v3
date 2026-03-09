import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Rectangle {
    id: root
    objectName: "errorBanner"
    property string message: ""
    property string operationId: ""
    property string severity: "warn"

    function toneColor() {
        return severity === "danger" ? Theme.danger : (severity === "ok" ? Theme.success : Theme.warning)
    }

    function backgroundTone() {
        return Theme.themeName === "Dark"
                ? Qt.darker(toneColor(), 2.8)
                : Qt.lighter(toneColor(), 1.9)
    }

    radius: 10
    implicitHeight: 52

    color: backgroundTone()
    border.width: 1
    border.color: toneColor()

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Rectangle {
            width: 10
            height: 10
            radius: 5
            color: toneColor()
        }

        Text {
            Layout.fillWidth: true
            text: message
            color: Theme.fgPrimary
            font.pixelSize: Theme.fontBodyPx
            elide: Text.ElideRight
        }

        Text {
            visible: operationId.length > 0
            text: operationId
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontCaptionPx
            font.family: "JetBrains Mono"
        }
    }
}
