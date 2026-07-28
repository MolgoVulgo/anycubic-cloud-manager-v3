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

Le logger de trafic brut réservé au mode développement possède son test de régression :

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug -R '^accloud_dev_raw_traffic_log$' --output-on-failure
```

Le test vérifie la création de `log_brut.txt`, la capture HTTP/MQTT et la redaction obligatoire des credentials et URLs signées.

Garde documentation et archive :

```bash
python ../tools/check_documentation_contract.py --repo-root ..
ctest --preset default -R '^accloud_documentation_contract$' --output-on-failure
```

La garde valide les paires bilingues, les liens locaux, les constantes MQTT/SSL figées, l’unique portée miniature de `ignoreSslErrors()`, les ressources QML actives, l’unicité des catalogues TS et le caractère exclusivement synthétique des données publiques de référence.

Ne pas inventer de commande absente de CMake. Les tests broker live exigent un environnement contrôlé et ne sont jamais couverts implicitement par un test unitaire local. L'exécution CTest par défaut classe `accloud_mqtt_live_broker` en **Skipped** tant que l'exécution live n'est pas explicitement activée.

Lancer le contrôle live uniquement avec une session locale valide et des chemins mTLS explicites :

```bash
ACCLOUD_MQTT_LIVE_TEST=1 \
ACCLOUD_MQTT_TLS_CA_PATH=/chemin/controle/ca.crt \
ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH=/chemin/controle/client.crt \
ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH=/chemin/controle/client.key \
ctest --preset default -R '^accloud_mqtt_live_broker$' --output-on-failure
```

Après activation explicite, une session ou un matériel TLS absent/invalide et un échec broker restent des échecs bloquants ; ils ne sont pas convertis en skip.

## Avant modification

1. Lire `AGENTS.md` et le document principal de la zone.
2. Vérifier la source compilée et le point d'entrée runtime.
3. Identifier le module propriétaire.
4. Distinguer actif, expérimental, legacy, test et référence.
5. Préserver les contrats cloud, MQTT, sécurité et thread GUI.
6. Limiter le changement.

## Produire un patch

Les règles complètes de production et de livraison sont fournies par la session GPT Web. Elles ne sont volontairement pas stockées dans ce dépôt. Ne pas créer, copier ni rechercher `regles-generales-production-correctifs.md` localement. Une copie de travail web-only sous `patch/` est ignorée par Git/l’archive locale et n’entre pas dans la garde documentaire locale.

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
