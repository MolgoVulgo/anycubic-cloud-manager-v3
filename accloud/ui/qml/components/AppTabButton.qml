import QtQuick 2.15
import QtQuick.Controls 2.15
import "Theme.js" as Theme

TabButton {
    id: root

    readonly property var ownerTabBar: TabBar.tabBar
    readonly property string tabVariant: (ownerTabBar && ownerTabBar.tabVariant) ? String(ownerTabBar.tabVariant) : "local"
    readonly property string tabLook: (ownerTabBar && ownerTabBar.tabLook) ? String(ownerTabBar.tabLook) : "classic"
    readonly property string tabSizingMode: (ownerTabBar && ownerTabBar.tabSizingMode) ? String(ownerTabBar.tabSizingMode) : "content"
    readonly property int computedEqualWidth: (ownerTabBar && ownerTabBar.equalTabWidth) ? Number(ownerTabBar.equalTabWidth) : 120
    readonly property color panelColor: (ownerTabBar && ownerTabBar.panelColor) ? ownerTabBar.panelColor : Theme.bgSurface
    readonly property color inactiveColor: (ownerTabBar && ownerTabBar.inactiveColor) ? ownerTabBar.inactiveColor : Theme.bgWindow
    readonly property color baselineColor: (ownerTabBar && ownerTabBar.baselineColor) ? ownerTabBar.baselineColor : Theme.borderDefault
    readonly property bool connectActiveToPanel: !!(ownerTabBar && ownerTabBar.connectActiveToPanel)
    readonly property int baseImplicitWidth: Math.max(92, contentItem.implicitWidth + leftPadding + rightPadding)

    implicitWidth: tabSizingMode === "equal" ? computedEqualWidth : baseImplicitWidth
    implicitHeight: Theme.controlHeight + 2
    leftPadding: tabVariant === "navigation" ? 16 : 14
    rightPadding: tabVariant === "navigation" ? 16 : 14
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    topPadding: 6
    bottomPadding: tabLook === "classic" ? 7 : 6

    font.pixelSize: Theme.fontBodyPx
    font.bold: checked

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    function backgroundColor() {
        if (checked)
            return panelColor
        if (hovered)
            return Qt.lighter(inactiveColor, 1.02)
        return inactiveColor
    }

    function foregroundColor() {
        if (checked)
            return Theme.fgPrimary
        return Theme.fgSecondary
    }

    background: Item {
        Rectangle {
            anchors.fill: parent
            radius: tabVariant === "navigation" ? Theme.radiusControl : (Theme.radiusControl - 1)
            color: root.backgroundColor()
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Theme.borderWidth
            color: root.checked ? Theme.borderDefault : root.baselineColor
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Theme.borderWidth
            color: root.checked || root.hovered || root.activeFocus ? Theme.borderDefault : root.baselineColor
        }

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Theme.borderWidth
            color: root.checked || root.hovered || root.activeFocus ? Theme.borderDefault : root.baselineColor
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Theme.borderWidth
            color: root.checked && root.connectActiveToPanel
                   ? root.panelColor
                   : root.baselineColor
        }
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
