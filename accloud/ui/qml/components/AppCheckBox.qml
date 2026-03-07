import QtQuick 2.15
import QtQuick.Controls 2.15
import "Theme.js" as Theme

CheckBox {
    id: root

    spacing: 8
    focusPolicy: Qt.TabFocus
    font.pixelSize: Theme.fontBodyPx

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        radius: 4
        color: Theme.bgSurface
        border.width: Theme.borderWidth
        border.color: root.activeFocus ? Theme.accent : Theme.borderDefault

        Text {
            anchors.centerIn: parent
            visible: root.checked
            text: "✓"
            color: Theme.accent
            font.pixelSize: Theme.fontCaptionPx
            font.bold: true
        }
    }

    contentItem: Text {
        text: root.text
        leftPadding: root.indicator.width + root.spacing
        color: root.enabled ? Theme.fgPrimary : Theme.fgDisabled
        font: root.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
