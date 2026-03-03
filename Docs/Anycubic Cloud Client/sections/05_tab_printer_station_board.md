## 5) View: Tab “Printer” — Station board

### But
Donner une vue “atelier” : statut des imprimantes, signalétique (online/offline/printing), détails du job courant, et un **payload preview** (debug).

### Data affichées
1) **Titre / sous-titre**
- “Printers”
- “Station board cloud: refresh, status, details, and print entrypoint.”

2) **Toolbar**
- Bouton **Refresh printers**

3) **Metrics**
- Online / Offline / Printing / Jobs history

4) **Status line**
- erreurs, résultats, “Loaded X printers…”, etc.

5) **Board split**
- Gauche : liste de cartes imprimantes (scroll)
- Droite : “Preview Payload” (JSON)

Carte imprimante :
- Nom
- Badge : OFFLINE / PRINTING / ONLINE
- Lignes : model/type, material/state, file/progress, elapsed/remaining/layers
- Action : Details

Dialog “Printer Details” : dump complet (QPlainTextEdit mono).

### Positionnement
- Layout vertical : titre → toolbar → metrics → status → board.
- Board : HBox
  - gauche (3/5) : cartes
  - droite (2/5) : preview payload.

### Thème
- Metrics = `card`
- Cartes imprimantes = `card`
- Panneau droit = `cardAlt` + `monoBlock`
- Badge = ok/warn/danger.

### Analyse
- Conforme au périmètre cloud : lecture seule + refresh + diagnostics.
- “Preview Payload” est un objet **démo** (printers sélectionnée) : utile en debug, mais pas un vrai “print entrypoint”.
- Auto-refresh activé par défaut (30s) dans la fenêtre principale : bon pour station board.

---

