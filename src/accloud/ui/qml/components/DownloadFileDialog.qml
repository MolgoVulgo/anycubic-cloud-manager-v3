import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.folderlistmodel
import QtCore
import "Theme.js" as Theme

AppDialogFrame {
    id: root
    objectName: "cloudDownloadFileDialog"

    readonly property var downloadFolders: StandardPaths.standardLocations(StandardPaths.DownloadLocation)
    property url currentFolder: downloadFolders.length > 0
                                ? downloadFolders[0]
                                : StandardPaths.writableLocation(StandardPaths.HomeLocation)
    property string suggestedFileName: "file"
    property string defaultSuffix: ""
    property var nameFilters: []
    property url selectedFile: ""
    property int selectedFilterIndex: 0
    property string pendingDestinationPath: ""

    readonly property string normalizedDefaultSuffix: normalizeSuffix(defaultSuffix)
    readonly property var effectiveNameFilters: nameFilters && nameFilters.length > 0
                                                ? nameFilters
                                                : (normalizedDefaultSuffix.length > 0
                                                   ? [qsTr("%1 files (*.%2)")
                                                      .arg(normalizedDefaultSuffix.toUpperCase())
                                                      .arg(normalizedDefaultSuffix)]
                                                   : [qsTr("All files (*)")])
    readonly property string finalName: normalizedFileName(fileNameField.text)
    readonly property string destinationPath: joinPath(currentFolder, finalName)
    readonly property bool canSave: finalName.length > 0

    signal fileChosen(url file)
    signal cancelled()

    title: qsTr("Save As")
    subtitle: qsTr("Choose the destination folder and file name.")
    dialogSize: "workspace"
    minimumWidth: 760
    maximumWidth: 1120
    minimumHeight: 560
    maximumHeight: 800
    allowScrimClose: false
    allowEscapeClose: true
    requestCloseCallback: function() { root.reject() }

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

    function fileBaseName(pathInput) {
        var path = localPathFromInput(pathInput)
        if (path.length === 0)
            return ""
        return path.split("/").pop()
    }

    function normalizeSuffix(value) {
        return String(value || "").trim().replace(/^\.+/, "").toLowerCase()
    }

    function fileExtension(fileName) {
        var name = String(fileName || "")
        var dot = name.lastIndexOf(".")
        if (dot <= 0 || dot + 1 >= name.length)
            return ""
        return name.slice(dot + 1).toLowerCase()
    }

    function sanitizeFileName(value) {
        var raw = String(value || "").trim().replace(/\\/g, "/")
        raw = raw.split("/").pop()
        raw = raw.replace(/[\u0000-\u001f]/g, "")
        raw = raw.replace(/[<>:"|?*]/g, "_").trim()
        if (raw === "." || raw === "..")
            return ""
        return raw
    }

    function normalizedFileName(value) {
        var name = sanitizeFileName(value)
        if (name.length === 0)
            return ""
        if (normalizedDefaultSuffix.length > 0 && fileExtension(name).length === 0)
            name += "." + normalizedDefaultSuffix
        return name
    }

    function normalizedSuggestedFileName() {
        var name = normalizedFileName(suggestedFileName)
        if (name.length > 0)
            return name
        return normalizedDefaultSuffix.length > 0
                ? "file." + normalizedDefaultSuffix
                : "file"
    }

    function joinPath(folderInput, nameInput) {
        var folder = localPathFromInput(folderInput)
        var name = sanitizeFileName(nameInput)
        if (folder.length === 0 || name.length === 0)
            return ""
        return folder === "/" ? ("/" + name) : (folder + "/" + name)
    }

    function parseFilterGlobs(filterText) {
        var raw = String(filterText || "")
        var match = raw.match(/\(([^)]+)\)/)
        if (!match || match.length < 2)
            return ["*"]
        var tokens = String(match[1]).trim().split(/\s+/)
        var globs = []
        for (var i = 0; i < tokens.length; ++i) {
            var token = String(tokens[i] || "").trim().toLowerCase()
            if (token.length > 0)
                globs.push(token)
        }
        return globs.length > 0 ? globs : ["*"]
    }

    function fileMatchesSelectedFilter(fileName) {
        var filters = effectiveNameFilters
        var index = Math.max(0, Math.min(selectedFilterIndex, filters.length - 1))
        var globs = parseFilterGlobs(filters[index])
        var lowerName = String(fileName || "").toLowerCase()
        for (var i = 0; i < globs.length; ++i) {
            var glob = globs[i]
            if (glob === "*" || glob === "*.*")
                return true
            if (glob.indexOf("*.") === 0 && lowerName.endsWith(glob.slice(1)))
                return true
        }
        return false
    }

    function navigateToFolder(pathInput) {
        var path = localPathFromInput(pathInput)
        if (path.length === 0)
            return
        currentFolder = pathToFileUrl(path)
        selectedFilterIndex = Math.max(0, Math.min(selectedFilterIndex, effectiveNameFilters.length - 1))
    }

    function selectExistingFile(pathInput) {
        var name = fileBaseName(pathInput)
        if (name.length === 0)
            return
        fileNameField.text = name
        fileNameField.forceActiveFocus()
        fileNameField.selectAll()
    }

    function destinationExists(pathInput) {
        var target = localPathFromInput(pathInput)
        var total = Number(folderModel.count || 0)
        for (var i = 0; i < total; ++i) {
            if (folderModel.get(i, "fileIsDir") === true)
                continue
            var candidate = localPathFromInput(folderModel.get(i, "filePath"))
            if (candidate === target)
                return true
        }
        return false
    }

    function requestSave() {
        if (!canSave)
            return
        var path = destinationPath
        if (destinationExists(path)) {
            pendingDestinationPath = path
            overwriteConfirmDialog.open()
            return
        }
        commitSave(path)
    }

    function commitSave(pathInput) {
        var path = localPathFromInput(pathInput)
        if (path.length === 0)
            return
        pendingDestinationPath = path
        selectedFile = pathToFileUrl(path)
        accept()
    }

    function refreshVisibleLists() {
        contentModel.clear()
        var total = Number(folderModel.count || 0)
        var directories = []
        var files = []
        for (var i = 0; i < total; ++i) {
            var name = String(folderModel.get(i, "fileName") || "")
            var path = String(folderModel.get(i, "filePath") || "")
            var isDir = folderModel.get(i, "fileIsDir") === true
            if (isDir) {
                directories.push({ "name": name, "path": path, "isDirectory": true })
            } else if (fileMatchesSelectedFilter(name)) {
                files.push({ "name": name, "path": path, "isDirectory": false })
            }
        }
        directories.sort(function(a, b) { return a.name.localeCompare(b.name) })
        files.sort(function(a, b) { return a.name.localeCompare(b.name) })
        for (var d = 0; d < directories.length; ++d)
            contentModel.append(directories[d])
        for (var f = 0; f < files.length; ++f)
            contentModel.append(files[f])
    }

    onOpened: {
        if (String(currentFolder || "").trim().length === 0)
            currentFolder = pathToFileUrl(StandardPaths.writableLocation(StandardPaths.HomeLocation))
        selectedFile = ""
        pendingDestinationPath = ""
        selectedFilterIndex = 0
        fileNameField.text = normalizedSuggestedFileName()
        refreshVisibleLists()
        Qt.callLater(function() {
            fileNameField.forceActiveFocus()
            fileNameField.selectAll()
        })
    }

    onAccepted: fileChosen(selectedFile)
    onRejected: cancelled()
    onSelectedFilterIndexChanged: refreshVisibleLists()
    onCurrentFolderChanged: refreshVisibleLists()

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.gapRow

        AppButton {
            objectName: "downloadDialogHomeButton"
            text: qsTr("Home")
            variant: "secondary"
            onClicked: root.navigateToFolder(StandardPaths.writableLocation(StandardPaths.HomeLocation))
        }

        AppButton {
            objectName: "downloadDialogDownloadsButton"
            text: qsTr("Downloads")
            variant: "secondary"
            enabled: root.downloadFolders.length > 0
            onClicked: {
                if (root.downloadFolders.length > 0)
                    root.navigateToFolder(root.downloadFolders[0])
            }
        }

        AppTextField {
            objectName: "downloadDialogCurrentFolderField"
            Layout.fillWidth: true
            readOnly: true
            text: root.localPathFromInput(root.currentFolder)
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: 400
        spacing: Theme.gapRow

        Rectangle {
            Layout.preferredWidth: 330
            Layout.minimumWidth: 270
            Layout.fillHeight: true
            radius: Theme.radiusControl
            color: Theme.bgSurface
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Text {
                    text: qsTr("Folders")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontCaptionPx
                }

                Flickable {
                    objectName: "downloadDialogFolderTree"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: Math.max(width, folderTreeRoot.implicitWidth)
                    contentHeight: folderTreeRoot.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    ScrollBar.horizontal: ScrollBar {}

                    FolderTreeNode {
                        id: folderTreeRoot
                        width: parent.width
                        folderUrl: root.pathToFileUrl(StandardPaths.writableLocation(StandardPaths.HomeLocation))
                        displayName: qsTr("Home")
                        currentFolder: root.currentFolder
                        expanded: true
                        onFolderActivated: function(folder) {
                            root.navigateToFolder(folder)
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusControl
            color: Theme.bgSurface
            border.width: Theme.borderWidth
            border.color: Theme.borderDefault

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Text {
                    text: qsTr("Content")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.fontCaptionPx
                }

                ListView {
                    objectName: "downloadDialogContentList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: contentModel
                    clip: true
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Rectangle {
                        required property string name
                        required property string path
                        required property bool isDirectory

                        width: ListView.view.width
                        height: 36
                        radius: Theme.radiusControl
                        color: entryMouse.containsMouse
                               ? Qt.lighter(Theme.bgSurface, 1.04)
                               : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            Text {
                                text: isDirectory ? "📁" : "📄"
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
                            id: entryMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (!isDirectory)
                                    root.selectExistingFile(path)
                            }
                            onDoubleClicked: {
                                if (isDirectory) {
                                    root.navigateToFolder(path)
                                } else {
                                    root.selectExistingFile(path)
                                    root.requestSave()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.gapRow

        Text {
            text: qsTr("File name")
            color: Theme.fgPrimary
            font.pixelSize: Theme.fontBodyPx
        }

        AppTextField {
            id: fileNameField
            objectName: "downloadFileNameField"
            Layout.fillWidth: true
            placeholderText: qsTr("File name")
            onAccepted: root.requestSave()
        }

        AppComboBox {
            objectName: "downloadDialogFilterCombo"
            Layout.preferredWidth: 260
            model: root.effectiveNameFilters
            currentIndex: root.selectedFilterIndex
            textColor: Theme.fgPrimary
            popupTextColor: Theme.fgPrimary
            onActivated: function(index) { root.selectedFilterIndex = index }
        }
    }

    footerTrailingData: [
        AppButton {
            objectName: "downloadDialogCancelButton"
            text: qsTr("Cancel")
            variant: "secondary"
            onClicked: root.reject()
        },
        AppButton {
            objectName: "downloadDialogSaveButton"
            text: qsTr("Save")
            variant: "primary"
            enabled: root.canSave
            onClicked: root.requestSave()
        }
    ]

    ListModel { id: contentModel }

    FolderListModel {
        id: folderModel
        folder: root.currentFolder
        nameFilters: ["*"]
        showDirs: true
        showFiles: true
        showDirsFirst: true
        showDotAndDotDot: false
        sortCaseSensitive: false
    }

    Connections {
        target: folderModel
        function onStatusChanged() { root.refreshVisibleLists() }
        function onCountChanged() { root.refreshVisibleLists() }
    }

    AppDialogFrame {
        id: overwriteConfirmDialog
        objectName: "downloadOverwriteConfirmDialog"
        title: qsTr("Replace existing file?")
        subtitle: qsTr("A file with this name already exists in the selected folder.")
        dialogSize: "small"
        minimumWidth: 520
        allowScrimClose: false
        requestCloseCallback: function() { overwriteConfirmDialog.close() }

        Text {
            Layout.fillWidth: true
            text: qsTr("Replace %1?").arg(root.fileBaseName(root.pendingDestinationPath))
            color: Theme.fgPrimary
            font.pixelSize: Theme.fontBodyPx
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("The existing file will be overwritten.")
            color: Theme.warning
            font.pixelSize: Theme.fontCaptionPx
            wrapMode: Text.WordWrap
        }

        footerTrailingData: [
            AppButton {
                text: qsTr("Cancel")
                variant: "secondary"
                onClicked: overwriteConfirmDialog.close()
            },
            AppButton {
                text: qsTr("Replace")
                variant: "danger"
                onClicked: {
                    var destination = root.pendingDestinationPath
                    overwriteConfirmDialog.close()
                    root.commitSave(destination)
                }
            }
        ]
    }
}
