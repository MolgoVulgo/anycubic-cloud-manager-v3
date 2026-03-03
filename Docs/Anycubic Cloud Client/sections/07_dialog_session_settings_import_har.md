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

