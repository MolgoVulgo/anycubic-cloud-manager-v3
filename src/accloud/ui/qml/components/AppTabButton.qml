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
    readonly property color baselineColor: (ownerTabBar && ownerTabBar.baselineColor) ? ownerTabBar.baselineColor : Theme.tabBaselineColor
    readonly property bool connectActiveToPanel: !!(ownerTabBar && ownerTabBar.connectActiveToPanel)
    readonly property int topCornerRadius: (ownerTabBar && ownerTabBar.tabTopCornerRadius)
                                         ? Math.max(0, Number(ownerTabBar.tabTopCornerRadius))
                                         : Theme.radiusControl
    readonly property int strokeWidth: Theme.tabStrokeWidth
    readonly property int baseImplicitWidth: Math.max(92, contentItem.implicitWidth + leftPadding + rightPadding)
    readonly property bool lastVisibleTab: {
        if (!ownerTabBar)
            return true
        for (var i = ownerTabBar.contentChildren.length - 1; i >= 0; --i) {
            var child = ownerTabBar.contentChildren[i]
            if (!child || child.visible === false || child.checked === undefined)
                continue
            return child === root
        }
        return true
    }
    readonly property int verticalBorderBottomMargin: root.strokeWidth

    implicitWidth: tabSizingMode === "equal" ? computedEqualWidth : baseImplicitWidth
    implicitHeight: Theme.controlHeight + 2
    leftPadding: 18
    rightPadding: 18
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    topPadding: 6
    bottomPadding: tabLook === "classic" ? 7 : 6
    z: checked ? 2 : 1

    font.pixelSize: Theme.fontBodyPx
    font.bold: checked

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    function backgroundColor() {
        if (checked)
            return panelColor
        if (hovered)
            return Qt.lighter(inactiveColor, 1.01)
        return inactiveColor
    }

    function foregroundColor() {
        if (checked)
            return Theme.fgPrimary
        return Theme.fgSecondary
    }

    function strokeColor() {
        if (checked || hovered || activeFocus)
            return Theme.tabStrokeColor
        return baselineColor
    }

    background: Item {
        Rectangle {
            id: tabFill
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.bottomMargin: root.strokeWidth
            radius: root.topCornerRadius
            antialiasing: true
            color: root.backgroundColor()
        }

        Rectangle {
            // Flatten lower corners so the tab reads as a strip-attached tongue.
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: root.strokeWidth
            height: Math.max(1, root.topCornerRadius)
            color: tabFill.color
        }

        Canvas {
            id: tabStrokeCanvas
            anchors.fill: parent
            antialiasing: true

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                var sw = Math.max(1, root.strokeWidth)
                var half = sw / 2.0
                var w = width
                var h = height
                var sideBottom = h - root.verticalBorderBottomMargin - half
                var radius = Math.max(0, Math.min(root.topCornerRadius, (w / 2.0) - half, sideBottom - half))
                var left = half
                var right = w - half
                var top = half

                ctx.lineWidth = sw
                ctx.strokeStyle = root.strokeColor()
                ctx.beginPath()
                ctx.moveTo(left, sideBottom)
                ctx.lineTo(left, top + radius)
                if (radius > 0) {
                    ctx.arc(left + radius, top + radius, radius, Math.PI, Math.PI * 1.5, false)
                    ctx.lineTo(right - radius, top)
                    ctx.arc(right - radius, top + radius, radius, Math.PI * 1.5, 0, false)
                } else {
                    ctx.lineTo(right, top)
                }
                if (root.lastVisibleTab)
                    ctx.lineTo(right, sideBottom)
                ctx.stroke()
            }

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: root.strokeWidth
            color: root.checked && root.connectActiveToPanel
                   ? root.panelColor
                   : "transparent"
        }
    }

    onCheckedChanged: tabStrokeCanvas.requestPaint()
    onHoveredChanged: tabStrokeCanvas.requestPaint()
    onActiveFocusChanged: tabStrokeCanvas.requestPaint()
    onLastVisibleTabChanged: tabStrokeCanvas.requestPaint()
    onTopCornerRadiusChanged: tabStrokeCanvas.requestPaint()
    onBaselineColorChanged: tabStrokeCanvas.requestPaint()

    contentItem: Text {
        text: root.text
        color: root.foregroundColor()
        font: root.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
