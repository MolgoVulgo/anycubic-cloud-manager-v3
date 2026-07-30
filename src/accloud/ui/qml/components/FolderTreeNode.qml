import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.folderlistmodel
import "Theme.js" as Theme

Item {
    id: root

    property url folderUrl: ""
    property string displayName: ""
    property url currentFolder: ""
    property int depth: 0
    property bool expanded: false

    signal folderActivated(url folder)

    readonly property string normalizedFolderPath: normalizedPath(folderUrl)
    readonly property string normalizedCurrentPath: normalizedPath(currentFolder)
    readonly property bool selected: normalizedFolderPath.length > 0
                                     && normalizedFolderPath === normalizedCurrentPath

    implicitHeight: nodeColumn.implicitHeight
    implicitWidth: nodeColumn.implicitWidth

    function syncExpansionToCurrentFolder() {
        var folder = normalizedFolderPath
        var current = normalizedCurrentPath
        if (folder.length === 0 || current.length === 0 || folder === current)
            return
        var prefix = folder === "/" ? "/" : folder + "/"
        if (current.indexOf(prefix) === 0)
            expanded = true
    }

    onCurrentFolderChanged: syncExpansionToCurrentFolder()
    onFolderUrlChanged: syncExpansionToCurrentFolder()
    Component.onCompleted: syncExpansionToCurrentFolder()

    function normalizedPath(value) {
        var path = String(value || "").trim()
        path = path.replace(/^file:\/\/localhost/i, "file://")
        path = path.replace(/^file:\/\//i, "")
        path = path.replace(/[?#].*$/, "")
        try {
            path = decodeURIComponent(path)
        } catch (err) {}
        path = path.replace(/\\/g, "/")
        if (path.length > 1)
            path = path.replace(/\/+$/, "")
        return path
    }

    Column {
        id: nodeColumn
        width: root.width
        spacing: 0

        Rectangle {
            id: nodeRow
            width: parent.width
            height: 34
            radius: Theme.radiusControl
            color: root.selected
                   ? Theme.accentSoft
                   : (rowMouse.containsMouse ? Qt.lighter(Theme.bgSurface, 1.04) : "transparent")

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6 + root.depth * 18
                anchors.rightMargin: 8
                spacing: 5

                ToolButton {
                    id: expandButton
                    objectName: "downloadFolderTreeToggle"
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 28
                    text: root.expanded ? "▾" : "▸"
                    font.pixelSize: Theme.fontBodyPx
                    onClicked: root.expanded = !root.expanded

                    background: Rectangle {
                        radius: Theme.radiusControl
                        color: expandButton.hovered ? Qt.lighter(Theme.bgSurface, 1.06) : "transparent"
                    }
                    contentItem: Text {
                        text: expandButton.text
                        color: Theme.fgSecondary
                        font: expandButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Text {
                    text: "📁"
                    font.pixelSize: Theme.fontBodyPx
                }

                Text {
                    Layout.fillWidth: true
                    text: root.displayName
                    color: root.selected ? Theme.accentStrong : Theme.fgPrimary
                    font.pixelSize: Theme.fontBodyPx
                    font.weight: root.selected ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }

            MouseArea {
                id: rowMouse
                anchors.fill: parent
                anchors.leftMargin: 34 + root.depth * 18
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton
                onClicked: root.folderActivated(root.folderUrl)
                onDoubleClicked: root.expanded = !root.expanded
            }
        }

        Loader {
            id: childrenLoader
            width: parent.width
            active: root.expanded
            visible: active

            sourceComponent: Component {
                Column {
                    width: childrenLoader.width

                    FolderListModel {
                        id: childFoldersModel
                        folder: root.folderUrl
                        nameFilters: ["*"]
                        showDirs: true
                        showFiles: false
                        showDirsFirst: true
                        showDotAndDotDot: false
                        sortField: FolderListModel.Name
                        sortCaseSensitive: false
                    }

                    Repeater {
                        model: childFoldersModel

                        delegate: Loader {
                            id: childNodeLoader

                            required property string fileName
                            required property url fileUrl

                            width: parent ? parent.width : 0
                            height: item ? item.implicitHeight : 0
                            source: Qt.resolvedUrl("FolderTreeNode.qml")

                            onLoaded: {
                                item.width = Qt.binding(function() {
                                    return childNodeLoader.width
                                })
                                item.folderUrl = childNodeLoader.fileUrl
                                item.displayName = childNodeLoader.fileName
                                item.currentFolder = Qt.binding(function() {
                                    return root.currentFolder
                                })
                                item.depth = root.depth + 1
                                item.folderActivated.connect(function(folder) {
                                    root.folderActivated(folder)
                                })
                            }
                        }
                    }
                }
            }
        }
    }
}
