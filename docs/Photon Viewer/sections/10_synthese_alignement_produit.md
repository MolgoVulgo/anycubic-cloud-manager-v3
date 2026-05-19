## 10) Synthese d'alignement produit (CDF -> UI) - Photon Viewer

Statut documentaire : `PARTIEL`

### Statut
- `PARTIEL`.

### Etat reel
- `ViewerDraftDialog.qml`: maquette UI de dialog viewer.
- `ViewerPage.qml`: composition en trois panes (`PreviewPane`, `LayerInspectorPane`, `Viewer3DPane`).
- Les panes sont encore des placeholders (pas de pipeline decode+render OpenGL final dans ce depot).

### Ecart vs cible produit
- Cible attendue: viewer 3D PWMB non bloquant avec build progressif, annulation, camera et export.
- Etat actuel: composants visuels et structure de page presentes, logique metier viewer encore inachevee.

### Consequence documentaire
- Toute mention de viewer "operationnel" doit etre marquee comme `SPEC` tant que le pipeline backend+rendu n'est pas livre.
- Les docs Cloud Client doivent presenter le viewer comme draft/debug, pas comme flux principal.

---
