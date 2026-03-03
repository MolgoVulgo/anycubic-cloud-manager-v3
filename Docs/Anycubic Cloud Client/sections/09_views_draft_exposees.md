## 9) Views “draft” exposées (risque UX)

### 9.1 Dialog “Upload .pwmb” (draft)

#### But
Maquette de workflow upload/auto-print/options.

#### Data affichées
- File path, Upload profile, Print target
- Options: Print after upload / Delete after print / Keep signed URL snapshot
- Actions: Browse / Close / Start upload (primary)

#### Positionnement
Form + options + boutons.

#### Thème
Standard panels.

#### Analyse
- **Non aligné avec l’état réel** : l’upload est déjà implémenté via Files tab (upload local vers cloud). Ce dialog “draft” devient une source de confusion.

### 9.2 Dialog “Send Print Order” (draft)

#### But
Maquette d’un print order direct.

#### Data affichées
- Liste imprimantes (mock)
- file_id, copies, priority, dry-run
- Actions: Preview payload / Close / Send order (primary)

#### Analyse
- Le print réel est implémenté depuis Files tab (choix imprimante via cloud + submit). Ce dialog est redondant et trompeur.

---

