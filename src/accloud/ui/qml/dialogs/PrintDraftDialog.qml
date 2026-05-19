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
        FormRow {
            labelText: qsTr("Printer")
            labelWidth: 100
            AppComboBox {
                Layout.fillWidth: true
                textRole: "label"
                model: [
                    { "value": "m7_workshop_a", "label": qsTr("M7-Workshop-A") },
                    { "value": "m5s_line_2", "label": qsTr("M5S-Line-2") },
                    { "value": "backup_x2", "label": qsTr("Backup-X2") }
                ]
            }
        },
        FormRow {
            labelText: qsTr("file_id")
            labelWidth: 100
            AppTextField { Layout.fillWidth: true; placeholderText: qsTr("f-2d8e91") }
        },
        FormRow {
            labelText: qsTr("Copies")
            labelWidth: 100
            AppSpinBox { from: 1; to: 10; value: 1 }
            FormLabel { text: qsTr("Priority"); preferredWidth: 56 }
            AppComboBox {
                textRole: "label"
                model: [
                    { "value": "normal", "label": qsTr("normal") },
                    { "value": "high", "label": qsTr("high") }
                ]
            }
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
