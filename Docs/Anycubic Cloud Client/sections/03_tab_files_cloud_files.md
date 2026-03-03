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

