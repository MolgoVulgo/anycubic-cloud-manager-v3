# Anycubic Cloud Manager V3

Anycubic Cloud Manager V3 est une application desktop C++20 / Qt6 / QML destinée à exploiter l’écosystème cloud Anycubic depuis Linux. Le périmètre actif couvre la gestion des fichiers cloud, l’état des imprimantes, l’observation MQTT, le workflow d’impression distante et la base technique du viewer Photon/PWMB.

Ce dépôt n’est pas un projet officiel Anycubic. C’est un client reconstruit à partir de comportements observés, de traces MQTT, de tests runtime et de décisions documentées. Anycubic peut modifier ses endpoints, topics MQTT, signatures, payloads, comportements firmware ou validations cloud sans préavis.

## Périmètre produit actuel

| Domaine | État | Sens produit |
| --- | --- | --- |
| Shell Qt/QML | Actif | Fenêtre principale, fichiers, imprimantes, import session, réglages, état runtime et logs en debug. |
| Client Anycubic Cloud | Actif / partiel | Import HAR, listing cloud, quota, dashboard imprimantes, downloads signés, upload, print order et fallback cache. |
| Runtime MQTT | Actif | Connexion mTLS, routage topics, store realtime imprimante, overlay print, capture discovery des messages inconnus. |
| Impression distante | Actif / partiel | Commande HTTP puis suivi MQTT. Certaines actions dépendent du modèle, du firmware et de l’API. |
| Viewer Photon/PWMB | Expérimental / partiel | Drivers formats, morceaux de décodage, squelette render3d et UI viewer existent, mais le flux produit n’est pas fermé. |
| Documentation | Active | La documentation fait partie du contrat projet : comportement, analyses, décisions et limites connues. |

## Capacités principales

- Importer ou persister une session Anycubic web depuis une capture HAR.
- Lister les fichiers cloud et ressources imprimantes du compte.
- Résoudre une URL de téléchargement signée et télécharger un fichier localement.
- Uploader un fichier local via le workflow observé `lock / presign / register / unlock`.
- Envoyer des ordres d’impression lorsque l’API cloud et la compatibilité imprimante le permettent.
- Souscrire aux topics MQTT imprimante et normaliser l’état temps réel.
- Suivre les phases préparation, téléchargement, contrôles, préchauffe, impression, échec et fin.
- Interpréter les messages résine selon la phase d’impression, en distinguant l’autoload avant préchauffe et les tentatives de remplissage pendant print.
- Maintenir cache, logs, thumbnails et settings runtime dans un répertoire contrôlé.
- Exclure les outils debug des builds production sauf activation explicite.

## Structure dépôt

```text
accloud/                 point d’entrée CMake
src/accloud/app/         bootstrap, bridges Qt, modèles exposés UI
src/accloud/domain/      contrats domaine et types stables
src/accloud/infra/       cloud, MQTT, cache, logs, drivers Photon, jobs
src/accloud/render3d/    base OpenGL / Qt Quick
src/accloud/ui/qml/      shell QML, pages, dialogs, composants partagés
tests/                   tests C++ et QML
tools/                   scripts CI et maintenance
i18n/                    catalogues Qt
resources/mqtt/tls/      fallback local du matériel TLS MQTT
packaging/               packaging, dont Arch
docs/                    documentation anglaise
FR/docs/                 documentation française
```

## Démarrage rapide

Depuis la racine du dépôt :

```bash
./start.sh 1   # compilation debug puis exécution avec UI debug
./start.sh 2   # exécution du build debug existant
./start.sh 3   # compilation production puis exécution production
```

Build manuel depuis `accloud/` :

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

Presets utiles :

```bash
cmake --preset default     # Debug, Qt actif, debug tooling désactivé
cmake --preset dev-debug   # Debug, Qt actif, debug tooling activé
cmake --preset prod        # Release, debug tooling exclu
```

## CLI

```bash
cd accloud
./build/default/accloud_cli --smoke
./build/default/accloud_cli --import-har /chemin/capture.har
./build/default/accloud_cli --import-har /chemin/capture.har --session-path /chemin/session.json
```

Les HAR contiennent des tokens réutilisables. Ils doivent être traités comme des secrets : pas de commit, pas d’issue publique, pas d’archive documentaire publique.

## Données runtime

Racine par défaut :

```text
~/.local/share/accloud
```

Fichiers et dossiers générés :

```text
~/.local/share/accloud/accloud.ini
~/.local/share/accloud/session.json
~/.local/share/accloud/settings.ini
~/.local/share/accloud/accloud_cache.db
~/.local/share/accloud/tmp/
~/.local/share/accloud/thumbnails/
~/.local/share/accloud/logs/
```

Overrides utiles :

```bash
export ACCLOUD_PATHS_INI=/chemin/accloud.ini
export ACCLOUD_SESSION_PATH=/chemin/session.json
export ACCLOUD_DB_PATH=/chemin/accloud_cache.db
export ACCLOUD_LOG_DIR=/chemin/logs
export ACCLOUD_THUMBNAIL_DIR=/chemin/thumbnails
```

## Documentation

Point d’entrée anglais :

```text
docs/README.md
```

Point d’entrée français :

```text
FR/docs/README.md
```

La documentation est organisée par domaine opérationnel : architecture, cloud, MQTT, workflow d’impression, UI/QML, performance UI, formats Photon/PWMB, i18n, opérations/sécurité, règles de développement, décisions et annexes.

## Règles de sécurité

Ne jamais logger ni committer :

- tokens cloud ;
- headers `Authorization` ;
- cookies ;
- URLs signées complètes ;
- captures HAR ;
- clés privées TLS MQTT ;
- credentials bruts ;
- fichiers session contenant des tokens réutilisables.

## Note de développement et remerciements

L’application est développée dans un workflow de vibe coding avec Codex, le code du dépôt et le comportement observé restant les sources de vérité finales.

Merci aux projets et mainteneurs suivants. Leurs travaux publiés, expérimentations et analyses de protocole ont rendu ce projet possible :

- [Royrdan/anycubic_cloud](https://github.com/Royrdan/anycubic_cloud), pour le travail initial et pratique autour du comportement Anycubic Cloud.
- [UVtools](https://github.com/sn4k3/UVtools), pour le travail important sur les formats de fichiers Anycubic et l’analyse des fichiers Photon.
- [WaresWichall/hass-anycubic_cloud](https://github.com/WaresWichall/hass-anycubic_cloud), pour le travail de compréhension du comportement MQTT Anycubic et l’intégration Home Assistant.

## Licence

Voir les fichiers de licence du dépôt. Le packaging actuel déclare le projet sous licence MIT.
