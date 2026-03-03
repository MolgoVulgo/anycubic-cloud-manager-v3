## 0) Vue d’ensemble (composition)

L’app est structurée autour d’une **fenêtre principale** (QMainWindow) avec :
- un **header “Control Room”** (actions globales)
- un **tabset central** : **Files / Printer / Log**
- des **dialogs** (modaux) : **Session Settings**, **PWMB 3D Viewer**, et quelques **dialogs “draft”** (Upload/Print) exposés mais non centraux.

Le produit réel (au sens “fonctionnel”) se concentre sur :
- **Files tab** (listing cloud + actions)
- **Session Settings** (import HAR)
- **PWMB 3D Viewer** (OpenGL + build progressif)
- **Printer tab** (état + détails)
- **Log tab** (tail multi-fichiers + filtres)

---

