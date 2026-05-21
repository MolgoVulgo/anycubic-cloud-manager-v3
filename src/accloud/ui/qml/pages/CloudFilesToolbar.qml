import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

RowLayout {
    id: root

    property bool loading: false
    property var typeFilterOptions: []
    property int typeFilterCurrentIndex: 0

    signal refreshRequested()
    signal uploadRequested()
    signal typeFilterSelected(int index, string code)

    Layout.fillWidth: true
    spacing: 8

    AppButton {
        id: refreshFilesButton
        objectName: "refreshFilesButton"
        text: root.loading ? qsTr("Loading...") : qsTr("Refresh")
        variant: "secondary"
        enabled: !root.loading
        onClicked: root.refreshRequested()
    }

    AppButton {
        id: uploadPwmbButton
        objectName: "uploadPwmbButton"
        text: qsTr("Upload")
        variant: "primary"
        onClicked: root.uploadRequested()
    }

    Item { Layout.fillWidth: true }

    RowLayout {
        spacing: 8

        Text {
            text: qsTr("Type")
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontBodyPx
        }

        AppComboBox {
            id: typeFilterCombo
            objectName: "filesTypeFilter"
            Layout.preferredWidth: 130
            textRole: "label"
            model: root.typeFilterOptions
            currentIndex: root.typeFilterCurrentIndex
            onActivated: {
                if (currentIndex >= 0 && currentIndex < model.length) {
                    root.typeFilterSelected(currentIndex, String(model[currentIndex].code))
                }
            }
        }
    }
}
