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
    Layout.preferredHeight: 92
    radius: embeddedInTabsContainer ? 0 : Theme.radiusControl
    color: Theme.bgSurface
    border.width: embeddedInTabsContainer ? 0 : Theme.borderWidth
    border.color: Theme.borderDefault

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 4

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
                    onClicked: root.printerSelected(printer && printer.id ? String(printer.id) : "")
                }
            }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: detailsRow.implicitWidth
            contentHeight: detailsRow.height
            clip: true

            RowLayout {
                id: detailsRow
                spacing: 6
                height: 34

                Repeater {
                    model: root.printersModel

                    AppButton {
                        required property int index
                        readonly property var printer: root.printersModel.get(index)
                        text: qsTr("Details: %1")
                              .arg(String(printer && printer.name ? printer.name : qsTr("Printer")))
                        compact: true
                        variant: "secondary"
                        onClicked: root.printerDetailsRequested(
                                       printer && printer.id ? String(printer.id) : "")
                    }
                }
            }
        }
    }
}
