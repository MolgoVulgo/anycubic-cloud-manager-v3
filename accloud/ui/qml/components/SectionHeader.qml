import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Item {
    id: root

    property alias title: titleLabel.text
    property alias subtitle: subtitleLabel.text

    implicitWidth: titleColumn.implicitWidth
    implicitHeight: titleColumn.implicitHeight

    ColumnLayout {
        id: titleColumn
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 2

        Text {
            id: titleLabel
            visible: text.length > 0
            text: ""
            color: Theme.fgPrimary
            font.pixelSize: Theme.fontSectionPx
            font.bold: true
        }

        Text {
            id: subtitleLabel
            visible: text.length > 0
            text: ""
            color: Theme.fgSecondary
            opacity: 0.9
            font.pixelSize: Theme.fontCaptionPx
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
