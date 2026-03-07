import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components/Theme.js" as Theme
import "../components"

Frame {
    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.bgSurface
        border.width: Theme.borderWidth
        border.color: Theme.borderDefault
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingPage
        spacing: Theme.gapRow
        Text { text: "Layer Inspector"; color: Theme.fgPrimary; font.pixelSize: Theme.fontSectionPx }
        AppSlider { from: 0; to: 1; value: 0 }
        AppButton { text: "Export layer" }
    }
}
