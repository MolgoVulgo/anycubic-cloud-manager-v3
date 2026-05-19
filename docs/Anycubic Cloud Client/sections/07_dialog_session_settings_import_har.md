## 7) View: Dialog “Session Settings” — Import HAR

Statut documentaire : `IMPLEMENTE`

### But
Importer une session (tokens) depuis un fichier HAR et persister en `session.json`.

### Data affichées
- Champ HAR file + bouton Browse
- Session target affiché en lecture seule (source: `Parametre > session`)
- Bloc “Security reminders”
- TextArea de résultat d’analyse (tokens détectés, stats, message)
- Status message (résultat analyse/commit)
- Action : `Fermer` / `Fermer et sauvegarder` (selon validité)

### Positionnement
- Form panel (HAR uniquement)
- Info panel (rappels sécurité)
- Status line
- Boutons en bas.

### Thème
- Panels `panel` et `cardAlt`
- Actions footer = bouton de fermeture contextuel (`Fermer` / `Fermer et sauvegarder`)

### Analyse
- Workflow actuel:
  - sélection HAR
  - analyse immédiate (sans persistance)
  - persistance de `session.json` à la fermeture si l’analyse est valide
  - vérification cloud après sauvegarde
- Le discours “Only token headers are imported…” est approximatif : l’import prend aussi des tokens de bodies JSON si présents (comportement utile, mais la phrase peut être trompeuse).

---
