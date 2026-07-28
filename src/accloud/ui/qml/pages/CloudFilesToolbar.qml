import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

RowLayout {
    id: root
    objectName: "cloudFilesToolbar"

    property bool loading: false
    property int selectedFilesCount: 0
    property bool batchDeleteRunning: false
    property int batchDeleteCompleted: 0
    property int batchDeleteTotal: 0
    property var typeFilterOptions: []
    property int typeFilterCurrentIndex: 0

    signal refreshRequested()
    signal deleteSelectedRequested()
    signal uploadRequested()
    signal typeFilterSelected(int index, string code)

    Layout.fillWidth: true
    spacing: 8

    Item {
        id: primaryActionsHost
        objectName: "filesPrimaryActionsHost"
        Layout.fillWidth: true
        implicitHeight: primaryActions.implicitHeight

        RowLayout {
            id: primaryActions
            objectName: "filesPrimaryActions"
            anchors.centerIn: parent
            spacing: 8

            AppButton {
                id: refreshFilesButton
                objectName: "refreshFilesButton"
                text: root.loading ? qsTr("Loading...") : qsTr("Refresh")
                variant: "secondary"
                compact: true
                enabled: !root.loading
                onClicked: root.refreshRequested()
            }

            AppButton {
                id: deleteSelectedFilesButton
                objectName: "deleteSelectedFilesButton"
                text: root.batchDeleteRunning
                      ? qsTr("Deleting %1/%2...")
                            .arg(String(root.batchDeleteCompleted))
                            .arg(String(root.batchDeleteTotal))
                      : qsTr("Delete (%1)").arg(String(root.selectedFilesCount))
                variant: "danger"
                compact: true
                visible: root.selectedFilesCount > 0 || root.batchDeleteRunning
                enabled: !root.loading && !root.batchDeleteRunning
                onClicked: root.deleteSelectedRequested()
            }

            AppButton {
                id: uploadPwmbButton
                objectName: "uploadPwmbButton"
                text: qsTr("Upload")
                variant: "primary"
                compact: true
                onClicked: root.uploadRequested()
            }
        }
    }

    RowLayout {
        id: typeFilterGroup
        objectName: "filesTypeFilterGroup"
        spacing: 8

        Text {
            text: qsTr("Type")
            color: Theme.fgSecondary
            font.pixelSize: Theme.fontBodyPx
        }

        AppComboBox {
            id: typeFilterCombo
            objectName: "filesTypeFilter"
            Layout.preferredWidth: 118
            Layout.preferredHeight: 30
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
