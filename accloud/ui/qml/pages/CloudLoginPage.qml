import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Item {
    anchors.fill: parent

    AppPageFrame {
        anchors.fill: parent
        sectionTitle: qsTr("Cloud Login")
        sectionSubtitle: qsTr("Connexion manuelle (mode fallback)")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.gapRow

            Item { Layout.fillHeight: true }

            AppTextField {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 340
                placeholderText: qsTr("Email")
            }

            AppTextField {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 340
                placeholderText: qsTr("Password")
                echoMode: TextInput.Password
            }

            AppButton {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Login")
                variant: "primary"
            }

            Item { Layout.fillHeight: true }
        }
    }
}
