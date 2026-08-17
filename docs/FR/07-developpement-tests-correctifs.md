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

## Gates de validation séparés

Le projet utilise deux gates de validation explicites afin qu'un environnement restreint ne prétende jamais valider la pile desktop Qt.

Gate core restreint et hors ligne :

```bash
# Placer accloud-build-deps.zip à la racine du dépôt, ou définir
# ACCLOUD_DEPENDENCY_ARCHIVE avec son chemin absolu.
cmake --preset protected-core
cmake --build --preset protected-core --clean-first
ctest --preset protected-core --output-on-failure
```

Ce gate désactive Qt, QML et les tests de services externes. Il valide le core portable, la logique cloud/MQTT sans dépendance Qt, les régressions de sécurité et les gardes Python statiques. L'archive de dépendances est extraite uniquement sous le répertoire de build CMake et aucun accès réseau n'est tenté.

Gate Qt local complet :

```bash
cmake --preset local-full
cmake --build --preset local-full --clean-first
ctest --preset local-full --output-on-failure
```

Le preset `local-full` constitue le gate Qt local complet. La configuration échoue si les composants Qt desktop, MQTT ou QuickTest requis sont absents, et il conserve les tests QML, SQL, GUI et d'intégration. Le test broker MQTT live reste soumis à l'activation explicite `ACCLOUD_MQTT_LIVE_TEST=1`.

Les tests exposent des labels CTest comme `core`, `static`, `qt`, `qml`, `sql`, `integration` et `live`, utilisables localement avec `ctest --preset local-full -L <label>`.

Exemples ciblés :

```bash
ctest --preset default -R '^accloud_har_import$' --output-on-failure
ctest --preset default -R '^accloud_security_redaction$' --output-on-failure
ctest --preset default -R '^accloud_mqtt_flow$' --output-on-failure
ctest --preset default -R '^accloud_ui_qml' --output-on-failure
```


Inventaire des tests CTest enregistrés par `accloud/CMakeLists.txt` :

- `accloud_cloud_api_architecture`
- `accloud_cloud_api_support`
- `accloud_cloud_bridge_architecture`
- `accloud_cloud_core_regressions`
- `accloud_cloud_files_delete`
- `accloud_dev_raw_traffic_log`
- `accloud_direct_print_lifecycle`
- `accloud_documentation_contract`
- `accloud_experimental_viewer_architecture`
- `accloud_experimental_viewer_scaffold`
- `accloud_har_import`
- `accloud_jsonl_logger_timestamp`
- `accloud_layer_stack_mesher`
- `accloud_local_cache_architecture`
- `accloud_log_flow`
- `accloud_mqtt_bridge_architecture`
- `accloud_mqtt_flow`
- `accloud_mqtt_live_broker`
- `accloud_pw0_decode`
- `accloud_pwsz_cloud_preview_update_order`
- `accloud_pwsz_preview_archive`
- `accloud_pwsz_reader`
- `accloud_render3d_shader_compile`
- `accloud_render3d_worker_benchmark_selftest`
- `accloud_render_pipeline`
- `accloud_security_redaction`
- `accloud_smoke`
- `accloud_support_analysis_diagnostics`
- `accloud_support_analyzer`
- `accloud_thumbnail_cache_policy`
- `accloud_thumbnail_candidates`
- `accloud_thumbnail_validation`
- `accloud_ui_migration_check`
- `accloud_ui_models`
- `accloud_ui_qml`
- `accloud_ui_qml_upload`
- `accloud_viewer_controls`

La liste ci-dessus correspond à l'ensemble complet des noms de tests enregistrés dans l'arbre source courant. Leur disponibilité dans un preset donné peut toujours dépendre des options de build et des dépendances natives ; les gates propres à chaque preset restent autoritaires pour ce qui est exécuté.

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

Garde de frontière du bridge cloud :

```bash
python ../tools/check_cloud_bridge_architecture.py --repo-root ..
ctest --preset default -R '^accloud_cloud_bridge_architecture$' --output-on-failure
```

Cette garde maintient `CloudBridge` comme une façade bornée et empêche le traitement TLS/image des miniatures, le transport par URL signée ou l’orchestration upload/PWSZ de revenir dans le bridge.

Garde de frontière du cache local :

```bash
python ../tools/check_local_cache_architecture.py --repo-root ..
ctest --preset default -R '^accloud_local_cache_architecture$' --output-on-failure
```

Cette garde maintient `LocalCacheStore` comme une petite façade de compatibilité, impose des unités séparées pour schéma/fichiers/imprimantes/jobs/état et vérifie que le runtime et les tests de régression SQL compilent le même ensemble `ACCLOUD_LOCAL_CACHE_SOURCES`.


Garde de frontière du bridge MQTT :

```bash
python ../tools/check_mqtt_bridge_architecture.py --repo-root ..
ctest --preset default -R '^accloud_mqtt_bridge_architecture$' --output-on-failure
```

Cette garde maintient `MqttBridge` comme une façade Qt bornée, impose les unités session/messages/télémétrie, vérifie l'ensemble partagé `ACCLOUD_MQTT_BRIDGE_SOURCES` et préserve la propriété de la configuration broker/SLICER figée dans l'unité session.

Isolation du viewer expérimental :

```bash
cmake --preset experimental-viewer-core
cmake --build --preset experimental-viewer-core --clean-first
ctest --preset experimental-viewer-core \
  -R '^(accloud_experimental_viewer_architecture|accloud_experimental_viewer_scaffold|accloud_pw0_decode|accloud_pwsz_reader|accloud_layer_stack_mesher|accloud_support_analyzer|accloud_support_analysis_diagnostics|accloud_render_pipeline|accloud_viewer_controls|accloud_render3d_worker_benchmark_selftest)$' \
  --output-on-failure
```

Le preset `default` active le viewer ; `dev-debug` et `local-full` héritent de cette valeur. `prod` et `protected-core` conservent explicitement `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER=OFF`. Le preset `experimental-viewer-core` valide sans Qt le lecteur PWSZ, le décodage, le meshing, la file d'upload bornée, le plan de rendu par plage, les contrôles de caméra et la supersession transactionnelle des demandes rapides de surfaces de coupe. La garde d'architecture vérifie que les sources Qt/OpenGL restent derrière l'option de build, que l'action PWSZ par fichier est visible et que la production reste désactivée.
Validation locale Qt/OpenGL obligatoire pour toute modification du viewer desktop :

```bash
cmake --preset experimental-viewer-qt
cmake --build --preset experimental-viewer-qt --clean-first
ctest --preset experimental-viewer-qt --output-on-failure
```

Ce preset hérite de `local-full`, exige les dépendances Qt natives, conserve `Qt6::OpenGL`, lie le viewer à `accloud_cli` et exécute également les tests QML. `accloud-build-deps.zip` ne doit pas être imposé sur le poste local lorsque `nlohmann_json` est déjà installé.
L’inventaire Qt/OpenGL du viewer inclut également `accloud_render3d_shader_compile` ; ce test exige la création d’un contexte OpenGL 3.3 Core avant la compilation des shaders. Un échec de création de ce contexte est un échec d’environnement et ne constitue pas une validation réussie des shaders.

L'ordonnancement des workers de l'analyse des supports peut être comparé sur exactement le même PWSZ sans modifier le résultat sémantique :

```bash
/usr/bin/time ./build/experimental-viewer-core/accloud_support_analysis_probe \
  /chemin/entree.pwsz --output /tmp/support-w1.json --workers 1
/usr/bin/time ./build/experimental-viewer-core/accloud_support_analysis_probe \
  /chemin/entree.pwsz --output /tmp/support-w4.json --workers 4
```

Après exclusion du champ `analysis_workers`, les deux sorties JSON doivent être sémantiquement identiques. L'analyseur décode/décrit chaque couche native exactement une fois, conserve les descriptions compactes pour la passe sémantique descendante, réutilise les métriques d'appariement parent de chaque composant pendant la couche courante et utilise un unique pool persistant à priorité pour la préparation des couches en faible priorité ainsi que les lots prioritaires de classification montante, recherche de lignée pièce et réconciliation descendante. Les éléments prioritaires sont réclamés dynamiquement par les workers configurés tandis que le coordinateur valide les résultats dans un ordre déterministe. La fenêtre de masques natifs préparés et retenus reste limitée à quatre même lorsqu'un nombre supérieur de workers sémantiques est sélectionné. Les structures sensibles aux allocations sont également réutilisées : l'extraction des composants connexes conserve ses buffers de lignes entre les scanlines, associe les racines du disjoint-set via un index dense au lieu d'une map ordonnée, réserve exactement le stockage des runs de chaque composant et `NodeState` référence le composant immuable déjà conservé au lieu de recopier son vecteur de runs natifs. `SparseRunMask` fusionne ses lignes sur place et ne parcourt pour les intersections que les lignes touchées par l'opérande le plus clairsemé ; les vecteurs de préparation montante/descendante conservent leur capacité entre les couches. Le test `accloud_support_analyzer` vérifie un seul `loadMask()` source par couche native pour les sources concurrentes comme sérialisées, contrôle qu'un run à 16 workers n'élargit jamais la préparation concurrente au-delà de quatre masques, compare le graphe/résultat sémantique à 1, 4 et plusieurs workers, contrôle la stabilité déterministe des identifiants locaux sur un cas dense de composants déconnectés et compare le chemin hybride bitset/SIMD de P5 au chemin canonique uniquement basé sur les runs sur des lignes fortement fragmentées. Le probe de diagnostic accepte `--no-bitsets` pour désactiver uniquement le cache hybride de bitsets de lignes lors des mesures A/B ; le dispatch AVX2 des scanlines conserve toujours son fallback scalaire obligatoire et ne modifie pas le contrat sémantique. Cette mesure est distincte du benchmark des workers de meshing ci-dessous.

Benchmark manuel des workers sur un même PWSZ :

```bash
./build/experimental-viewer-core/accloud_render3d_worker_benchmark \
  --input /chemin/Beetle-2.pwsz \
  --workers 4,8,16 \
  --repeats 1 \
  --layer-stride 2 \
  --chunk-layers 8,16,32 \
  --output-prefix /tmp/beetle-workers
```

Le benchmark ouvre une seule fois le même fichier et exécute le produit cartésien complet des tailles de chunks et nombres de workers demandés. Pour une taille de chunk donnée, tous les runs workers doivent produire la même signature compacte : chunks, rectangles de surface, triangles, octets compacts et octets équivalents de l'ancien maillage. Les nombres de chunks ne sont pas comparés entre tailles différentes. Il mesure le décodage PWSZ et le maillage CPU ; l'upload GPU et le rendu sont volontairement exclus. Les rapports `/tmp/beetle-workers.csv` et `/tmp/beetle-workers.jsonl` indiquent notamment `surface_quads`, `compact_bytes`, `legacy_equivalent_bytes`, `compression_ratio`, la taille de chunk, la durée totale et la latence du premier chunk. Le ratio attendu du chemin principal est exactement `15.0` : 8 octets compacts remplacent 120 octets de vertices/index historiques par rectangle. `--repeats 2` ou `3` améliore la stabilité statistique, inverse l’ordre de la matrice complète lors des répétitions paires et multiplie directement la durée du test. Le PWSZ reste externe au dépôt et ce benchmark réel n'est pas un test CTest bloquant. Le test CTest `accloud_render3d_worker_benchmark_selftest` valide la matrice, la stabilité géométrique et le ratio compact sur une source synthétique courte.

Après une modification du chemin GPU compact, la validation locale Qt doit être complétée par un essai runtime sur `Beetle-2.pwsz` avec le stride fixe actif du viewer (`layer_step = 2`). La génération doit atteindre 100 %, l'application doit rester active et manipulable, et `render3d.jsonl` doit contenir des événements `gpu.compact_chunk_uploaded` avec `compression_ratio = 15`, sans `gpu.budget_exceeded`, `gpu.compact_upload_failed` ni arrêt `SIGABRT`. Le champ `resident_bytes` doit rester inférieur ou égal à `budget_bytes`. Lorsque l'analyse des supports est activée, les couches de transition supplémentaires sélectionnées par l'analyse sémantique restent intégrées au workflow normal en stride 2. Cette validation runtime réelle complète les tests synthétiques ; elle ne doit pas être remplacée par le seul benchmark CPU.


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

Les règles complètes de production et de livraison sont fournies séparément par la session GPT Web dans `regles-generales-production.md`. Ce fichier normatif n’est volontairement ni stocké, ni copié, ni recréé dans le dépôt et ne doit jamais entrer dans `acm.zip` ou une archive patch.

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
