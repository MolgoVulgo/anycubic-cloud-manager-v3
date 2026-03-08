import QtQuick 2.15
import QtQuick.Controls 2.15
import "Theme.js" as Theme

TabBar {
    id: root

    spacing: 6
    padding: 6

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.bgWindow
        border.width: Theme.borderWidth
        border.color: Theme.borderDefault
    }
}
