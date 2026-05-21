# Décisions et points ouverts

Statut : `IMPLEMENTE` pour les décisions, `PARTIEL` pour les chantiers.

## Décisions

| ID | Décision | Raison |
| --- | --- | --- |
| D-001 | Cloud manager prioritaire. | Flux le plus implémenté et utile. |
| D-002 | MQTT = source d’état realtime. | Le suivi print dépend des transitions live. |
| D-003 | HTTP/MQTT arbitrés explicitement. | Scopes de vérité différents. |
| D-004 | HAR supporté mais secret. | Contient tokens/URLs signées. |
| D-005 | URLs signées jamais loggées complètes. | Query sensible. |
| D-006 | UI production async pour réseau/cache lourd. | Évite freeze GUI. |
| D-007 | Debug tooling gated par `ACCLOUD_DEBUG`. | Production propre. |
| D-008 | Viewer : vérité matière seuil 0 non-noir. | Les pixels AA sont matière. |
| D-009 | Géométrie principale sans dépendance contours. | Contours = analyse/export optionnel. |
| D-010 | Anglais par défaut, français maintenu. | GitHub default EN, travail projet FR. |
| D-011 | Résine interprétée selon phase. | Autoload pré-print ≠ refill runtime. |
| D-012 | Endpoints doc reliés au C++ runtime. | Snapshots historiques peuvent dériver. |

## Points ouverts

Cloud : fermer contrat sync par scope, async partout côté UI. MQTT : étendre couverture modèles et discovery redacted. UI : cacher/wirer dialogs draft, finir lazy loading. i18n : migration complète chaînes visibles. Viewer : fermer decode->mask->render et goldens. Ops : debug bundle redacted.

## Règle future

Chaque décision doit noter contexte, choix, alternatives rejetées, preuve, impact code/tests/docs.
