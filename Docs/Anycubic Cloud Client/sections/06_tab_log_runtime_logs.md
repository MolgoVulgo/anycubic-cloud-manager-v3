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

