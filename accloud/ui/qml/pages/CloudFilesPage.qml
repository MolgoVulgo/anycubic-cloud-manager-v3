import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs
import QtQuick.Layouts 1.15
import QtCore
import "../components/Theme.js" as Theme
import "../components"

Item {
    id: root
    objectName: "cloudFilesPage"
    Layout.fillWidth: true
    Layout.fillHeight: true
    property alias filesModel: cloudFilesModel

    // ── État ──────────────────────────────────────────────────────────────
    property bool   loading:      false
    property string statusMsg:    "Prêt."
    property string statusSev:    "ok"
    property var    quotaData:    null

    // ── Modèle ────────────────────────────────────────────────────────────
    ListModel { id: cloudFilesModel }

    // ── Chargement des fichiers + quota ───────────────────────────────────
    function hasCloudBridge() {
        return (typeof cloudBridge !== "undefined")
                && cloudBridge !== null
                && typeof cloudBridge.fetchFiles === "function"
                && typeof cloudBridge.fetchQuota === "function"
    }

    function loadMockFiles() {
        cloudFilesModel.clear()
        cloudFilesModel.append({
            "fileId": "demo-001",
            "fileName": "rook_plate_v12.pwmb",
            "status": "READY",
            "sizeText": "42.6 MB",
            "machine": "Photon Mono M7",
            "material": "Eco Resin Gray",
            "uploadTime": "today",
            "printTime": "02h 15m",
            "layerThickness": "0.05 mm",
            "layers": 1850,
            "isPwmb": true,
            "resinUsage": "67 ml",
            "dimensions": "102x68x120",
            "thumbnailUrl": "",
            "gcodeId": "demo-gcode-001"
        })
        cloudFilesModel.append({
            "fileId": "demo-002",
            "fileName": "calibration_tower.pws",
            "status": "READY",
            "sizeText": "11.8 MB",
            "machine": "Photon Mono M5s",
            "material": "ABS-Like Resin",
            "uploadTime": "today",
            "printTime": "00h 48m",
            "layerThickness": "0.05 mm",
            "layers": 620,
            "isPwmb": false,
            "resinUsage": "14 ml",
            "dimensions": "35x35x80",
            "thumbnailUrl": "",
            "gcodeId": "demo-gcode-002"
        })
        root.quotaData = {
            "totalDisplay": "2.0 GB",
            "usedDisplay": "1.1 GB",
            "totalBytes": 2147483648,
            "usedBytes": 1181116006
        }
        root.statusMsg = "Mode démonstration (backend indisponible)."
        root.statusSev = "warn"
        root.loading = false
    }

    function loadFiles() {
        if (root.loading) return
        root.loading   = true
        root.statusMsg = "Chargement en cours…"
        root.statusSev = "ok"

        if (!hasCloudBridge()) {
            loadMockFiles()
            return
        }

        // Quota (on l'ignore si ça échoue, l'UI affiche "—")
        var q = cloudBridge.fetchQuota()
        if (q.ok === true) root.quotaData = q

        // Listing fichiers
        var r = cloudBridge.fetchFiles(1, 20)
        root.loading = false

        if (r.ok !== true) {
            root.statusMsg = "Erreur listing : " + String(r.message)
            root.statusSev = "danger"
            return
        }

        cloudFilesModel.clear()
        var files = r.files !== undefined ? r.files : []
        for (var i = 0; i < files.length; i++) {
            cloudFilesModel.append(files[i])
        }
        root.statusMsg = String(r.message)
        root.statusSev = "ok"
    }

    Component.onCompleted: loadFiles()

    // ── Connexions aux signaux async de CloudBridge ───────────────────────
    Connections {
        target: (typeof cloudBridge !== "undefined") ? cloudBridge : null

        function onDownloadProgress(received, total) {
            if (total > 0)
                downloadOverlay.progress = received / total
            else
                downloadOverlay.progress = 0
            downloadOverlay.received = received
            downloadOverlay.total    = total
        }

        function onDownloadFinished(ok, message, savedPath) {
            downloadOverlay.visible = false
            if (ok)
                root.statusMsg = "Téléchargé : " + savedPath
            else
                root.statusMsg = "Erreur téléchargement : " + message
            root.statusSev = ok ? "ok" : "danger"
        }
    }

    // ── Dialog de confirmation de suppression ─────────────────────────────
    Dialog {
        id: deleteConfirmDialog
        property string pendingId:   ""
        property string pendingName: ""
        title: "Supprimer le fichier ?"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Yes | Dialog.No

        background: Rectangle {
            radius: 12
            color: Theme.card
            border.width: 1
            border.color: Theme.panelStroke
        }

        Text {
            text: "Supprimer définitivement « " + deleteConfirmDialog.pendingName + " » ?"
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
            width: 380
        }

        onAccepted: {
            if (!hasCloudBridge() || typeof cloudBridge.deleteFile !== "function") {
                root.statusMsg = "Suppression indisponible sans backend."
                root.statusSev = "warn"
                return
            }
            root.loading   = true
            root.statusMsg = "Suppression en cours…"
            var r = cloudBridge.deleteFile(pendingId)
            root.loading = false
            if (r.ok === true) {
                root.statusMsg = "Fichier supprimé."
                root.statusSev = "ok"
                loadFiles()
            } else {
                root.statusMsg = "Erreur suppression : " + String(r.message)
                root.statusSev = "danger"
            }
        }
    }

    // ── Dialog de sauvegarde ──────────────────────────────────────────────
    FileDialog {
        id: saveDialog
        property string pendingUrl:  ""
        property string suggestName: "fichier"
        readonly property var downloadFolders: StandardPaths.standardLocations(StandardPaths.DownloadLocation)
        title: "Enregistrer sous…"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Tous les fichiers (*)"]
        currentFolder: downloadFolders.length > 0
                     ? downloadFolders[0]
                     : StandardPaths.writableLocation(StandardPaths.HomeLocation)

        onAccepted: {
            if (!hasCloudBridge() || typeof cloudBridge.startDownload !== "function") {
                root.statusMsg = "Téléchargement indisponible sans backend."
                root.statusSev = "warn"
                return
            }
            var dest = String(selectedFile).replace(/^file:\/\//, "")
            downloadOverlay.visible  = true
            downloadOverlay.progress = 0
            downloadOverlay.received = 0
            downloadOverlay.total    = 0
            cloudBridge.startDownload(pendingUrl, dest)
        }
    }

    // ── Overlay de progression du téléchargement ──────────────────────────
    Rectangle {
        id: downloadOverlay
        anchors.fill: parent
        color: "#aa000000"
        visible: false
        z: 10

        property real   progress: 0
        property real   received: 0
        property real   total:    0

        Rectangle {
            anchors.centerIn: parent
            width: 360
            height: 130
            radius: 14
            color: Theme.panel
            border.width: 1
            border.color: Theme.panelStroke

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                Text {
                    text: "Téléchargement en cours…"
                    color: Theme.textPrimary
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                ProgressBar {
                    id: dlBar
                    from: 0; to: 1
                    value: downloadOverlay.total > 0 ? downloadOverlay.progress : 0
                    indeterminate: downloadOverlay.total <= 0
                    Layout.fillWidth: true
                    background: Rectangle {
                        implicitHeight: 6
                        radius: 3
                        color: Theme.panelStroke
                    }
                    contentItem: Item {
                        implicitHeight: 6
                        Rectangle {
                            width: dlBar.indeterminate ? parent.width * 0.3 : dlBar.visualPosition * parent.width
                            height: parent.height
                            radius: 3
                            color: Theme.accent
                        }
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    color: Theme.textSecondary
                    text: downloadOverlay.total > 0
                          ? (Math.round(downloadOverlay.received / 1048576) + " MB / "
                             + Math.round(downloadOverlay.total / 1048576) + " MB")
                          : "Connexion…"
                }

                Button {
                    text: "Annuler"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: {
                        if (hasCloudBridge() && typeof cloudBridge.cancelDownload === "function") {
                            cloudBridge.cancelDownload()
                        } else {
                            downloadOverlay.visible = false
                        }
                    }
                    background: Rectangle {
                        radius: 6
                        color: parent.down ? Qt.darker(Theme.danger, 1.1) : Theme.danger
                    }
                    contentItem: Text {
                        text: parent.text; color: "#fff4f4"
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // ── Layout principal ──────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Text {
            text: "Cloud Files"
            color: Theme.textPrimary
            font.pixelSize: 26
            font.bold: true
        }

        Text {
            text: "Vue condensée: quota, fichiers et miniatures (100x100)."
            color: Theme.textSecondary
            font.pixelSize: 14
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                objectName: "refreshFilesButton"
                text: root.loading ? "Chargement…" : "Refresh"
                enabled: !root.loading
                onClicked: loadFiles()
                background: Rectangle {
                    radius: 8
                    color: parent.down ? Qt.darker(Theme.panel, 1.1)
                                       : (parent.hovered ? Qt.lighter(Theme.panel, 1.03) : Theme.panel)
                    border.width: 1
                    border.color: Theme.panelStroke
                }
                contentItem: Text {
                    text: parent.text
                    color: Theme.textPrimary
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                id: uploadPwmbButton
                objectName: "uploadPwmbButton"
                text: "Upload .pwmb"
                font.bold: true
                background: Rectangle {
                    radius: 8
                    color: parent.down ? Qt.darker(Theme.accent, 1.12) : Theme.accent
                    border.width: 1
                    border.color: Theme.accentStrong
                }
                contentItem: Text {
                    text: parent.text
                    color: "#f8fffe"
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Item { Layout.fillWidth: true }
        }

        // ── Barre de quota ────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 62
            radius: 12
            color: Theme.panel
            border.width: 1
            border.color: Theme.panelStroke

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 16

                Text {
                    text: root.quotaData
                          ? ("Utilisé " + root.quotaData.usedDisplay + " / " + root.quotaData.totalDisplay)
                          : "Quota : —"
                    color: Theme.textPrimary
                    font.bold: true
                }

                Text {
                    text: root.quotaData
                          ? ("Libre " + (function() {
                              var free = root.quotaData.totalBytes - root.quotaData.usedBytes
                              if (free >= 1073741824) return (free / 1073741824).toFixed(1) + " GB"
                              return (free / 1048576).toFixed(0) + " MB"
                          })())
                          : ""
                    color: Theme.textSecondary
                }

                Text {
                    text: cloudFilesModel.count > 0
                          ? ("Fichiers " + cloudFilesModel.count)
                          : ""
                    color: Theme.textSecondary
                }

                Item { Layout.fillWidth: true }
            }
        }

        ErrorBanner {
            Layout.fillWidth: true
            message: root.statusMsg
            operationId: "op_files_refresh"
            severity: root.statusSev
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Rectangle {
                width: parent.width
                implicitHeight: filesColumn.implicitHeight + 20
                color: Theme.panel
                radius: 12
                border.width: 1
                border.color: Theme.panelStroke

                Column {
                    id: filesColumn
                    width: parent.width
                    spacing: 10
                    padding: 10

                    Repeater {
                        model: cloudFilesModel
                        delegate: FileCard {
                            width: filesColumn.width - 20
                            fileName:       model.fileName       || ""
                            fileId:         model.fileId         || ""
                            status:         model.status         || "UNKNOWN"
                            sizeText:       model.sizeText       || "—"
                            machine:        model.machine        || "—"
                            material:       model.material       || "—"
                            uploadTime:     model.uploadTime     || "—"
                            printTime:      model.printTime      || "—"
                            layerThickness: model.layerThickness || "—"
                            layers:         model.layers         || 0
                            isPwmb:         model.isPwmb         || false
                            resinUsage:     model.resinUsage     || "—"
                            dimensions:     model.dimensions     || "—"

                            onDeleteRequested: function(fid) {
                                deleteConfirmDialog.pendingId   = fid
                                deleteConfirmDialog.pendingName = model.fileName || fid
                                deleteConfirmDialog.open()
                            }

                            onDownloadRequested: function(fid) {
                                if (!hasCloudBridge() || typeof cloudBridge.getDownloadUrl !== "function") {
                                    root.statusMsg = "Téléchargement indisponible sans backend."
                                    root.statusSev = "warn"
                                    return
                                }
                                var r = cloudBridge.getDownloadUrl(fid)
                                if (r.ok !== true) {
                                    root.statusMsg = "Impossible d'obtenir l'URL : " + String(r.message)
                                    root.statusSev = "danger"
                                    return
                                }
                                saveDialog.pendingUrl  = r.url
                                saveDialog.defaultSuffix = (model.fileName || "").split(".").pop() || ""
                                saveDialog.open()
                            }
                        }
                    }

                    // État vide
                    Text {
                        visible: cloudFilesModel.count === 0 && !root.loading
                        width: parent.width - 20
                        horizontalAlignment: Text.AlignHCenter
                        text: "Aucun fichier cloud. Cliquez sur Refresh pour charger."
                        color: Theme.textSecondary
                        padding: 20
                    }
                }
            }
        }
    }

    // ── ListView caché pour les tests ─────────────────────────────────────
    ListView {
        id: hiddenListForTests
        objectName: "filesListView"
        visible: false
        model: cloudFilesModel
    }
}
