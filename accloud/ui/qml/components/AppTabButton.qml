import QtQuick 2.15
import QtQuick.Controls 2.15
import "Theme.js" as Theme

TabButton {
    id: root

    implicitHeight: Theme.controlHeight
    leftPadding: 14
    rightPadding: 14
    focusPolicy: Qt.TabFocus
    hoverEnabled: true

    font.pixelSize: Theme.fontBodyPx
    font.bold: checked

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    function backgroundColor() {
        if (checked)
            return Theme.accentSoft
        if (hovered)
            return Qt.lighter(Theme.bgSurface, 1.03)
        return Theme.bgSurface
    }

    function foregroundColor() {
        if (checked)
            return Theme.fgPrimary
        return Theme.fgSecondary
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: root.backgroundColor()
        border.width: Theme.borderWidth
        border.color: root.activeFocus || root.checked ? Theme.accent : Theme.borderDefault
    }

    contentItem: Text {
        text: root.text
        color: root.foregroundColor()
        font: root.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
