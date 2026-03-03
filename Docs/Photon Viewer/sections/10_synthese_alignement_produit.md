## 10) Synthese d'alignement produit (CDF -> UI) — Photon Viewer

Source de base: section 10 de `Docs/ui_views.md`.

- **Viewer 3D**: OK (build progressif, cancel, cache, LOD via quality presets).
- **UI reactive**: OK (pipeline async + polling, pas d'operation lourde sur le thread UI).
- **Fidelite rendu**: OK (verite matiere via threshold minimal + bin_mode strict dans la vue 3D).
- **Points de vigilance UX**: acces viewer en global + contextuel (Files tab) a clarifier selon strategie cloud-only.
- **Dette produit liee au viewer**: consolider les flux reels et masquer les dialogs "draft" redondants depuis le header.
