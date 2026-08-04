import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Rectangle {
    id: root

    property int minimumLayer: 1
    property int maximumLayer: 1
    property int lowerLayer: 1
    property int upperLayer: 1

    signal lowerLayerMoved(int layer)
    signal upperLayerMoved(int layer)

    implicitWidth: 64
    implicitHeight: 260
    radius: Theme.radiusControl
    color: "#99000000"
    border.width: 1
    border.color: "#55777777"

    function upperLayerAfterWheel(currentUpper, lower, maximum, angleDeltaY) {
        if (angleDeltaY === 0)
            return currentUpper
        var stepCount = Math.max(1, Math.round(Math.abs(angleDeltaY) / 120.0))
        var direction = angleDeltaY > 0 ? 1 : -1
        return Math.max(lower, Math.min(maximum, currentUpper + direction * stepCount))
    }

    function applyWheel(angleDeltaY) {
        var nextLayer = upperLayerAfterWheel(root.upperLayer,
                                             root.lowerLayer,
                                             root.maximumLayer,
                                             angleDeltaY)
        if (nextLayer !== root.upperLayer)
            root.upperLayerMoved(nextLayer)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        Text {
            id: maximumLayerLabel
            objectName: "viewerLayerMaximumLabel"
            Layout.alignment: Qt.AlignHCenter
            text: String(root.maximumLayer)
            color: Theme.viewportFg
            font.pixelSize: Theme.fontCaptionPx
            font.bold: true
        }

        RangeSlider {
            id: layerRangeSlider
            objectName: "viewerLayerRangeSlider"
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 40
            orientation: Qt.Vertical
            from: root.minimumLayer
            to: Math.max(root.minimumLayer, root.maximumLayer)
            stepSize: 1
            snapMode: RangeSlider.SnapAlways
            enabled: root.maximumLayer >= root.minimumLayer

            first.value: Math.max(root.minimumLayer,
                                  Math.min(root.lowerLayer, root.maximumLayer))
            second.value: Math.max(first.value,
                                   Math.min(root.upperLayer, root.maximumLayer))

            first.onMoved: root.lowerLayerMoved(Math.round(first.value))
            second.onMoved: root.upperLayerMoved(Math.round(second.value))

            background: Rectangle {
                x: layerRangeSlider.leftPadding
                   + (layerRangeSlider.availableWidth - width) / 2
                y: layerRangeSlider.topPadding
                width: 6
                height: layerRangeSlider.availableHeight
                radius: 3
                color: "#66777777"
            }

            first.handle: Rectangle {
                id: lowerLayerHandle
                objectName: "viewerLowerLayerHandle"
                x: layerRangeSlider.leftPadding
                   + (layerRangeSlider.availableWidth - width) / 2
                y: layerRangeSlider.topPadding
                   + layerRangeSlider.first.visualPosition
                     * (layerRangeSlider.availableHeight - height)
                implicitWidth: 24
                implicitHeight: 24
                radius: width / 2
                color: layerRangeSlider.first.pressed
                       ? Qt.darker(Theme.accent, 1.08)
                       : Theme.accent
                border.width: Theme.borderWidth
                border.color: Qt.darker(Theme.accent, 1.2)

                HoverHandler {
                    id: lowerLayerHover
                }

                ToolTip.visible: lowerLayerHover.hovered
                ToolTip.delay: 0
                ToolTip.text: String(Math.round(layerRangeSlider.first.value))
            }

            second.handle: Rectangle {
                id: upperLayerHandle
                objectName: "viewerUpperLayerHandle"
                x: layerRangeSlider.leftPadding
                   + (layerRangeSlider.availableWidth - width) / 2
                y: layerRangeSlider.topPadding
                   + layerRangeSlider.second.visualPosition
                     * (layerRangeSlider.availableHeight - height)
                implicitWidth: 24
                implicitHeight: 24
                radius: width / 2
                color: layerRangeSlider.second.pressed
                       ? Qt.darker(Theme.accent, 1.08)
                       : Theme.accent
                border.width: Theme.borderWidth
                border.color: Qt.darker(Theme.accent, 1.2)

                HoverHandler {
                    id: upperLayerHover
                }

                ToolTip.visible: upperLayerHover.hovered
                ToolTip.delay: 0
                ToolTip.text: String(Math.round(layerRangeSlider.second.value))
            }

            MouseArea {
                id: upperLayerWheelArea
                objectName: "viewerUpperLayerWheelArea"
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                hoverEnabled: false
                onWheel: function(wheel) {
                    root.applyWheel(wheel.angleDelta.y)
                    wheel.accepted = true
                }
            }
        }

        Text {
            id: minimumLayerLabel
            objectName: "viewerLayerMinimumLabel"
            Layout.alignment: Qt.AlignHCenter
            text: String(root.minimumLayer)
            color: Theme.viewportFg
            font.pixelSize: Theme.fontCaptionPx
            font.bold: true
        }
    }
}
