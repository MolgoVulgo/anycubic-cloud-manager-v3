## 2) View: Fenetre principale - "Anycubic Cloud Control Room"

Statut documentaire : `IMPLEMENTE`

### Statut
- `IMPLEMENTE` (avec actions draft visibles en mode debug-ui).

### But
Point d'entree global pour:
- navigation `Files / Printers / Logs`
- actions session (menu)
- reglages theme/langue/rendu par defaut

### Data affichees
- Titre: `Anycubic Cloud Control Room`
- Sous-titre de statut runtime/session
- Tabs: `Files`, `Printers`, `Logs`

### Positionnement
- Header en haut:
  - gauche: titre + sous-titre
  - droite: boutons raccourcis `Print Dialog`, `3D Viewer Dialog`, `Upload Dialog`
- MenuBar:
  - `Session`: `Details`, `import HAR`
  - `Settings`: `Session`, `Theme`, `3D rendering`, `Language`
  - `?`: `About`, `git`
- Contenu principal: tabs + status bar inline globale

### Comportement reel
- Les boutons header sont surtout des raccourcis vers dialogs draft.
- Ils sont pertinents surtout en `--debug-ui`.
- Les flux de prod restent dans les tabs `Files` et `Printers`.

### Analyse
- Architecture claire et stable pour l'usage Cloud manager.
- Risque UX connu: confusion possible entre raccourcis draft et flux metier reels.

---
