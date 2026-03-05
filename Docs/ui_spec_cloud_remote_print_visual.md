# Spécification UI — Cloud + Remote Print (exploitable)

Ce document est la **source unique** pour construire l’UI : structure, composants, layout, thème, densité, états. Il intègre :

- cadrage fonctionnel (Cloud + Remote Print)
- conventions Workshop4 (printer-centric print, file details, options)
- contraintes V3 (debug_ui, data-centric)
- corrections graphiques observées (titres illisibles, contrôles dark, densité, décorations)

> Hors périmètre : Viewer 3D (bouton possible, pas de logique), contrôles machine (exposure/cleanup/infeed).

---

## 1) Architecture UI

### 1.1 Fenêtre

- **Fenêtre unique** : `Anycubic Cloud Control Room`
- Layout : `Header` + `Navigation` + `Content`

### 1.2 Navigation

- Navigation principale : **Tabs** `Files` / `Printers` / `Logs`.

### 1.3 Header (release vs dev)

Release :
- Titre app (20px)
- sous-titre (caption secondary) : statut session (ex: `Session loaded` / `No session`)
- **Aucune action métier** (pas d’upload/print).

Dev (`debug_ui=true`) :
- boutons de raccourci **autorisés** (dialogs) mais :
  - visibles uniquement en debug
  - appellent les **mêmes handlers** que le flux final

---

## 2) Design system

### 2.1 Objectif visuel

UI **warm-light** par défaut, dense, lisible, peu décorée.

- fond léger
- surfaces claires
- accent unique
- bordures fines (1 niveau)
- hiérarchie de texte explicite (primary/secondary/caption)

### 2.2 Tokens de thème (obligatoires)

Couleurs (rôles) :
- `bgWindow`, `bgSurface`, `bgDialog`
- `fgPrimary`, `fgSecondary`, `fgDisabled`
- `borderDefault`, `borderSubtle`
- `accent`, `accentFg`
- `danger`, `warning`, `success`
- `selectionBg`, `selectionFg`

Typo :
- `fontTitlePx=18` (page/dialog)
- `fontSectionPx=14`
- `fontBodyPx=13`
- `fontCaptionPx=12`

Layout :
- `paddingPage=16`
- `paddingDialog=20`
- `gapRow=12`
- `gapSection=16`
- `controlHeight=36`
- `radiusControl=8`
- `radiusDialog=12`
- `borderWidth=1`

#### 2.2.1 Couleurs de texte (règles)

Règle globale : **aucun texte secondaire ne doit utiliser `fgPrimary`.**

- **Titre app / titre page / titre dialog** : `Theme.fgPrimary`
- **Sous-titre (header, aides, descriptions courtes)** : `Theme.fgSecondary`
- **Section title (14px)** : `Theme.fgPrimary`
- **Libellés (labels de champs, colonnes table)** : `Theme.fgSecondary`
- **Valeurs (contenu principal, cellules table)** : `Theme.fgPrimary`
- **Caption (info discrète, meta, op_id)** : `Theme.fgSecondary` + `opacity: 0.9`
- **Placeholder (TextField/ComboBox)** : `Theme.fgSecondary` + `opacity: 0.75`
- **Texte désactivé** : `Theme.fgDisabled`
- **Lien/Action inline** (si utilisé) : `Theme.accent` (soulignement optionnel)

Couleurs de texte par état :
- **Success** : `Theme.success`
- **Warning** : `Theme.warning`
- **Error** : `Theme.danger`

> Les états colorés doivent rester rares : préférer `StatusChip` et `InlineStatusBar` plutôt que colorer des blocs entiers.


Couleurs (rôles) :
- `bgWindow`, `bgSurface`, `bgDialog`
- `fgPrimary`, `fgSecondary`, `fgDisabled`
- `borderDefault`, `borderSubtle`
- `accent`, `accentFg`
- `danger`, `warning`, `success`
- `selectionBg`, `selectionFg`

Typo :
- `fontTitlePx=18` (page/dialog)
- `fontSectionPx=14`
- `fontBodyPx=13`
- `fontCaptionPx=12`

Layout :
- `paddingPage=16`
- `paddingDialog=20`
- `gapRow=12`
- `gapSection=16`
- `controlHeight=36`
- `radiusControl=8`
- `radiusDialog=12`
- `borderWidth=1`

### 2.3 Palettes (presets)

Minimum :
- `WarmLight`
- `Dark`

Accent : presets (ex: Teal / Coral / Blue)

### 2.4 Paramètres → Thème

Dialog `Theme Settings` :
- `Theme preset` dropdown
- `Accent` dropdown
- Aperçu : chip accent + samples texte primary/secondary
- `Reset to defaults`
- Application live + persistance (`ui.themeName`, `ui.accentName`)

---

## 3) Composants UI (bibliothèque interne)

Ces composants doivent être utilisés partout pour éviter les écarts (ex: titres blancs, contrôles dark).

### 3.1 `AppPageFrame`

- surface unique (1 border)
- padding = `Theme.paddingPage`
- gère `SectionHeader` optionnel

### 3.2 `SectionHeader`

- titre (14px semi-bold)
- sous-texte (caption secondary) optionnel

### 3.3 `AppButton`

Variants :
- `primary` (bg accent, fg accentFg)
- `secondary` (bgSurface + border)
- `danger` (bg danger)

Height = `Theme.controlHeight`.

### 3.4 `AppTextField` / `AppComboBox`

- background = bgSurface
- text = fgPrimary
- placeholder/label = fgSecondary
- border = borderDefault
- focus ring = accent

### 3.5 `AppDialogFrame`

Composant obligatoire pour **toutes** les fenêtres modales.

#### Structure
- **Scrim** (fond assombri derrière le modal)
- **Dialog surface** (contenu)
  - Header
  - Body
  - Footer

#### Couleurs / textes
- `title.color = Theme.fgPrimary`
- `subtitle.color = Theme.fgSecondary`
- labels : `Theme.fgSecondary`
- valeurs : `Theme.fgPrimary`

#### Décoration (modal)

**Scrim**
- couleur : `#000000`
- opacité : `0.35` (WarmLight) / `0.45` (Dark)
- effet : clic sur scrim = **close** uniquement si dialog non-destructif (sinon ignore)

**Surface dialog**
- background : `Theme.bgDialog`
- radius : `Theme.radiusDialog`
- border : `Theme.borderWidth` + `Theme.borderDefault`
- ombre : `DropShadow` subtil
  - `color: "#000000"`, `opacity: 0.18`
  - `blurRadius: 24`, `verticalOffset: 10`, `horizontalOffset: 0`

**Header**
- padding : `Theme.paddingDialog`
- divider bas : 1px `Theme.borderSubtle` (optionnel si header très léger)
- bouton close : en haut à droite, style `secondary` compact (ou icône)

**Body**
- padding : `Theme.paddingDialog`
- espacement interne : `Theme.gapRow` / `Theme.gapSection`

**Footer**
- padding : `Theme.paddingDialog`
- divider haut : 1px `Theme.borderSubtle`
- boutons alignés à droite
- destructive (`Delete`) à gauche ou isolé visuellement, variant `danger`

#### Règles
- aucun titre blanc
- pas de rectangles décoratifs imbriqués dans le dialog
- pas de tailles fixes inutiles (laisser le contenu dicter la hauteur)
- focus visible (tab/focus ring accent sur controls)


### 3.6 `InlineStatusBar`

Barre compacte (32–36px) :
- icône + message à gauche
- `op_id` (caption) à droite
- couleurs : info/success/warn/error (subtiles)

### 3.7 `StatusChip`

- texte + couleur (pas de pill muet)
- mapping :
  - `Offline` = gris
  - `Ready/Free` = vert/neutre
  - `Printing` = accent
  - `Error` = danger

---

## 4) Page Files — Cloud Files

### 4.1 Toolbar

Gauche :
- `Refresh` (secondary)
- `Upload` (primary) — **upload local → cloud**

Droite :
- filtre `Type` (combo)

### 4.2 Quota

Bloc compact (44–48px) :
- texte : `Used / Total`, `Free`, `Files`
- progress bar

### 4.3 Status (opération)

`InlineStatusBar` sous quota :
- ex: `21 file(s) loaded` + `op_files_refresh`

### 4.4 Listing (mode dense)

Référence : table / rows (style Workshop4).

Colonnes :
- checkbox
- thumbnail (48)
- `File name`
- `Type`
- `Size`
- `Date`
- `Status`
- Actions

Row height : 56–64.

Actions (à droite) :
- `Details` (secondary)
- `Download` (secondary)
- `Print` (primary)
- menu `…` : `Rename`, `Delete`

### 4.5 File Details (dialog)

Dialog `File Details` (AppDialogFrame) :

Header :
- thumbnail + nom
- `Rename` (secondary) (ou icône crayon)

Tabs :
- `Basic Information`
- `Slice Settings`

Footer actions :
- gauche : `Delete` (danger)
- droite : `Download` (secondary), `Print` (primary), `Close` (secondary)

---

## 5) Page Printers

### 5.1 Toolbar

- `Refresh printers` (secondary)

### 5.2 KPIs (option)

- `Online`, `Offline`, `Printing`, `Jobs`
- style : cards compactes (hauteur 44–48)

### 5.3 Listing imprimantes

Chaque item affiche :
- nom
- modèle
- `StatusChip` (Offline/Ready/Printing)
- résumé job si printing (progress + time best-effort)

### 5.4 Détails imprimante (device details)

Entrée via `Details`.

Affiche :
- statut
- job en cours : file/progress/elapsed/remaining (si dispo)
- Remote print entrypoints :
  - `From Cloud File`
  - `From Local File`

> Les contrôles machine avancés restent hors scope.

### 5.5 Contrat “Details” (pas de bouton perdu)

- `Details` est **dans chaque item** (recommandé) OU
- global uniquement si sélection active (disabled sinon).

---

## 6) Remote Print (workflow)

### 6.1 Principe Workshop4

Remote print est **printer-centric** :
- on est dans l’imprimante → on choisit cloud/local → on configure → start.

### 6.2 Select Cloud File (dialog)

Dialog `Select Cloud File` :
- liste filtrée par compatibilité (type imprimante)
- sélection unique (radio)
- CTA `Start Printing` (primary) désactivé sans sélection

### 6.3 Remote Print config (dialog)

Sections :
- `Print Task` (thumb, task name, printer, estimated time, resin volume)
- `Select Printer` + `Change`
- `Options` : flags + lien `More`

CTA : `Start Printing` (primary)

### 6.4 Print Config (More)

Dialog `Print Config` :
- liste de flags (booléens) + description

---

## 7) Page Logs

### 7.1 Objectif

Observation et diagnostic.

### 7.2 UI

- barre filtres (controlHeight=36)
- zone log mono-font, taille body/caption

---

## 8) États UI (contrat)

Pour chaque page :
- `loading` → skeleton / spinner discret
- `empty` → message caption
- `error` → InlineStatusBar error + message
- `ready`

Règle : messages courts + `op_id` copiable.

---

## 9) Règles anti-régression (visuel)

1. Aucun texte secondaire ne peut être `fgPrimary`.
2. Aucun titre de dialog ne peut être blanc.
3. Aucun contrôle Qt “dark” par défaut n’apparaît en WarmLight : tout passe par `App*` wrappers.
4. Pas de double titrage (header + page).
5. 1 seul niveau de bordure par surface.
6. `Print` doit être visuellement primary partout.
7. `Offline` ne doit pas être rouge.

---

## 10) Checklist UI (DoD)

- Theme Settings fonctionne (preset + accent, live + persistance)
- Files : listing dense + actions + File Details conforme
- Printers : listing + StatusChip + Details + entrypoints remote print
- Remote print : select file + config + more
- Dialogs : header lisible + footer cohérent
- Aucune incohérence dark/light

