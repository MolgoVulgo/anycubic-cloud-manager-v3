## 11) Printers Tabs / Cards

Statut documentaire : `IMPLEMENTE`

### Direction actuelle
- La navigation principale des imprimantes est en **onglets**.
- Le titre de chaque onglet contient l'information de status:
  - `PrinterName | Ready`
  - `PrinterName | Printing`
  - `PrinterName | Offline`
  - `PrinterName | Error`

### Contenu obligatoire (dans le panneau details actif)
1) **Identité**
- Nom imprimante.
- Modele / type.
- Firmware (si disponible).

2) **Statut machine**
- `StatusChip`: Offline / Ready / Printing / Error.
- Raison normalisee (avec mapping reason_catalog si possible).
- Lien d'aide (help_url) si disponible.

3) **Job**
- Nom du fichier en cours.
- Progression (pourcentage).
- Elapsed / Remaining (best-effort).

4) **Actions**
- `From Cloud File` (principal).
- `From Local File` (secondaire, listing local imprimante via `order_id=103`).

5) **Historique recent**
- Liste compacte des derniers projets de l'imprimante (task/status/progress/start/end).

### Garde-fous impression distante
- Verification compatibilite par `file_id` avant `sendOrder`.
- Blocage explicite si:
  - imprimante offline,
  - imprimante deja en cours d'impression,
  - imprimante en erreur,
  - fichier incompatible.

### Interdits
- JSON debug dans la zone principale utilisateur.
- Trop de metriques simultanees qui degradent la lisibilite.
- Status visuel sans texte.

Reference snapshot: `Docs/02_ui_qml.md`
