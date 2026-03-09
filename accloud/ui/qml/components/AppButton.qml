import QtQuick 2.15
import QtQuick.Controls 2.15
import "Theme.js" as Theme

Button {
    id: root

    property string variant: "secondary" // primary | secondary | danger
    property bool compact: false

    implicitHeight: compact ? Math.max(28, Theme.controlHeight - 8) : Theme.controlHeight
    implicitWidth: compact ? 36 : 96
    padding: compact ? 8 : 12
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    font.pixelSize: Theme.fontBodyPx
    font.bold: variant === "primary" || variant === "danger"

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    function backgroundColor() {
        if (!enabled)
            return Theme.bgSurface
        if (variant === "primary")
            return down ? Qt.darker(Theme.accent, 1.08) : (hovered ? Qt.lighter(Theme.accent, 1.04) : Theme.accent)
        if (variant === "danger")
            return down ? Qt.darker(Theme.danger, 1.08) : (hovered ? Qt.lighter(Theme.danger, 1.04) : Theme.danger)
        return down ? Qt.darker(Theme.bgSurface, 1.05) : (hovered ? Qt.lighter(Theme.bgSurface, 1.02) : Theme.bgSurface)
    }

    function borderColor() {
        if (activeFocus)
            return Theme.accent
        if (variant === "primary")
            return Qt.darker(Theme.accent, 1.15)
        if (variant === "danger")
            return Qt.darker(Theme.danger, 1.15)
        return Theme.borderDefault
    }

    function foregroundColor() {
        if (!enabled)
            return Theme.fgDisabled
        if (variant === "primary")
            return Theme.accentFg
        if (variant === "danger")
            return Theme.fgOnDanger
        return Theme.fgPrimary
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: root.backgroundColor()
        border.width: Theme.borderWidth
        border.color: root.borderColor()
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
