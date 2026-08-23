import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "Theme.js" as Theme

Rectangle {
    id: root
    objectName: "viewerBuildModal"
    property bool running: false
    property real progress: 0.0
    readonly property real boundedProgress: Math.max(0.0, Math.min(1.0, root.progress))

    visible: root.running
    color: Theme.overlayScrim
    z: 100
    focus: visible

    MouseArea {
        id: inputBlocker
        objectName: "viewerBuildInputBlocker"
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: function(wheel) { wheel.accepted = true }
    }

    Rectangle {
        id: progressCard
        objectName: "viewerBuildProgressCard"
        anchors.centerIn: parent
        width: Math.min(Math.max(320, parent.width * 0.46), 520)
        height: progressLayout.implicitHeight + 40
        radius: Theme.radiusDialog
        color: Theme.bgDialog
        border.width: Theme.borderWidth
        border.color: Theme.borderDefault

        ColumnLayout {
            id: progressLayout
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Text {
                objectName: "viewerBuildProgressTitle"
                text: qsTr("Creating 3D view…")
                color: Theme.fgPrimary
                font.pixelSize: Theme.fontBodyPx
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            ProgressBar {
                id: progressBar
                objectName: "viewerBuildProgressBar"
                from: 0.0
                to: 1.0
                value: root.boundedProgress
                Layout.fillWidth: true
                Layout.preferredWidth: 360
                Layout.alignment: Qt.AlignHCenter

                background: Rectangle {
                    implicitHeight: 8
                    radius: 4
                    color: Theme.borderSubtle
                }

                contentItem: Item {
                    implicitHeight: 8

                    Rectangle {
                        width: progressBar.visualPosition * parent.width
                        height: parent.height
                        radius: 4
                        color: Theme.accent
                    }
                }
            }

            Text {
                objectName: "viewerBuildProgressPercent"
                text: Math.round(root.boundedProgress * 100) + "%"
                color: Theme.fgSecondary
                font.pixelSize: Theme.fontCaptionPx
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
        }
    }
}
