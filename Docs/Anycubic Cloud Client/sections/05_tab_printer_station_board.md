## 5) View: Tab "Printer" - Station board

### Statut
- `IMPLEMENTE` (workflow principal de production cote impression distante).

### But
- Visualiser l'etat des imprimantes
- Voir details + historique recent
- Lancer impression distante avec garde-fous de compatibilite

### Data affichees
1) Toolbar
- `Refresh printers`
- toggle debug labels

2) Navigation imprimantes
- onglets dynamiques `PrinterName | Status`

3) Device details
- identite (name/model/firmware)
- etat (`StatusChip`)
- raison normalisee (catalogue reason)
- job courant (progress/elapsed/remaining)
- infos enrichies (count/time/material/help)

4) Historique recent
- projets de l'imprimante (task/status/progress/start/end)

5) Flux remote print
- dialog `Select Cloud File`
- dialog `Remote Print Config`
- dialog `Print Config` (flags avances)
- dialog `Select Local Printer File`

### Flux boutons `Print` (source officielle)
- `From Cloud File`:
  - source cloud uniquement
  - filtre sur fichiers compatibles imprimante
- `From Local File`:
  - source stockage local imprimante uniquement
  - listing via `sendOrder(order_id=103)` puis reponse MQTT `file/listLocal`
  - impression locale: `order_id` cible observe = `1` (validation finale en cours)

Reference snapshot: `Docs/02_ui_qml.md`

### Guardrails reels
- blocage si imprimante offline/printing/error
- blocage si incompatibilite fichier vs imprimante
- verification prioritaire via `fetchCompatiblePrintersByFileId`
- message explicite utilisateur si blocage

### Positionnement
- Layout vertical: toolbar -> tabs imprimantes -> details + historique
- panneau JSON endpoint visible seulement en debug labels

### Analyse
- Aligne avec l'approche printer-centric.
- Bonne resilience via cache-first + sync cloud asynchrone.
- Auto-refresh periodique apres premier refresh cloud reussi.

---
