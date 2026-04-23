import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.folderlistmodel
import QtCore
import "Theme.js" as Theme

Dialog {
    id: root

    property url currentFolder: StandardPaths.writableLocation(StandardPaths.HomeLocation)
    property var nameFilters: [qsTr("All files (*)")]
    property url selectedFile: ""

    signal fileChosen(url file)
    signal cancelled()

    property int selectedFilterIndex: 0
    property string selectedPath: ""
    property bool selectedIsDir: false
    property string homePath: localPathFromInput(StandardPaths.writableLocation(StandardPaths.HomeLocation))
    readonly property bool hasSelection: selectedPath.length > 0
    readonly property bool canConfirm: hasSelection && !selectedIsDir && isSupportedSliceFile(selectedPath)

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    parent: Overlay.overlay
    width: Math.min(1200, Math.round((parent ? parent.width : 1280) * 0.9))
    height: Math.min(800, Math.round((parent ? parent.height : 860) * 0.9))
    x: Math.round(((parent ? parent.width : width) - width) / 2)
    y: Math.round(((parent ? parent.height : height) - height) / 2)
    padding: 0
    title: qsTr("Select file to upload")

    function localPathFromInput(pathInput) {
        var raw = String(pathInput || "").trim()
        if (raw.length === 0)
            return ""
        var normalized = raw.replace(/^file:\/\/localhost/i, "file://")
        if (normalized.indexOf("file://") === 0)
            normalized = normalized.replace(/^file:\/\//i, "")
        normalized = normalized.replace(/[?#].*$/, "")
        try {
            normalized = decodeURIComponent(normalized)
        } catch (err) {}
        normalized = normalized.replace(/\\/g, "/")
        if (normalized.length > 1)
            normalized = normalized.replace(/\/+$/, "")
        return normalized
    }

    function pathToFileUrl(pathInput) {
        var path = localPathFromInput(pathInput)
        if (path.length === 0)
            path = StandardPaths.writableLocation(StandardPaths.HomeLocation)
        if (path.charAt(0) === "/")
            return "file://" + path
        return "file:///" + path
    }

    function parentFolderPath(pathInput) {
        var path = localPathFromInput(pathInput)
        var slash = path.lastIndexOf("/")
        if (slash <= 0)
            return path
        return path.slice(0, slash)
    }

    function fileBaseName(pathInput) {
        var path = localPathFromInput(pathInput)
        if (path.length === 0)
            return ""
        return path.split("/").pop()
    }

    function fileExtension(pathInput) {
        var name = fileBaseName(pathInput)
        var dot = name.lastIndexOf(".")
        if (dot < 0 || dot + 1 >= name.length)
            return ""
        return name.slice(dot + 1).toLowerCase()
    }

    function parseFilterGlobs(filterText) {
        var raw = String(filterText || "")
        var match = raw.match(/\(([^)]+)\)/)
        if (!match || match.length < 2)
            return ["*"]
        var tokens = String(match[1]).trim().split(/\s+/)
        var globs = []
        for (var i = 0; i < tokens.length; ++i) {
            var token = String(tokens[i] || "").trim()
            if (token.length > 0)
                globs.push(token)
        }
        return globs.length > 0 ? globs : ["*"]
    }

    function selectedFilterGlobs() {
        var list = nameFilters || []
        if (list.length <= 0)
            return ["*"]
        var idx = Math.max(0, Math.min(selectedFilterIndex, list.length - 1))
        return parseFilterGlobs(list[idx])
    }

    function supportedSliceExtensions() {
        var list = nameFilters || []
        if (list.length <= 0)
            return []
        var globs = parseFilterGlobs(list[0])
        var exts = []
        for (var i = 0; i < globs.length; ++i) {
            var token = String(globs[i] || "").toLowerCase().trim()
            if (token.indexOf("*.") === 0 && token.length > 2)
                exts.push(token.slice(2))
        }
        return exts
    }

    function isSupportedSliceFile(pathInput) {
        var ext = fileExtension(pathInput)
        if (ext.length === 0)
            return false
        var exts = supportedSliceExtensions()
        for (var i = 0; i < exts.length; ++i) {
            if (exts[i] === ext)
                return true
        }
        return false
    }

    function resetSelection() {
        selectedPath = ""
        selectedIsDir = false
        selectedFile = ""
    }

    function navigateToFolder(pathInput) {
        currentFolder = pathToFileUrl(pathInput)
        resetSelection()
    }

    function chooseCurrentSelection() {
        if (!canConfirm)
            return
        selectedFile = pathToFileUrl(selectedPath)
        close()
        fileChosen(selectedFile)
    }

    function refreshVisibleLists() {
        directoriesModel.clear()
        filesModel.clear()
        var total = Number(folderModel.count || 0)
        for (var i = 0; i < total; ++i) {
            var name = String(folderModel.get(i, "fileName") || "")
            var path = String(folderModel.get(i, "filePath") || "")
            var isDir = folderModel.get(i, "fileIsDir") === true
            var row = { "name": name, "path": path, "isDir": isDir }
            if (isDir)
                directoriesModel.append(row)
            else
                filesModel.append(row)
        }
        rebuildFolderTree()
    }

    function pathStartsWith(pathValue, baseValue) {
        var path = String(pathValue || "")
        var base = String(baseValue || "")
        if (base.length <= 0)
            return true
        if (path === base)
            return true
        return path.indexOf(base + "/") === 0
    }

    function pathSegments(pathValue) {
        var path = localPathFromInput(pathValue)
        var chunks = path.split("/")
        var out = []
        for (var i = 0; i < chunks.length; ++i) {
            var value = String(chunks[i] || "").trim()
            if (value.length > 0)
                out.push(value)
        }
        return out
    }

    function rebuildFolderTree() {
        folderTreeModel.clear()

        var currentPath = localPathFromInput(currentFolder)
        var rootBase = pathStartsWith(currentPath, homePath) ? homePath : "/"
        var rootLabel = rootBase === "/" ? "/" : fileBaseName(rootBase)
        var depth = 0

        folderTreeModel.append({
            "name": rootLabel,
            "path": rootBase,
            "depth": depth,
            "isCurrent": currentPath === rootBase
        })

        var remainder = ""
        if (rootBase === "/")
            remainder = currentPath
        else if (currentPath.length > rootBase.length)
            remainder = currentPath.slice(rootBase.length + 1)

        var parent = rootBase
        var segments = pathSegments(remainder)
        for (var i = 0; i < segments.length; ++i) {
            var seg = segments[i]
            parent = parent === "/" ? ("/" + seg) : (parent + "/" + seg)
            depth += 1
            folderTreeModel.append({
                "name": seg,
                "path": parent,
                "depth": depth,
                "isCurrent": currentPath === parent
            })
        }

        for (var j = 0; j < directoriesModel.count; ++j) {
            var entry = directoriesModel.get(j)
            var childPath = String(entry.path || "")
            var childName = String(entry.name || "")
            folderTreeModel.append({
                "name": childName,
                "path": childPath,
                "depth": depth + 1,
                "isCurrent": false
            })
        }
    }

    onOpened: {
        if (String(currentFolder || "").trim().length <= 0)
            currentFolder = pathToFileUrl(StandardPaths.writableLocation(StandardPaths.HomeLocation))
        resetSelection()
    }

    onRejected: cancelled()
    Keys.onEscapePressed: {
        root.close()
        event.accepted = true
    }
    Keys.onReturnPressed: {
        chooseCurrentSelection()
        event.accepted = true
    }
    Keys.onEnterPressed: {
        chooseCurrentSelection()
        event.accepted = true
    }

    background: Rectangle {
        radius: Theme.radiusDialog
        color: Theme.bgDialog
        border.width: Theme.borderWidth
        border.color: Theme.borderDefault
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingDialog
        spacing: Theme.gapSection

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text { text: root.title; color: Theme.fgPrimary; font.pixelSize: Theme.fontTitlePx; font.bold: true }
                Text { text: String(root.currentFolder); color: Theme.fgSecondary; font.pixelSize: Theme.fontCaptionPx; elide: Text.ElideMiddle; Layout.fillWidth: true }
            }

            AppButton { text: qsTr("X"); variant: "secondary"; compact: true; onClicked: root.close() }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapRow

            AppButton {
                text: qsTr("Home")
                variant: "secondary"
                onClicked: root.navigateToFolder(StandardPaths.writableLocation(StandardPaths.HomeLocation))
            }

            AppButton {
                text: qsTr("Up")
                variant: "secondary"
                onClicked: {
                    var parentPath = root.parentFolderPath(root.currentFolder)
                    root.navigateToFolder(parentPath)
                }
            }

            TextField {
                Layout.fillWidth: true
                readOnly: true
                text: String(root.currentFolder)
                color: Theme.fgPrimary
                selectByMouse: true
                background: Rectangle {
                    radius: Theme.radiusControl
                    color: Theme.bgSurface
                    border.width: Theme.borderWidth
                    border.color: Theme.borderDefault
                }
            }

            AppComboBox {
                Layout.preferredWidth: 360
                model: root.nameFilters
                currentIndex: root.selectedFilterIndex
                textColor: Theme.fgPrimary
                popupTextColor: Theme.fgPrimary
                onActivated: function(index) {
                    root.selectedFilterIndex = index
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.gapRow

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusCard
                color: Theme.bgSurface
                border.width: Theme.borderWidth
                border.color: Theme.borderDefault

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6
                    Text { text: qsTr("Folders"); color: Theme.fgSecondary; font.pixelSize: Theme.fontCaptionPx }
                    ListView {
                        id: foldersList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: folderTreeModel
                        clip: true
                        currentIndex: -1
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property string name
                            required property string path
                            required property int depth
                            required property bool isCurrent
                            width: ListView.view.width
                            height: 34
                            radius: Theme.radiusControl
                            color: root.selectedPath === path
                                   ? Theme.selectionBg
                                   : (folderMouse.containsMouse ? Qt.lighter(Theme.bgSurface, 1.04) : "transparent")
                            border.width: root.selectedPath === path ? Theme.borderWidth : 0
                            border.color: Theme.accent

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8 + (depth * 14)
                                anchors.rightMargin: 10
                                spacing: 8

                                Text {
                                    text: isCurrent ? "▾" : "▸"
                                    color: Theme.fgSecondary
                                    font.pixelSize: Theme.fontCaptionPx
                                }

                                Text {
                                    text: "📁"
                                    font.pixelSize: Theme.fontBodyPx
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: name
                                    color: Theme.fgPrimary
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                id: folderMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    root.selectedPath = path
                                    root.selectedIsDir = true
                                }
                                onDoubleClicked: root.navigateToFolder(path)
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusCard
                color: Theme.bgSurface
                border.width: Theme.borderWidth
                border.color: Theme.borderDefault

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6
                    Text { text: qsTr("Files"); color: Theme.fgSecondary; font.pixelSize: Theme.fontCaptionPx }
                    ListView {
                        id: filesList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: filesModel
                        clip: true
                        currentIndex: -1
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property string name
                            required property string path
                            width: ListView.view.width
                            height: 34
                            radius: Theme.radiusControl
                            color: root.selectedPath === path
                                   ? Theme.selectionBg
                                   : (fileMouse.containsMouse ? Qt.lighter(Theme.bgSurface, 1.04) : "transparent")
                            border.width: root.selectedPath === path ? Theme.borderWidth : 0
                            border.color: Theme.accent

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: "📄"
                                font.pixelSize: Theme.fontBodyPx
                            }

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 36
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: name
                                color: Theme.fgPrimary
                                elide: Text.ElideRight
                            }

                            MouseArea {
                                id: fileMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    root.selectedPath = path
                                    root.selectedIsDir = false
                                }
                                onDoubleClicked: {
                                    root.selectedPath = path
                                    root.selectedIsDir = false
                                    root.chooseCurrentSelection()
                                }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                color: root.canConfirm ? Theme.fgSecondary : Theme.warning
                font.pixelSize: Theme.fontCaptionPx
                text: root.hasSelection
                      ? (root.selectedIsDir
                         ? qsTr("Folder selected: %1").arg(root.fileBaseName(root.selectedPath))
                         : root.canConfirm
                           ? qsTr("Selected file: %1").arg(root.fileBaseName(root.selectedPath))
                           : qsTr("Unsupported file type: %1").arg(root.fileBaseName(root.selectedPath)))
                      : qsTr("No file selected")
                elide: Text.ElideMiddle
            }

            AppButton { text: qsTr("Cancel"); variant: "secondary"; onClicked: root.close() }
            AppButton {
                text: qsTr("Select")
                variant: "primary"
                enabled: root.canConfirm
                onClicked: root.chooseCurrentSelection()
            }
        }
    }

    ListModel { id: directoriesModel }
    ListModel { id: filesModel }
    ListModel { id: folderTreeModel }

    FolderListModel {
        id: folderModel
        folder: root.currentFolder
        nameFilters: root.selectedFilterGlobs()
        showDirs: true
        showFiles: true
        showDirsFirst: true
        showDotAndDotDot: false
        sortCaseSensitive: false
    }

    onCurrentFolderChanged: refreshVisibleLists()
    onSelectedFilterIndexChanged: refreshVisibleLists()

    Connections {
        target: folderModel
        function onStatusChanged() { root.refreshVisibleLists() }
        function onCountChanged() { root.refreshVisibleLists() }
    }
}
