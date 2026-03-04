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
  - colonne droite : 3 boutons (Print Dialog, 3D Viewer Dialog, Upload Dialog)
- **MenuBar top** :
  - `Session` : `Details`, `import HAR`
  - `Parametre` : `session`, `theme`, `rendu 3d`
  - `?` : `A propos`, `git`
- **Tabs** sous le header, occupant le reste de la fenêtre.

### Thème
- Header = `panel` (bg_panel)
- Boutons primary = `Upload Dialog` (primary)
- Fenêtre root = gradient global

### Analyse
- Conforme au besoin “fenêtre principale centrée Cloud” (accès direct aux fichiers + actions).
- Les actions session passent par le menu top (plus de bouton `Session Settings` dans le header).
- Les boutons **Upload Dialog** et **Print Dialog** ouvrent des **dialogs “draft”** (design preview) : risque de confusion avec les actions réelles (upload/print déjà disponibles dans Files tab).
- Le viewer est accessible globalement (OK) + depuis les cartes fichiers (mieux, car contextuel).

---
