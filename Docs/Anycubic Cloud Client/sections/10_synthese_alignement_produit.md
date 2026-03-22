## 10) Synthese d'alignement produit (CDF -> UI) - Anycubic Cloud Client

### Statut
- `IMPLEMENTE` pour le coeur Cloud manager.
- `PARTIEL` pour les ecrans draft exposes en raccourci header.

### Alignement confirme (etat reel)
- Cloud-first: `CloudFilesPage.qml` + `PrinterPage.qml` portent les flux metier principaux.
- Session/HAR: import et validation session operationnels via `SessionSettingsDialog.qml` et `SessionImportBridge`.
- UI reactive: chargements asynchrones et fallback cache/cloud (pas de blocage UI sur refresh principal).
- Observabilite runtime: onglet Logs disponible en build debug (`ACCLOUD_DEBUG=ON`).

### Ecarts fonctionnels encore ouverts
- Upload depuis Files: bouton present mais pipeline upload complet encore incomplet dans cette vue.
- Vues draft header (`Upload`, `Print`, `3D Viewer`) encore accessibles et potentiellement confondables avec les flux production.
- Actions imprimante avancees (pause/resume/stop) non exposees dans le flux courant.

### Position produit actuelle
- Flux recommande: `Files -> Printers` pour impression distante avec guardrails de compatibilite.
- Les dialogs draft doivent etre consideres comme outils demo/debug, pas comme parcours nominal.

### Decision documentaire
- `Docs/docs_unifies_ui_qml.md` et cette section servent de reference UI implementation.
- Toute doc SPEC qui decrit des parcours viewer/upload complets doit expliciter l'ecart avec cet etat reel.

---
