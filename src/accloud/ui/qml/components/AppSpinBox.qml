import QtQuick 2.15
import QtQuick.Controls 2.15
import "Theme.js" as Theme

SpinBox {
    id: root

    editable: true
    implicitHeight: Theme.controlHeight
    implicitWidth: 120
    focusPolicy: Qt.TabFocus
    font.pixelSize: Theme.fontBodyPx

    contentItem: TextInput {
        z: 2
        text: root.textFromValue(root.value, root.locale)
        font: root.font
        color: root.enabled ? Theme.fgPrimary : Theme.fgDisabled
        selectionColor: Theme.selectionBg
        selectedTextColor: Theme.selectionFg
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !root.editable
        validator: root.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly

        onTextEdited: function() {
            root.value = root.valueFromText(text, root.locale)
        }
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.bgSurface
        border.width: Theme.borderWidth
        border.color: root.activeFocus ? Theme.accent : Theme.borderDefault
    }
}
