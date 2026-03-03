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

    radius: 10
    implicitHeight: 52

    color: severity === "danger" ? "#f6dddd" : (severity === "ok" ? "#ddf1e2" : "#f9edd2")
    border.width: 1
    border.color: severity === "danger" ? Theme.danger : (severity === "ok" ? Theme.ok : Theme.warn)

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Rectangle {
            width: 10
            height: 10
            radius: 5
            color: severity === "danger" ? Theme.danger : (severity === "ok" ? Theme.ok : Theme.warn)
        }

        Text {
            Layout.fillWidth: true
            text: message
            color: Theme.textPrimary
            elide: Text.ElideRight
        }

        Text {
            visible: operationId.length > 0
            text: operationId
            color: Theme.textSecondary
            font.family: "JetBrains Mono"
        }
    }
}
