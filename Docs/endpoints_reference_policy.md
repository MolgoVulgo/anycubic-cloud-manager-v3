# Endpoints reference policy

Date de reference: 2026-03-08

## Statut des sources

- `Docs/end_points.md`: `SPEC`
  - Contrat transverse et catalogue large d'endpoints Anycubic.
- `Docs/end_points_v2_verifie.md`: `IMPLEMENTE`
  - Verification factuelle contre la v2 Python referencee.
- `Docs/endpoints_capture_report.md` et `.json`: `SNAPSHOT`
  - Capture datee d'une session reelle, informative, non normative.

## Priorite en cas de divergence

1. Code en execution dans ce depot (`accloud/infra/cloud`, `accloud/app/CloudBridge.cpp`).
2. `Docs/end_points_v2_verifie.md` (preuve implementation reference).
3. `Docs/end_points.md` (contrat cible/elargi).
4. `Docs/endpoints_capture_report.md` (constat ponctuel date).

## Regles de maintenance

1. Toute nouvelle capture doit etre marquee `SNAPSHOT` avec date UTC explicite.
2. Tout endpoint passe en production dans ce repo doit etre reflete dans `end_points_v2_verifie.md`.
3. `end_points.md` peut lister un endpoint non implemente, mais doit le signaler explicitement.
4. Ne jamais traiter une capture unique comme contrat API stable.

## Securite documentaire

- Redacter tokens/signatures/URLs signees dans les exemples.
- Les endpoints intrusifs (ex: `sendOrder`) doivent etre executes explicitement en mode opt-in lors des captures.
