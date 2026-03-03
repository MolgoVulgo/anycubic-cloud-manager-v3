# UI Views — Anycubic Cloud Client + PWMB Viewer (Qt)

## 0) Vue d’ensemble (composition)

L’app est structurée autour d’une **fenêtre principale** (QMainWindow) avec :
- un **header “Control Room”** (actions globales)
- un **tabset central** : **Files / Printer / Log**
- des **dialogs** (modaux) : **Session Settings**, **PWMB 3D Viewer**, et quelques **dialogs “draft”** (Upload/Print) exposés mais non centraux.

Le produit réel (au sens “fonctionnel”) se concentre sur :
- **Files tab** (listing cloud + actions)
- **Session Settings** (import HAR)
- **PWMB 3D Viewer** (OpenGL + build progressif)
- **Printer tab** (état + détails)
- **Log tab** (tail multi-fichiers + filtres)

---

## 1) Thème global (app-wide)

### But
Uniformiser la lecture et la hiérarchie visuelle (titre → sous-titre → panneaux → cartes) + signalétique d’état (primary / danger / warn / ok).

### Data affichées
N/A (thème).

### Positionnement
- Palette “papier” (fonds chauds), panneaux arrondis, bordures douces.
- QPalette + stylesheet global (Fusion + overrides).

### Thème
- Fond : beige/ivoire (root/panel/card/cardAlt) + dégradés.
- Accent principal : **teal** (boutons primary, sélection, highlight).
- États : **danger** (rouge), **warn** (ambre), **ok** (vert).
- Typo : sans (IBM Plex / Source Sans / Noto) + mono (JetBrains Mono / Fira Code).

### Analyse
- Cohérent, lisible, peu de bruit.
- Style homogène via `objectName` (panel/card/cardAlt/title/subtitle/monoBlock).
- Les miniatures ont un style local (gradient vert) qui tranche : volontaire mais “hors palette” (à assumer ou à aligner).

---

## 2) View: Fenêtre principale — “Anycubic Cloud Control Room”

### But
Point d’entrée : exposer les **actions globales** (session, viewer, dialogs) et la **navigation** entre les 3 zones fonctionnelles (Files/Printer/Log).

### Data affichées
- Titre : “Anycubic Cloud Control Room”
- Sous-titre de statut (phase / message)
- Tabs : Files / Printer / Log

### Positionnement
- **Header** en haut (QFrame panel), layout horizontal :
  - colonne gauche : titre + sous-titre
  - colonne droite : 4 boutons (Session Settings, Print Dialog, 3D Viewer Dialog, Upload Dialog)
- **Tabs** sous le header, occupant le reste de la fenêtre.

### Thème
- Header = `panel` (bg_panel)
- Boutons primary = `Upload Dialog` (primary)
- Fenêtre root = gradient global

### Analyse
- Conforme au besoin “fenêtre principale centrée Cloud” (accès direct aux fichiers + actions).
- Les boutons **Upload Dialog** et **Print Dialog** ouvrent des **dialogs “draft”** (design preview) : risque de confusion avec les actions réelles (upload/print déjà disponibles dans Files tab).
- Le viewer est accessible globalement (OK) + depuis les cartes fichiers (mieux, car contextuel).

---

## 3) View: Tab “Files” — Cloud Files

### But
Lister les fichiers cloud, exposer les actions : **refresh**, **upload**, **download**, **delete**, **details**, **print**, **ouvrir viewer 3D** (si PWMB).

### Data affichées
1) **Titre / sous-titre**
- “Cloud Files”
- “Vue condensée: quota, fichiers et miniatures (100x100).”

2) **Toolbar**
- Bouton **Refresh**
- Bouton **Upload .pwmb** (primary)

3) **Quota summary**
- `used / total (percent) | free | files count`

4) **Status line**
- messages de refresh partiel, cache local, erreurs, succès, etc.

5) **Listing** (cartes)
Chaque carte fichier affiche :
- Miniature 100×100 (thumbnail_url si dispo, sinon placeholder)
- Nom fichier (title)
- “Delete” (danger)
- Détails “courts” :
  - Layers
  - Print time
  - Upload time
  - Layer thickness
- Détails “étendus” (si présents) :
  - Material / Machine / Resin usage / Size XYZ
- Meta :
  - Size (human readable)
  - Id
  - Status
- Actions : **Details**, **Print**, **Download**, **Open 3D Viewer** (uniquement si extension .pwmb)

### Positionnement
- Layout vertical : titre → toolbar → quota → status → panel scroll.
- Zone cartes : `panel` + `QScrollArea` (liste verticale).
- Carte : HBox
  - gauche : vignette fixe 100×100
  - droite : contenu (VBox) + actions en bas.

### Thème
- Cartes : `cardAlt` (fond alterné)
- Delete : bouton `danger`
- Upload : bouton `primary`
- Miniature : fond gradient vert + border dédiée.

### Analyse
- Conforme à l’esprit CDF “listing cloud + thumbnail + actions”.
- UX “non bloquante” : refresh/upload/download/delete/print → thread + polling timer (pas de freeze UI).
- Thumbnail : téléchargement asynchrone avec cache + TTL + limitation de concurrence (semaphore 4). Re-render complet du listing à chaque rafale de miniatures : acceptable en v0, mais peut faire “micro-jitter” sur gros listings.
- Périmètre actuel : page 1 / 20 fichiers (pas de pagination, pas de filtre/tri). Cohérent pour un MVP, mais limite la valeur quand le cloud est chargé.

---

## 4) View: Dialog “File Details” (depuis Files)

### But
Afficher un dump lisible des métadonnées (général/slicing/cloud) d’un fichier cloud.

### Data affichées
- Section [General] : name, file_id, extension, size, gcode_id, status, status_code
- Section [Slicing] : print time, layers, thickness, machine, material, resin usage, dimensions, bottom layers, exposure/off time, printers, md5
- Section [Cloud] : upload/created/updated, thumbnail URL, download URL, region/bucket/path

### Positionnement
- QDialog, taille ~760×460.
- Titre (label) + body (QPlainTextEdit monoBlock) + Close.

### Thème
- Body = `monoBlock` (monospace).

### Analyse
- Très “debug-friendly” (bon pour reverse/diagnostic).
- Aucun lien cliquable (URL affichées en texte). OK si contrainte “pas de fuite”, mais peut être frustrant.

---

## 5) View: Tab “Printer” — Station board

### But
Donner une vue “atelier” : statut des imprimantes, signalétique (online/offline/printing), détails du job courant, et un **payload preview** (debug).

### Data affichées
1) **Titre / sous-titre**
- “Printers”
- “Station board cloud: refresh, status, details, and print entrypoint.”

2) **Toolbar**
- Bouton **Refresh printers**

3) **Metrics**
- Online / Offline / Printing / Jobs history

4) **Status line**
- erreurs, résultats, “Loaded X printers…”, etc.

5) **Board split**
- Gauche : liste de cartes imprimantes (scroll)
- Droite : “Preview Payload” (JSON)

Carte imprimante :
- Nom
- Badge : OFFLINE / PRINTING / ONLINE
- Lignes : model/type, material/state, file/progress, elapsed/remaining/layers
- Action : Details

Dialog “Printer Details” : dump complet (QPlainTextEdit mono).

### Positionnement
- Layout vertical : titre → toolbar → metrics → status → board.
- Board : HBox
  - gauche (3/5) : cartes
  - droite (2/5) : preview payload.

### Thème
- Metrics = `card`
- Cartes imprimantes = `card`
- Panneau droit = `cardAlt` + `monoBlock`
- Badge = ok/warn/danger.

### Analyse
- Conforme au périmètre cloud : lecture seule + refresh + diagnostics.
- “Preview Payload” est un objet **démo** (printers sélectionnée) : utile en debug, mais pas un vrai “print entrypoint”.
- Auto-refresh activé par défaut (30s) dans la fenêtre principale : bon pour station board.

---

## 6) View: Tab “Log” — Runtime Logs

### But
Observer le runtime en live : tail multi-sources, filtres par niveau/source/component/event/op_id + recherche texte.

### Data affichées
- Filtre level (ALL, INFO+, etc.)
- Filtre source (All sources/app/http/fault/render3d)
- Filtre component/event (alimentés dynamiquement)
- op_id exact
- query (contains)
- Zone de texte : lignes rendues (JSON ou fallback “best-effort”).

### Positionnement
- Barre filtres en haut (panel)
- Zone logs (cardAlt) en dessous, occupant la hauteur.
- Poll 1s.

### Thème
- Zone logs = `monoBlock`
- Filtres = QComboBox/QLineEdit style global.

### Analyse
- Très utile pour l’approche “op_id / req_id / events”.
- Résilient aux rotations/truncations (détecte inode/size).
- Cache 1000 lignes : suffisant pour du live, mais pas pour l’analyse longue (à faire offline sur fichiers).

---

## 7) View: Dialog “Session Settings” — Import HAR

### But
Importer une session (tokens) depuis un fichier HAR et persister en `session.json`.

### Data affichées
- Champ HAR file + bouton Browse
- Champ Session target (path)
- Bloc “Security reminders”
- Status message (résultat import/validation)
- Actions : Close / Import HAR

### Positionnement
- Form panel (HAR + target)
- Info panel (rappels sécurité)
- Status line
- Boutons en bas.

### Thème
- Panels `panel` et `cardAlt`
- Import = bouton `primary`

### Analyse
- Aligne bien le workflow : import → merge → save → validation.
- Le discours “Only token headers are imported…” est approximatif : l’import prend aussi des tokens de bodies JSON si présents (comportement utile, mais la phrase peut être trompeuse).

---

## 8) View: Dialog “PWMB 3D Viewer” — OpenGL viewport

### But
Afficher un PWMB en 3D via pipeline progressif :
- **Pass 1** contours
- **Pass 2** fill
Avec **progress**, **annulation**, **cache**, **qualité (sampling)** et contrôle caméra.

### Data affichées
- Source PWMB (path) + Browse
- Layer cutoff (slider 0..N)
- Quality preset (33/66/100)
- Palette (plusieurs palettes)
- Toggle “Contour only”
- Info status (idle/progress/errors + stats build)
- Progress bar (0..100)
- Viewport OpenGL : rendu (lignes/triangles/points)
- Actions : Rebuild / Retry / Cancel / Reset camera / Export screenshot / Close

### Positionnement
- En-tête texte (titre + aide interactions)
- Split horizontal :
  - gauche : `panel` controls (min width 320)
  - droite : viewport (QOpenGLWidget / fallback)
- Barre boutons en bas.

### Thème
- Controls panel = `panel`
- UI standard; rendu OpenGL gère ses propres couleurs via “Palette”.

### Analyse
- Conforme aux exigences “UI non bloquante” : build sur runner + progress queue + cancel token + polling 80ms.
- “Vérité matière” : le viewer utilise un threshold minimal (équivalent non-noir) + bin_mode strict ; cohérent pour un rendu fidèle.
- Le viewer peut être ouvert de 2 façons :
  - **depuis un fichier cloud** : resolve → download cache → build auto
  - **depuis un chemin local** : Browse → build
  Si l’objectif produit est “cloud-only” au niveau UX, il faudra masquer/désactiver Browse quand un `file_label` cloud est fourni.
- Retry last build bien cadré (réutilise la dernière config) + messages d’erreur classés (OpenGL / decode / parse).

---

## 9) Views “draft” exposées (risque UX)

### 9.1 Dialog “Upload .pwmb” (draft)

#### But
Maquette de workflow upload/auto-print/options.

#### Data affichées
- File path, Upload profile, Print target
- Options: Print after upload / Delete after print / Keep signed URL snapshot
- Actions: Browse / Close / Start upload (primary)

#### Positionnement
Form + options + boutons.

#### Thème
Standard panels.

#### Analyse
- **Non aligné avec l’état réel** : l’upload est déjà implémenté via Files tab (upload local vers cloud). Ce dialog “draft” devient une source de confusion.

### 9.2 Dialog “Send Print Order” (draft)

#### But
Maquette d’un print order direct.

#### Data affichées
- Liste imprimantes (mock)
- file_id, copies, priority, dry-run
- Actions: Preview payload / Close / Send order (primary)

#### Analyse
- Le print réel est implémenté depuis Files tab (choix imprimante via cloud + submit). Ce dialog est redondant et trompeur.

---

## 10) Synthèse d’alignement produit (CDF → UI)

- **Cloud-first** : OK (Files/Printer basés sur API + cache local fallback).
- **HAR import** : OK (workflow central, auto-ouverture si session invalide au startup).
- **Viewer 3D** : OK (progress, cancel, cache, LOD via quality presets).
- **UI réactive** : OK (threads + timers, pas d’opérations lourdes sur thread UI).
- **Manques fonctionnels (MVP assumés)** : pagination/tri/recherche fichiers, actions imprimante (pause/resume/stop), upload “pipeline complet” avec lock/presign/register/unlock exposé en UI.
- **Dette UX** : dialogs “draft” exposés dans le header alors que les flows réels sont ailleurs.

