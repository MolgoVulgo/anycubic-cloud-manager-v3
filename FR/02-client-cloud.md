# Client Anycubic Cloud

Statut : `PARTIEL`. Les principaux workflows cloud sont utilisables, mais le comportement Anycubic Cloud est reconstruit par observation et peut changer.

## Périmètre

Le sous-système cloud couvre : session, signature web/workbench, listing fichiers, quota, dashboard imprimantes, downloads signés, upload, print order, cache local, fallback, parsing des réponses et classification des erreurs.

## Source de vérité endpoints

La documentation endpoints doit distinguer : endpoints capturés, endpoints représentés dans le runtime C++, endpoints validés par tests ou comportement live. Seul le code C++ courant définit le comportement runtime. Les listes historiques servent à la découverte.

Pour chaque endpoint documenté, conserver : nom logique, méthode HTTP, chemin, auth/signature, payload, enveloppe de réponse, module propriétaire et statut de vérification.

## Session / HAR

L’import HAR extrait les tokens réutilisables en priorité depuis les bodies JSON, puis headers/query en fallback. Une session persistée peut être partielle ; le runtime normalise ce qui est exploitable. Le fichier session est un secret et ne doit jamais être loggé brut.

## Signature Workbench

Les appels Workbench utilisent les headers `XX-*` observés côté web UI. Les paramètres doivent rester configurables : `ACCLOUD_PUBLIC_APP_ID`, `ACCLOUD_PUBLIC_APP_SECRET`, `ACCLOUD_PUBLIC_VERSION`, `ACCLOUD_PUBLIC_DEVICE_TYPE`, `ACCLOUD_PUBLIC_IS_CN`, `ACCLOUD_REGION`, `ACCLOUD_DEVICE_ID`, `ACCLOUD_USER_AGENT`, `ACCLOUD_CLIENT_VERSION`.

## Interprétation des réponses

| Condition | Interprétation |
| --- | --- |
| HTTP `2xx` et pas de code payload, ou `code == 1` | Succès. |
| HTTP `2xx` avec `code != 1` | Erreur métier/API. |
| HTTP `401` / `403` | Session expirée ou non autorisée. |
| HTTP `429` / `5xx` | Retry borné possible avec backoff. |
| JSON invalide | Erreur API, pas succès silencieux. |

## Download

Le download se fait en deux étapes : récupérer une URL signée, puis faire un `GET` direct sans headers cloud. L’URL signée est sensible et doit être redacted dans les logs.

## Upload

Workflow observé : `lockStorageSpace`, `PUT` binaire vers `preSignUrl`, `newUploadFile`, puis `unlockStorageSpace` même en erreur partielle. Une erreur de PUT et une erreur d’enregistrement ne se traitent pas pareil.

## Cache et sync

Le cache local accélère l’UI, sert de fallback et garde la mémoire de sync. La sync doit être gouvernée par scope : fichiers, quota, imprimantes, jobs. Chaque scope doit exposer dernier succès, dernière tentative, fraîcheur, source et erreur.

Le problème central identifié reste valide : la sync fonctionne, mais le contrat doit être explicitement fermé. Un resync doit reconstruire les scopes nécessaires, pas seulement diagnostiquer.

## Règle UI

Toute opération cloud longue doit être asynchrone côté UI. QML appelle un bridge, reçoit des signaux et met à jour des modèles. Pas de réseau, SQLite ou grosse conversion JSON bloquante sur le thread GUI.

## Décision

Le client cloud est critique produit. Les captures et références historiques restent utiles, mais le runtime C++ implémenté est la référence.
