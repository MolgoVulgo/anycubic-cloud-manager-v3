import QtQuick 2.15
import QtQuick.Controls 2.15
import "Theme.js" as Theme

Button {
    id: root

    property string variant: "secondary" // primary | secondary | danger
    property string disabledStatus: "" // optional status tone for a disabled control
    property bool compact: false

    implicitHeight: compact ? Math.max(28, Theme.controlHeight - 8) : Theme.controlHeight
    implicitWidth: Math.max(compact ? 36 : 96, buttonLabel.implicitWidth + padding * 2)
    padding: compact ? 8 : 12
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    font.pixelSize: Theme.fontBodyPx
    font.bold: variant === "primary" || variant === "danger"

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    function normalizedDisabledStatus() {
        return String(disabledStatus || "").toLowerCase()
    }

    function disabledToneColor() {
        var state = normalizedDisabledStatus()
        if (state === "offline")
            return Theme.fgSecondary
        if (state === "printing")
            return Theme.accent
        if (state === "error")
            return Theme.danger
        return Theme.fgDisabled
    }

    function disabledBackgroundColor() {
        var state = normalizedDisabledStatus()
        if (state.length <= 0)
            return Theme.bgSurface
        var baseColor = disabledToneColor()
        return Theme.themeName === "Dark"
                ? Qt.darker(baseColor, 2.6)
                : Qt.lighter(baseColor, 1.9)
    }

    function backgroundColor() {
        if (!enabled)
            return disabledBackgroundColor()
        if (variant === "primary")
            return down ? Qt.darker(Theme.accent, 1.08) : (hovered ? Qt.lighter(Theme.accent, 1.04) : Theme.accent)
        if (variant === "danger")
            return down ? Qt.darker(Theme.danger, 1.08) : (hovered ? Qt.lighter(Theme.danger, 1.04) : Theme.danger)
        return down ? Qt.darker(Theme.bgSurface, 1.05) : (hovered ? Qt.lighter(Theme.bgSurface, 1.02) : Theme.bgSurface)
    }

    function borderColor() {
        if (!enabled && normalizedDisabledStatus().length > 0)
            return disabledToneColor()
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
            return disabledToneColor()
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
        id: buttonLabel
        text: root.text
        color: root.foregroundColor()
        font: root.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
