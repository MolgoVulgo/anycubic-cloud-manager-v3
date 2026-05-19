import QtQuick 2.15
import QtQuick.Controls 2.15
import "Theme.js" as Theme

Slider {
    id: root

    implicitHeight: Theme.controlHeight
    focusPolicy: Qt.TabFocus

    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: 6
        radius: 3
        color: Theme.borderSubtle

        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: 3
            color: Theme.accent
        }
    }

    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        implicitWidth: 16
        implicitHeight: 16
        radius: 8
        color: root.pressed ? Qt.darker(Theme.accent, 1.08) : Theme.accent
        border.width: Theme.borderWidth
        border.color: Qt.darker(Theme.accent, 1.2)
    }
}
