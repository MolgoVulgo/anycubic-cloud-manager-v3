import QtQuick 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme
import "."

RowLayout {
    id: root

    property string labelText: ""
    property int labelWidth: 120
    property bool fillFields: true
    property bool emphasizedLabel: false
    default property alias fieldData: fieldsRow.data

    Layout.fillWidth: true
    spacing: Theme.gapRow

    FormLabel {
        text: root.labelText
        preferredWidth: root.labelWidth
        emphasize: root.emphasizedLabel
    }

    RowLayout {
        id: fieldsRow
        Layout.fillWidth: root.fillFields
        spacing: Theme.gapRow
    }
}
