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
    property color inactiveColor: Theme.bgWindow
    property color baselineColor: Theme.borderDefault
    property int tabGap: tabLook === "classic" ? 0 : 4
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

    background: Item {
        Rectangle {
            anchors.fill: parent
            color: "transparent"
        }

        Rectangle {
            // Base separator line, interrupted by active tab bottom edge.
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Theme.borderWidth
            color: root.baselineColor
        }
    }
}
