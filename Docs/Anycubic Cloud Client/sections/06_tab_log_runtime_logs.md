## 6) View: Tab "Log" - Runtime Logs

Statut documentaire : `PARTIEL`

### Statut
- `IMPLEMENTE` en build debug (`ACCLOUD_DEBUG=ON`).
- En build prod: vue remplacee par un panneau explicatif (logs viewer desactive).

### But
Observer le runtime en live avec filtres operationnels:
- niveau
- source
- component
- event
- op_id
- recherche texte

### Data affichees
- Filtres dynamiques depuis les sinks JSONL
- Zone log mono (tail)
- Indicateur mode (`Live mode` / `Demo mode`)

### Positionnement
- panel filtres en haut
- zone logs scrollable en dessous
- polling periodique 1s

### Comportement reel
- Si `logBridge` present: snapshot des `*.jsonl` en local
- Sinon: fallback demo
- Sources/components/events detectees dynamiquement

### Analyse
- Tres utile pour debug et correlation `op_id`.
- Comportement explicitement conditionne au flag build debug.

---
