# Décisions techniques

> Statut : registre ACTIF. Les faits runtime doivent rester alignés sur le code compilé.


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
| D-013 | La configuration broker MQTT Anycubic validée reste figée. | Une normalisation MQTT générique peut casser la compatibilité observée. |
| D-014 | `ignoreSslErrors()` reste local au téléchargement des miniatures. | L’exception maintient un cache image non critique sans affaiblir les opérations cloud authentifiées. |
| D-015 | MQTT `VerifyNone` et OpenSSL `SECLEVEL=0` sont des contrôles de compatibilité dédiés, pas une politique SSL globale. | Les contraintes broker MQTT et l’exception miniature ont des propriétaires et risques distincts. |
| D-016 | Les URLs sont réduites à une représentation sûre avant journalisation des miniatures. | Query, fragment et userinfo peuvent exposer un accès signé ou des credentials. |
| D-017 | Le fallback TLS MQTT local est explicite et résout `<racine-du-depot>/resources/mqtt/tls`. | Le fallback silencieux masquait les erreurs de configuration et l’ancien chemin `accloud/resources/...` ne correspondait pas à l’arborescence réelle. |
| D-018 | Les données publiques de référence sont exclusivement synthétiques et contrôlées mécaniquement. | Les historiques MQTT bruts exposent des identifiants opérationnels persistants et n’ont pas leur place dans l’archive distribuable. |

## Points ouverts

Cloud : fermer contrat sync par scope, async partout côté UI. MQTT : étendre couverture modèles et discovery redacted. UI : cacher/wirer dialogs draft, finir lazy loading. i18n : migration complète chaînes visibles. Viewer : fermer decode->mask->render et goldens. Ops : debug bundle redacted.

## Règle future

Chaque décision doit noter contexte, choix, alternatives rejetées, preuve, impact code/tests/docs.
