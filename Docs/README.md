# Documentation Map

Ce fichier devient le point d'entree unique de la documentation du depot.

## Statut des pages

- `IMPLEMENTE`: decrit un comportement present dans le code actuel.
- `PARTIEL`: partiellement implemente, avec ecart connu.
- `SPEC`: cible produit/architecture, non totalement implementee.
- `SNAPSHOT`: capture datee, informative, non normative.

## Index rapide

### Vue globale
- [Etat reel vs cible](./etat_reel_vs_cible.md) - `IMPLEMENTE`
- [Checklist maintenance doc](./docs_checklist.md) - `IMPLEMENTE`

### Build / Runtime
- [Build modes debug/prod](./debug_build_modes.md) - `IMPLEMENTE`
- [Logging reference](./logging_reference.md) - `IMPLEMENTE`
- [Local cache sync strategy](./local_cache_sync_strategy.md) - `IMPLEMENTE`

### Cloud endpoints
- [Contrat endpoints](./end_points.md) - `SPEC`
- [Endpoints verifies v2](./end_points_v2_verifie.md) - `IMPLEMENTE`
- [Policy references endpoints](./endpoints_reference_policy.md) - `IMPLEMENTE`
- [Capture report endpoints](./endpoints_capture_report.md) - `SNAPSHOT`
- [Capture report endpoints (JSON)](./endpoints_capture_report.json) - `SNAPSHOT`

### Session / HAR
- [HAR import session](./har.md) - `IMPLEMENTE`

### UI (Cloud Client)
- [Vue UI centrale](./ui_views.md) - `IMPLEMENTE`
- [Spec visuelle Cloud + Remote Print](./ui_spec_cloud_remote_print_visual.md) - `SPEC`
- [Implementation lots 1 a 5](./ui_lots_1_5_implementation.md) - `IMPLEMENTE`
- [Anycubic Cloud Client (sections)](./Anycubic%20Cloud%20Client/README.md) - `IMPLEMENTE`

### UI (Photon Viewer)
- [Photon Viewer (sections)](./Photon%20Viewer/README.md) - `PARTIEL`

### i18n
- [Migration i18n synthese](./i18n/i18n_migration_synthese.md) - `PARTIEL`
- [Workflow i18n](./i18n/i18n_workflow.md) - `IMPLEMENTE`

### Formats / architecture
- [Structure application photons](./structure_application_photons.md) - `SPEC`
- [Formats Photon Anycubic](./photon_formats.md) - `SPEC`

## Regles d'entretien

1. Toute nouvelle page doit declarer son statut (`IMPLEMENTE`, `PARTIEL`, `SPEC`, `SNAPSHOT`).
2. Toute page qui decrit un comportement runtime doit pointer vers au moins un fichier code.
3. Toute capture datee doit etre marquee `SNAPSHOT` et ne doit pas etre traitee comme contrat produit.
4. Si une fonctionnalite passe de `PARTIEL` a `IMPLEMENTE`, mettre a jour ce fichier dans le meme commit.
