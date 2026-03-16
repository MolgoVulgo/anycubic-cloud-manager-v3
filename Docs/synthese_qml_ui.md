# Synthese QML - Anycubic Cloud Manager v3

Date d'analyse: 2026-03-09  
Perimetre: `accloud/ui/qml/**/*.qml` (32 fichiers) + `accloud/tests/ui/qml/tst_control_room.qml` (1 fichier de test)

## 1) Decoupage hierarchique

### 1.1 Architecture globale
- **Niveau 0**: `MainWindow.qml` (`ApplicationWindow`, 1480x920).
- **Niveau 1**:
  - `components/`: design system QML (boutons, champs, tabs, cadres, banniere/statut, overlays).
  - `pages/`: ecrans metier (Cloud files, Printer, Logs, Settings, Debug, Login, Viewer).
  - `dialogs/`: modales metier (session/HAR, upload, print, viewer).
  - `panes/`: sous-panneaux du viewer.
  - `components/Theme.js`: tokens UI (palette, typo, espacements, rayons).

### 1.2 Hierarchie ecran principal (`MainWindow.qml`)
- `ApplicationWindow`
  - `MenuBar`
    - Session (Details / Import HAR)
    - Settings (Session / Theme / 3D rendering / Language)
    - Help (? / About / git)
  - Dialogs embarques
    - `Dialogs.SessionSettingsDialog`
    - `Dialogs.UploadDraftDialog`
    - `Dialogs.PrintDraftDialog`
    - `Dialogs.ViewerDraftDialog`
    - Dialogs internes: `sessionPathDialog`, `sessionDetailsDialog`, `render3dDefaultsDialog`, `aboutDialog`, `gitDialog`
    - Cadres generiques: `AppDialogFrame` pour Language + Theme
  - `Loader` -> `controlRoomShellComponent`
    - Header (titre, sous-titre, actions debug)
    - Panel onglets
      - `AppTabBar` (Files / Printers / Logs)
      - `StackLayout`
        - `Pages.CloudFilesPage`
        - `Pages.PrinterPage`
        - Host `LogPage` (loader actif en build debug, fallback sinon)
      - `InlineStatusBar` global

### 1.3 Hierarchie des pages metier
- `CloudFilesPage.qml` (1592 lignes):
  - Toolbar (refresh/upload/filtre type)
  - Carte quota
  - Table custom (header + rows + actions)
  - Pagination (rows/page, nav)
  - Overlay details fichier (dialog custom)
- `PrinterPage.qml` (1526 lignes):
  - Toolbar + auto-refresh
  - Tabs imprimantes
  - Liste fichiers cloud selectionnables
  - Panel details imprimante + historique
  - Panneau debug endpoint JSON
- `LogPage.qml` (401 lignes):
  - Filtres (niveau/source/component/event/op_id/query)
  - Viewer logs en `TextArea` mono + refresh timer 1s
- `ViewerPage.qml`:
  - `SplitView` horizontal
    - `PreviewPane`
    - `LayerInspectorPane`
    - `Viewer3DPane`
- Pages secondaires: `SettingsPage`, `DebugPage`, `CloudLoginPage` (toutes basees sur `AppPageFrame`).

### 1.4 Composants transverses (design system)
- Inputs/actions: `AppButton`, `AppTextField`, `AppComboBox`, `AppSpinBox`, `AppSlider`, `AppCheckBox`.
- Navigation: `AppTabBar`, `AppTabButton`.
- Structure: `AppPageFrame`, `AppDialogFrame`, `SectionHeader`.
- Feedback: `InlineStatusBar`, `StatusChip`, `ErrorBanner`, `ProgressCard`, `BusyOverlay`.
- Carte metier: `FileCard`.

## 2) Police / couleur / taille utilisees

### 2.1 Typographie (tokens)
Source: `accloud/ui/qml/components/Theme.js`
- `fontTitlePx = 18`
- `fontSectionPx = 14`
- `fontBodyPx = 13`
- `fontCaptionPx = 12`

### 2.2 Polices explicites relevees
- Par defaut: police Qt (non forcee dans la plupart des composants).
- Monospace explicite:
  - `"JetBrains Mono"` (`ErrorBanner`, `ProgressCard`, `InlineStatusBar`, `DebugPage`, `LogPage`)
  - `"monospace"` (cas isole dans `PrinterPage`)

### 2.3 Ecarts de taille observes (hors tokens)
- Titres specifiques: `26` (`LogPage`), `20` (`MainWindow` header), `18` (`FileCard`), `14`, `13`, `12`, `10`.

### 2.4 Couleurs (palette active WarmLight)
Source: `Theme.js`
- Fonds: `bgWindow #f7f2e8`, `bgSurface #fffaf0`, `bgDialog #fffdf7`
- Texte: `fgPrimary #2f2a21`, `fgSecondary #6a5f4f`, `fgDisabled #9a9184`
- Bordures: `borderDefault #d4c7af`, `borderSubtle #e5dccb`
- Accent: `accent #0d7f77`, `accentFg #f5fffd`
- Etats: `danger #b13b3b`, `warning #b27618`, `success #2f8a45`
- Selection: `selectionBg #d8ece8`, `selectionFg #1f2b29`
- Overlay: `overlayScrim #7a000000`

Note: preset `Dark` et accents `Coral`/`Blue` egalement disponibles.

## 3) Positionnement des elements

### 3.1 Strategie dominante
- Positionnement majoritaire par `anchors.fill` + `Layout.*` (`ColumnLayout` / `RowLayout` / `StackLayout` / `SplitView`).
- Dialogs centres via `anchors.centerIn: Overlay.overlay` avec largeur/hauteur bornes par `Math.min/Math.max` et ratio viewport.
- Peu de positionnement absolu.

### 3.2 Cas de positionnement absolu identifies
- `AppSlider`: placement de la piste et du handle via `x/y`.
- `CloudFilesPage`: table custom a colonnes fixes calculees (`colXThumb`, `colXName`, etc.) puis `x` par cellule.
- `AppComboBox` popup: `y: root.height + 4`.

### 3.3 Dimensions structurantes
- Fenetre racine: `1480x920`.
- Dialogs metier responsives:
  - Upload/Print: env. `820-1040` de large.
  - SessionSettings: `860-1120`.
  - Viewer: `980-1320`.
- Hauteur controle standard: `Theme.controlHeight = 36`.

## 4) Padding / marges / espacements

### 4.1 Tokens de reference
- `paddingPage = 16`
- `paddingDialog = 20`
- `gapRow = 12`
- `gapSection = 16`
- `radiusControl = 8`
- `radiusDialog = 12`
- `borderWidth = 1`

### 4.2 Valeurs recurrentes observees
- `padding`: `0` (dialogs), `6`, `10`, `4`, `compact ? 8 : 12` (boutons).
- `anchors.margins`: tres souvent `15` (dialogs), puis `10`, `12`, `14`, `8`, `6`.
- `spacing`: tres souvent `8`, puis `10`, `6`, `Theme.gapRow (12)`, `Theme.gapSection (16)`, `2`, `0`.

### 4.3 Lecture UX
- Densite assez homogene dans les modales (marge interne 15, spacing 8-10).
- Mix token + litteraux (risque d'incoherence long terme, surtout sur `CloudFilesPage` / `PrinterPage`).

## 5) Elements necessaires pour une note de cadrage

### 5.1 Constat de depart
- UI QML deja structuree en design system + pages metier.
- Deux pages tres volumineuses (`CloudFilesPage`, `PrinterPage`) concentrent complexite layout + logique.
- Theming centralise dans `Theme.js` avec presets et accents, mais encore des tailles/marges litterales.

### 5.2 Objectifs de cadrage proposes
- Stabiliser la cohérence visuelle (100% tokens pour typo/espace/couleur).
- Reduire la complexite des pages majeures par modularisation.
- Garder les contrats UX existants verifies par `tst_control_room.qml`.

### 5.3 Perimetre recommande
- **In scope**:
  - Refacto UI de `CloudFilesPage` et `PrinterPage` en sous-composants.
  - Harmonisation des espacements/tailles avec tokens.
  - Formalisation d'une charte UI QML (grille, espacements, tokens, composants).
- **Out of scope**:
  - Refonte fonctionnelle backend/API.
  - Rework complet du style (palette deja en place).

### 5.4 Livrables cibles
- Spec UI courte (tokens + regles layout + nomenclature composants).
- Decoupage des pages lourdes en modules QML testables.
- Matrice de regression UI (scenarios onglets/dialogs/table/liste).
- Mise a jour tests QML pour couvrir nouveaux composants.

### 5.5 Risques / points d'attention
- Regression visuelle lors de l'extraction des blocs de table/liste.
- Divergence entre style tokenise et litteraux historiques.
- Couplage logique-metier/affichage dans les gros fichiers.

### 5.6 KPI de pilotage suggerees
- % proprietes style via tokens (objectif >90%).
- Taille max d'un fichier page QML (objectif <600 lignes/page).
- Nb de composants reutilisables extraits depuis pages metier.
- Couverture tests QML sur navigation, dialogs et tables critiques.

## 6) Inventaire des fichiers analyses

### UI (`accloud/ui/qml`)
- `MainWindow.qml`
- `components/*.qml` (18 fichiers)
- `dialogs/*.qml` (4 fichiers)
- `pages/*.qml` (7 fichiers)
- `panes/*.qml` (3 fichiers)
- `components/Theme.js` (tokens et theming)

### Tests (`accloud/tests/ui/qml`)
- `tst_control_room.qml`

