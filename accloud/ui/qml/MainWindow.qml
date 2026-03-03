import QtQuick 2.15
import QtQuick.Controls 2.15

ApplicationWindow {
    id: root
    width: 1440
    height: 900
    visible: true
    title: "accloud"

    header: ToolBar {
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12
            ToolButton { text: "Cloud"; onClicked: stack.currentIndex = 0 }
            ToolButton { text: "Viewer"; onClicked: stack.currentIndex = 1 }
            ToolButton { text: "Settings"; onClicked: stack.currentIndex = 2 }
            ToolButton { text: "Debug"; onClicked: stack.currentIndex = 3 }
        }
    }

    StackLayout {
        id: stack
        anchors.fill: parent

        Loader { source: "pages/CloudFilesPage.qml" }
        Loader { source: "pages/ViewerPage.qml" }
        Loader { source: "pages/SettingsPage.qml" }
        Loader { source: "pages/DebugPage.qml" }
    }
}
