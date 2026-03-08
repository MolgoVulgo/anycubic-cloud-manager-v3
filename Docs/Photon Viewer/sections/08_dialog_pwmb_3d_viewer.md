## 8) View: Dialog "PWMB 3D Viewer" - Draft dialog

### Statut
- `PARTIEL` / `DRAFT`.

### Role actuel
- Fournir une maquette de dialogue viewer 3D (`ViewerDraftDialog.qml`).
- Exposer des controles UI (path, quality, palette, contour-only) sans pipeline rendu 3D branche au backend.

### Elements visibles
- Champ `Source PWMB path` (placeholder).
- Slider `Layer cutoff`.
- Presets qualite `33/66/100`.
- Palettes (`Steel`, `Resin`, `Heat`).
- Toggle `Contour only`.
- Barre de progression de demonstration.
- Zone viewport: texte `OpenGL viewport placeholder`.
- Boutons: `Rebuild`, `Cancel`, `Retry`, `Reset camera`, `Export screenshot`, `Close`.

### Limites reelles
- Pas de decode PWMB ni extraction de geometry dans ce dialog.
- Pas de rendu OpenGL produit.
- Boutons hors `Close` sans flux metier final connecte.

### Analyse
- Ce dialog est utile pour cadrer l'UX cible.
- Il ne doit pas etre presente comme viewer de production.
- Pour la production, l'etat reel reste `ViewerPage.qml` + panes placeholders, donc couverture viewer globale encore `PARTIEL`.

---
