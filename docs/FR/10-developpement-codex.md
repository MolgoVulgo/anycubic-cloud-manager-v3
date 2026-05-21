# Règles de développement assisté par Codex

Statut : `IMPLEMENTE` comme règles de travail.

## Cadre

Projet C++20 / Qt6 / QML. Python possible pour scripts/outillage/analyse, jamais comme centre architectural.

## Règles non négociables

Ne pas inventer commandes, presets, fichiers ou tests. Lire le dépôt avant modification. Préserver CMake/presets. Ne pas committer build outputs, caches, binaires, runtime local, HAR, tokens ou clés privées. Garder QML fin. Séparer cloud, MQTT, cache, viewer. Marquer honnêtement partiel/spec.

## Commandes standard

```bash
cd accloud
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
../tools/ci/run_core_tests.sh
```

Si dépendances manquantes, le dire comme limite d’environnement, pas comme succès.

## Secrets

Redacter diagnostics, ne pas afficher tokens/URLs signées, ne pas hardcoder credentials privés, garder les constantes publiques configurables, traiter HAR comme secret.

## Politique modification

Mettre à jour la doc quand un changement touche endpoints, MQTT, UI/workflow, cache/sync, i18n, viewer, sécurité/logging.

## Décision

Codex accélère le développement. Il n’est pas source de vérité. Code, tests, captures et documentation décident.
