## 10) Synthese d'alignement produit (CDF -> UI) — Anycubic Cloud Client

Source de base: section 10 de `Docs/ui_views.md`.

- **Cloud-first**: OK (Files/Printer relies API + fallback cache local).
- **HAR import**: OK (workflow central, auto-ouverture si session invalide au startup).
- **UI reactive**: OK (threads + timers, pas d'operations lourdes sur le thread UI).
- **Manques fonctionnels (MVP)**: pagination/tri/recherche fichiers, actions imprimante (pause/resume/stop), upload pipeline complet lock/presign/register/unlock.
- **Dette UX**: dialogs "draft" exposes dans le header alors que les flows reels sont dans Files tab.
