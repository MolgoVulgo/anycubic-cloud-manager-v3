import QtQuick 2.15
import QtQuick.Controls 2.15
import "Theme.js" as Theme

TabBar {
    id: root

    // "local" for compact section tabs, "navigation" for primary page tabs.
    property string tabVariant: "local"
    // "classic" renders a connected tab-strip style.
    property string tabLook: "classic"
    // "content" keeps intrinsic widths, "equal" normalizes widths across visible tabs.
    property string tabSizingMode: tabVariant === "navigation" ? "equal" : "content"
    property int minTabWidth: tabVariant === "navigation" ? 120 : 96
    property bool connectActiveToPanel: true
    property color panelColor: tabVariant === "navigation" ? Theme.bgSurface : Theme.bgDialog
    property color inactiveColor: panelColor
    property color baselineColor: Theme.tabBaselineColor
    property int baselineWidth: Theme.tabStrokeWidth
    property color stripColor: panelColor
    property int tabTopCornerRadius: Theme.radiusControl
    property int tabGap: tabLook === "classic" ? 0 : 4
    readonly property var _activeTabItem: (currentItem && currentItem.visible) ? currentItem : null
    readonly property bool _hasActiveTab: _activeTabItem !== null
    readonly property real _activeTabLeft: _activeTabItem ? Math.round(_activeTabItem.x) : -1
    readonly property real _activeTabRight: _activeTabItem ? Math.round(_activeTabItem.x + _activeTabItem.width) : -1
    readonly property int _visibleTabCount: {
        var count = 0
        for (var i = 0; i < contentChildren.length; ++i) {
            var child = contentChildren[i]
            if (!child || child.visible === false)
                continue
            if (child.checked !== undefined)
                count += 1
        }
        return Math.max(1, count)
    }
    readonly property int _innerWidth: Math.max(0, width - (padding * 2) - Math.max(0, _visibleTabCount - 1) * spacing)
    readonly property int equalTabWidth: Math.max(minTabWidth, Math.floor(_innerWidth / _visibleTabCount))

    spacing: tabGap
    padding: tabLook === "classic" ? 0 : (tabVariant === "navigation" ? 4 : 3)

    background: Rectangle {
        color: root.stripColor

        Rectangle {
            visible: root.tabLook === "classic"
            x: 0
            y: parent.height - root.baselineWidth
            width: root._hasActiveTab ? Math.max(0, root._activeTabLeft) : parent.width
            height: root.baselineWidth
            color: root.baselineColor
        }

        Rectangle {
            visible: root.tabLook === "classic"
            x: root._hasActiveTab ? root._activeTabRight : 0
            y: parent.height - root.baselineWidth
            width: root._hasActiveTab ? Math.max(0, parent.width - root._activeTabRight) : 0
            height: root.baselineWidth
            color: root.baselineColor
        }
    }
}
