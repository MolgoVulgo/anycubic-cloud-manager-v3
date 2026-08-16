# Anycubic Cloud Manager V3

Anycubic Cloud Manager V3 est un client desktop Linux développé en C++20, Qt6 et QML. Il accède aux services cloud Anycubic observés, importe une session web réutilisable depuis une capture HAR, gère les fichiers cloud, affiche l'état des imprimantes et suit les impressions résine via MQTT.

Ce projet n'est pas une application officielle Anycubic. Les endpoints, signatures, topics MQTT et comportements imprimante sont reconstruits à partir d'observations et peuvent changer sans préavis.

## Fonctions disponibles

- import HAR et persistance locale de session ;
- liste, quota, téléchargement signé, upload et suppression des fichiers cloud ;
- tableau de bord imprimantes, compatibilité et commandes distantes ;
- connexion MQTT mTLS et store temps réel ;
- cache local, miniatures, logs structurés redacted et interface bilingue ;
- parsing Photon/PWMB partiel et viewer 3D PWSZ de développement fonctionnel mais expérimental, avec aperçu fixe une couche sur deux et coloration support/pièce optionnelle issue d’une analyse en deux passes ; l’intégration production reste désactivée.

## Compiler et lancer

```bash
./start.sh 1       # compile et lance le mode développement
./start.sh 2       # lance le build développement existant
./start.sh 3       # compile et lance le mode production
```

Build manuel depuis `accloud/` :

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

Les captures HAR et les fichiers `session.json` générés contiennent des credentials réutilisables. Ils ne doivent jamais être commités ni partagés.

## Documentation

Commencer par [l'index documentaire français](docs/FR/README.md). Le guide principal est limité à sept documents progressifs :

1. présentation et démarrage ;
2. architecture et runtime actif ;
3. workflows cloud Anycubic ;
4. MQTT et état temps réel ;
5. interface QML et internationalisation ;
6. sécurité, logs, cache et données ;
7. développement, tests et livraison des correctifs.
