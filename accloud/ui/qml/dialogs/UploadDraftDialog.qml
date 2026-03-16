import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components"

AppDialogFrame {
    id: root
    title: qsTr("Upload .pwmb (Draft)")
    subtitle: qsTr("Draft workflow: upload + optional print actions")
    minimumWidth: 820
    maximumWidth: 1040
    minimumHeight: 520
    maximumHeight: 680
    dialogSize: "large"

    bodyData: [
        FormRow {
            labelText: qsTr("File")
            labelWidth: 100
            AppTextField { Layout.fillWidth: true; placeholderText: qsTr("/path/model.pwmb") }
            AppButton { text: qsTr("Browse") }
        },
        FormRow {
            labelText: qsTr("Upload profile")
            labelWidth: 100
            AppComboBox { Layout.fillWidth: true; model: [qsTr("Default"), qsTr("High reliability"), qsTr("Fast")] }
        },
        FormRow {
            labelText: qsTr("Print target")
            labelWidth: 100
            AppComboBox { Layout.fillWidth: true; model: [qsTr("None"), "M7-Workshop-A", "M5S-Line-2"] }
        },
        AppCheckBox { text: qsTr("Print after upload") },
        AppCheckBox { text: qsTr("Delete after print") },
        AppCheckBox { text: qsTr("Keep signed URL snapshot") }
    ]

    footerTrailingData: [
        AppButton { text: qsTr("Close"); onClicked: root.close() },
        AppButton {
            text: qsTr("Start upload")
            variant: "primary"
        }
    ]
}
