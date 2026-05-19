import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

AppDialogFrame {
    id: root

    property alias scrollBodyData: scrollBody.data
    property int bodySpacing: Theme.gapSection
    property int contentMinimumHeight: 260

    bodyData: [
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Math.max(root.contentMinimumHeight, scrollBody.implicitHeight)
            clip: true

            ColumnLayout {
                id: scrollBody
                width: parent.width
                spacing: root.bodySpacing
            }
        }
    ]
}
