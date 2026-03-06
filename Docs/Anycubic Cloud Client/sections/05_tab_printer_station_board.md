## 5) View: Tab “Printer” — Station board

### But
Donner une vue “atelier” : statut des imprimantes, signalétique (online/offline/printing), details enrichis, historique des projets, et entrypoint d'impression distante avec garde-fous.

### Data affichées
1) **Titre / sous-titre**
- “Printers”
- “Station board cloud: refresh, status, details, and print entrypoint.”

2) **Toolbar**
- Bouton **Refresh printers**

3) **Status line**
- erreurs, résultats, “Loaded X printers…”, etc.

4) **Navigation imprimantes**
- Barre d'onglets horizontale (1 onglet par imprimante).
- Titre d'onglet: `PrinterName | Status` (ex: `Anycubic Photon Mono M7 Pro | Printing`).
- Le status de l'imprimante est visible directement dans le titre d'onglet.

5) **Panneau Device Details**
- Identite: nom, modele, firmware.
- Statut: chip + raison normalisee (`reason_catalog` si resolu).
- Job courant: fichier/progress/elapsed/remaining.
- Infos enrichies: print_count, print_totaltime, material_used, liens help/quick-start.
- Capacites: `tools`, `advance` (liste compacte).
- Historique recent: `getProjects?printer_id=...` (task, status, progress, start/end).

6) **Remote print guardrails**
- Avant `sendOrder`, verification de compatibilite par `file_id` via `v2/printer/printersStatus?file_id=...`.
- Blocage explicite si imprimante offline/printing/error/incompatible.
- Message utilisateur visible dans le dialog de configuration d'impression.

7) **Debug**
- Panneau JSON endpoint conserve en mode debug.

### Positionnement
- Layout vertical: toolbar -> status -> tabs -> details -> debug (optionnel).

### Thème
- Tabs = `card` surface.
- Device details = `card`.
- Debug payload = `cardAlt` + `monoBlock`.
- Status chip = ok/warn/danger.

### Analyse
- Conforme au perimetre cloud: refresh, diagnostics, print remote controle.
- Le flux principal repose sur `getPrinters` (+ projets actifs) et reserve les endpoints v2 pour enrichissement/guardrails.
- Le mode debug reste optionnel et n'impacte pas le flux utilisateur standard.

---
