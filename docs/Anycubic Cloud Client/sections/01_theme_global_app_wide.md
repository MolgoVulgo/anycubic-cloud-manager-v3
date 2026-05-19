## 1) Thème global (app-wide)

Statut documentaire : `IMPLEMENTE`

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

