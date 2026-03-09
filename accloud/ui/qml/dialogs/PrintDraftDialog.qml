import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components"

AppDialogFrame {
    id: root
    title: qsTr("Send Print Order (Draft)")
    subtitle: qsTr("Draft direct print order dialog")
    minimumWidth: 820
    maximumWidth: 1040
    minimumHeight: 500
    maximumHeight: 640
    dialogSize: "large"

    bodyData: [
        RowLayout {
            Layout.fillWidth: true
            Text { text: qsTr("Printer"); Layout.preferredWidth: 92 }
            AppComboBox { Layout.fillWidth: true; model: ["M7-Workshop-A", "M5S-Line-2", "Backup-X2"] }
        },
        RowLayout {
            Layout.fillWidth: true
            Text { text: qsTr("file_id"); Layout.preferredWidth: 92 }
            AppTextField { Layout.fillWidth: true; placeholderText: qsTr("f-2d8e91") }
        },
        RowLayout {
            Layout.fillWidth: true
            Text { text: qsTr("Copies"); Layout.preferredWidth: 92 }
            AppSpinBox { from: 1; to: 10; value: 1 }
            Text { text: qsTr("Priority") }
            AppComboBox { model: [qsTr("normal"), qsTr("high")] }
            AppCheckBox { text: qsTr("Dry-run") }
        }
    ]

    footerTrailingData: [
        AppButton { text: qsTr("Preview payload") },
        AppButton { text: qsTr("Close"); onClicked: root.close() },
        AppButton {
            text: qsTr("Send order")
            variant: "primary"
        }
    ]
}
