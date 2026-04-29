import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../components"

RowLayout {
    id: root
    objectName: "printerToolbar"

    property bool loading: false
    property bool debugChecked: false
    property bool showDebugControls: false

    signal refreshRequested()
    signal debugToggled(bool checked)

    Layout.fillWidth: true
    spacing: 8

    AppButton {
        objectName: "refreshPrintersButton"
        text: root.loading ? qsTr("Refreshing...") : qsTr("Refresh printers")
        variant: "secondary"
        enabled: !root.loading
        onClicked: root.refreshRequested()
    }

    AppCheckBox {
        objectName: "debugLabelsToggle"
        visible: root.showDebugControls
        text: qsTr("Debug UI")
        checked: root.debugChecked
        onToggled: root.debugToggled(checked)
    }

    Item { Layout.fillWidth: true }
}
