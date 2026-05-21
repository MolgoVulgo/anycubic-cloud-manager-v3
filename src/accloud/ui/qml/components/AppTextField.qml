import QtQuick 2.15
import QtQuick.Controls 2.15
import "Theme.js" as Theme

TextField {
    id: root

    implicitHeight: Theme.controlHeight
    leftPadding: 10
    rightPadding: 10
    selectByMouse: true
    focusPolicy: Qt.TabFocus
    font.pixelSize: Theme.fontBodyPx
    color: enabled ? Theme.fgPrimary : Theme.fgDisabled
    placeholderTextColor: Theme.fgSecondary
    selectedTextColor: Theme.selectionFg
    selectionColor: Theme.selectionBg

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.bgSurface
        border.width: Theme.borderWidth
        border.color: root.activeFocus ? Theme.accent : Theme.borderDefault
    }
}
