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

