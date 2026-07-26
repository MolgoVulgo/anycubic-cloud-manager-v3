# Développement, tests et correctifs

## En bref

Corriger le propriétaire actif du comportement, valider uniquement le contrat touché et livrer un patch autoportant. Ne pas réparer une dette hors périmètre ni modifier les protocoles Anycubic observés par convention.

## Commandes standard

Depuis `accloud/` :

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

Exemples ciblés :

```bash
ctest --preset default -R '^accloud_har_import$' --output-on-failure
ctest --preset default -R '^accloud_security_redaction$' --output-on-failure
ctest --preset default -R '^accloud_mqtt_flow$' --output-on-failure
ctest --preset default -R '^accloud_ui_qml' --output-on-failure
```

Garde documentation et archive :

```bash
python ../tools/check_documentation_contract.py --repo-root ..
ctest --preset default -R '^accloud_documentation_contract$' --output-on-failure
```

La garde valide les paires bilingues, les liens locaux, les constantes MQTT/SSL figées, l’unique portée miniature de `ignoreSslErrors()`, les ressources QML actives, l’unicité des catalogues TS et le caractère exclusivement synthétique des données publiques de référence.

Ne pas inventer de commande absente de CMake. Les tests broker live exigent un environnement contrôlé et ne sont jamais couverts implicitement par un test unitaire local.

## Avant modification

1. Lire `AGENTS.md` et le document principal de la zone.
2. Vérifier la source compilée et le point d'entrée runtime.
3. Identifier le module propriétaire.
4. Distinguer actif, expérimental, legacy, test et référence.
5. Préserver les contrats cloud, MQTT, sécurité et thread GUI.
6. Limiter le changement.

## Produire un patch

Les règles complètes sont dans `regles-generales-production-correctifs.md`.

```text
analyser
-> modifier le périmètre strict
-> validation ciblée
-> construire le ZIP
-> contrôler mécaniquement le ZIP
-> rapporter validations exécutées et manquantes
```

Un patch contient `PATCH_MANIFEST.md`, `DELETE_FILES.txt`, `MOVE_FILES.txt` et uniquement les fichiers projet modifiés avec leurs chemins relatifs.

## Appliquer un patch

`codex-patch-mode.md` s'applique seulement lorsqu'un patch ZIP complet existe déjà.

```text
inspecter le manifeste
-> vérifier chemins et contenus
-> appliquer exactement déplacements/suppressions/remplacements
-> exécuter les validations demandées
-> arrêter au premier échec obligatoire
```

L'applicateur ne redéfinit pas le patch et ne modifie pas le code pour faire passer un test.

## Politique d'échec

Un build ou test obligatoire en échec arrête la livraison. Rapporter la commande, la sortie utile redacted, la cause probable, la qualification et le correctif séparé requis. Ne pas revert automatiquement les changements hors périmètre.

## Git

Ne jamais commit ni push sans instruction explicite. Outputs de build, fichiers runtime, logs, sessions, HAR et credentials sont toujours exclus.
