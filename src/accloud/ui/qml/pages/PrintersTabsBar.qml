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
    property bool embeddedInTabsContainer: false

    signal printerSelected(string printerId)
    signal printerDetailsRequested(string printerId)

    Layout.fillWidth: true
    Layout.preferredHeight: 56
    radius: embeddedInTabsContainer ? 0 : Theme.radiusControl
    color: Theme.bgSurface
    border.width: embeddedInTabsContainer ? 0 : Theme.borderWidth
    border.color: Theme.borderDefault

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        AppTabBar {
            id: printersTabBar
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            tabVariant: "local"
            tabLook: "classic"
            tabSizingMode: "content"
            minTabWidth: 170
            connectActiveToPanel: true
            panelColor: Theme.bgSurface
            stripColor: Theme.bgSurface
            tabTopCornerRadius: embeddedInTabsContainer ? Theme.radiusControl : root.radius
            clip: false

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
                    rightPadding: 94
                    onClicked: root.printerSelected(printer && printer.id ? String(printer.id) : "")

                    AppButton {
                        objectName: "printerTabDetailsButton"
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        compact: true
                        variant: "secondary"
                        text: qsTr("Details")
                        onClicked: root.printerDetailsRequested(
                                       printer && printer.id ? String(printer.id) : "")
                    }
                }
            }
        }
    }
}
