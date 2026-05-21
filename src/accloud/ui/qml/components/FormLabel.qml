import QtQuick 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Text {
    id: root

    property int preferredWidth: 120
    property bool emphasize: false

    Layout.preferredWidth: preferredWidth
    Layout.alignment: Qt.AlignVCenter

    color: Theme.fgPrimary
    font.pixelSize: Theme.fontBodyPx
    font.bold: emphasize
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight
}
