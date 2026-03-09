# UI Views - reference implementation

Statut: `IMPLEMENTE` pour les vues Cloud, `PARTIEL` pour le viewer Photon.

Ce document sert de vue centrale pour les sections UI detaillees:

- Anycubic Cloud Client: `Docs/Anycubic Cloud Client/README.md`
- Photon Viewer: `Docs/Photon Viewer/README.md`

## Scope reel (2026-03-08)

### Vues implementees et actives

- Fenetre principale: `accloud/ui/qml/MainWindow.qml`
- Onglet Files: `accloud/ui/qml/pages/CloudFilesPage.qml`
- Onglet Printers: `accloud/ui/qml/pages/PrinterPage.qml`
- Onglet Logs (build debug uniquement): `accloud/ui/qml/pages/LogPage.qml`
- Dialog session HAR: `accloud/ui/qml/dialogs/SessionSettingsDialog.qml`

### Vues existantes mais non centrales (draft / placeholder)

- Upload draft: `accloud/ui/qml/dialogs/UploadDraftDialog.qml`
- Print draft: `accloud/ui/qml/dialogs/PrintDraftDialog.qml`
- Viewer draft dialog: `accloud/ui/qml/dialogs/ViewerDraftDialog.qml`
- Viewer panes placeholder: `accloud/ui/qml/panes/PreviewPane.qml`, `LayerInspectorPane.qml`, `Viewer3DPane.qml`

## Regles de lecture

1. La source de verite implementation est le code QML/C++ actuel.
2. Les docs UI de type spec doivent mentionner explicitement si un ecran est draft.
3. Les flux de production sont ceux accessibles via Files/Printers/Logs + Session HAR.
