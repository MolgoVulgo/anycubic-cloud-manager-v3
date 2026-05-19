## 9) Views draft exposees (risque UX)

Statut documentaire : `PARTIEL`

### Statut
- `PARTIEL`.

### 9.1 Upload dialog (draft)

#### Role actuel
- Maquette UI uniquement.
- Non connectee a un pipeline upload cloud complet dans cette vue.

#### Risque
- Peut etre confondu avec le flux metier reel Files/Printers.

### 9.2 Send Print Order dialog (draft)

#### Role actuel
- Maquette de payload direct.
- Le flux reel d'impression est printer-centric dans `PrinterPage`.

#### Risque
- Redondant avec `Select Cloud File` + `Remote Print Config`.

### 9.3 Viewer dialog (draft)

#### Role actuel
- Placeholder visuel de controle viewer.
- Pas de pipeline rendu OpenGL produit dans ce dialog.

### Recommendation
- Conserver ces vues pour debug/demo uniquement.
- Eviter de les presenter comme flux de production.

---
