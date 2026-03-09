import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Rectangle {
    id: root
    objectName: "printersTabsBar"

    property var printersModel: null
    property string selectedPrinterId: ""
    property var tabTitleProvider: null

    signal printerSelected(string printerId)

    Layout.fillWidth: true
    Layout.preferredHeight: 50
    radius: Theme.radiusControl
    color: Theme.bgSurface
    border.width: Theme.borderWidth
    border.color: Theme.borderDefault

    AppTabBar {
        id: printersTabBar
        anchors.fill: parent
        anchors.margins: 0
        tabVariant: "local"
        tabLook: "classic"
        tabSizingMode: "content"
        minTabWidth: 170
        connectActiveToPanel: true
        panelColor: Theme.bgSurface
        clip: true

        Repeater {
            model: root.printersModel

            AppTabButton {
                objectName: "printerTabButton"
                required property int index
                readonly property var printer: root.printersModel.get(index)
                text: typeof root.tabTitleProvider === "function"
                      ? String(root.tabTitleProvider(printer))
                      : String(printer && printer.name ? printer.name : qsTr("Printer"))
                checked: root.selectedPrinterId === String(printer && printer.id ? printer.id : "")
                onClicked: root.printerSelected(printer && printer.id ? String(printer.id) : "")
            }
        }
    }
}
