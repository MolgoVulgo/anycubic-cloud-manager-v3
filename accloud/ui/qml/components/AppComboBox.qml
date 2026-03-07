import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

ComboBox {
    id: root

    implicitHeight: Theme.controlHeight
    leftPadding: 10
    rightPadding: 30
    focusPolicy: Qt.TabFocus
    font.pixelSize: Theme.fontBodyPx

    contentItem: Text {
        leftPadding: root.leftPadding
        rightPadding: root.indicator.width + root.spacing
        text: root.displayText
        font: root.font
        color: root.enabled ? Theme.fgPrimary : Theme.fgDisabled
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        text: qsTr("v")
        color: root.enabled ? Theme.fgSecondary : Theme.fgDisabled
        font.pixelSize: Theme.fontCaptionPx
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.bgSurface
        border.width: Theme.borderWidth
        border.color: root.activeFocus ? Theme.accent : Theme.borderDefault
    }

    delegate: ItemDelegate {
        width: ListView.view ? ListView.view.width : root.width
        highlighted: root.highlightedIndex === index
        hoverEnabled: true
        font.pixelSize: Theme.fontBodyPx

        function delegateText() {
            if (typeof modelData === "string")
                return modelData
            if (modelData !== null && modelData !== undefined) {
                if (root.textRole.length > 0 && modelData[root.textRole] !== undefined)
                    return modelData[root.textRole]
                if (modelData.text !== undefined)
                    return modelData.text
                return String(modelData)
            }
            return ""
        }

        contentItem: Text {
            text: parent.delegateText()
            color: parent.highlighted ? Theme.selectionFg : Theme.fgPrimary
            font: parent.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: parent.highlighted ? Theme.selectionBg : Theme.bgSurface
            border.width: 0
        }
    }

    popup: Popup {
        y: root.height + 4
        width: root.width
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)

        background: Rectangle {
            radius: Theme.radiusControl
            color: Theme.bgDialog
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault
        }

        contentItem: ListView {
            clip: true
            model: root.delegateModel
            currentIndex: root.highlightedIndex
            implicitHeight: contentHeight
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }
    }
}
