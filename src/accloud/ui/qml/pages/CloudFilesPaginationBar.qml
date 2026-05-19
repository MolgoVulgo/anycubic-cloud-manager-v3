import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Rectangle {
    id: root

    property int currentPage: 0
    property int totalPages: 1
    property int rowsCount: 0
    property int pageSize: 10
    property bool visibleWhen: true

    signal pageSizeSelected(int value)
    signal previousRequested()
    signal nextRequested()

    visible: visibleWhen
    Layout.fillWidth: true
    Layout.preferredHeight: 40
    radius: Theme.radiusControl
    color: Theme.bgWindow
    border.width: Theme.borderWidth
    border.color: Theme.borderSubtle

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        Text {
            text: qsTr("Page %1 / %2").arg(root.currentPage + 1).arg(root.totalPages)
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontCaptionPx
        }

        Text {
            text: qsTr("Rows: %1").arg(root.rowsCount)
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontCaptionPx
        }

        Text {
            text: qsTr("Rows/page")
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontCaptionPx
        }

        AppComboBox {
            id: filesRowsPerPage
            objectName: "filesRowsPerPage"
            Layout.preferredWidth: 88
            textRole: "label"
            model: [
                { "value": 10, "label": "10" },
                { "value": 20, "label": "20" },
                { "value": 50, "label": "50" },
                { "value": 100, "label": "100" }
            ]
            currentIndex: root.pageSize === 20 ? 1 : (root.pageSize === 50 ? 2 : (root.pageSize === 100 ? 3 : 0))
            onActivated: {
                if (currentIndex >= 0 && currentIndex < model.length)
                    root.pageSizeSelected(Number(model[currentIndex].value))
            }
        }

        Item { Layout.fillWidth: true }

        AppButton {
            text: qsTr("Previous")
            variant: "secondary"
            enabled: root.currentPage > 0
            onClicked: root.previousRequested()
        }

        AppButton {
            text: qsTr("Next")
            variant: "secondary"
            enabled: root.currentPage < root.totalPages - 1
            onClicked: root.nextRequested()
        }
    }
}
