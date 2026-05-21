import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    anchors.fill: parent

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        Loader {
            SplitView.fillWidth: true
            source: "../panes/PreviewPane.qml"
        }
        Loader {
            SplitView.fillWidth: true
            source: "../panes/LayerInspectorPane.qml"
        }
        Loader {
            SplitView.fillWidth: true
            source: "../panes/Viewer3DPane.qml"
        }
    }
}
