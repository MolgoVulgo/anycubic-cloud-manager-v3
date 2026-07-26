# Documentation française

La documentation suit une progression unique : objectif visible, fonctionnement général, runtime actif, contraintes techniques puis diagnostic.

## Guide principal

1. [Présentation et démarrage](01-presentation-demarrage.md)
2. [Architecture et runtime actif](02-architecture-runtime.md)
3. [Cloud Anycubic](03-cloud-anycubic.md)
4. [MQTT et état temps réel](04-mqtt-temps-reel.md)
5. [Interface QML et internationalisation](05-interface-qml.md)
6. [Sécurité, logs, cache et données](06-securite-donnees.md)
7. [Développement, tests et correctifs](07-developpement-tests-correctifs.md)

## Annexes techniques

- [Performance UI](annexes/performance-ui.md)
- [Formats Photon/PWMB et viewer](annexes/viewer-photon-formats.md)
- [Extensions de fichiers Anycubic](annexes/extensions-fichiers-anycubic.md)
- [Écrans du client cloud](annexes/ecrans-client-cloud.md)
- [Structures JSON MQTT](annexes/structures-json-mqtt.md)
- [Matrice des endpoints cloud actifs](annexes/endpoints-cloud-runtime.md)
- [Topics MQTT actifs](annexes/topics-mqtt.md)
- [Variables d'environnement runtime](annexes/variables-environnement.md)
- [Analyse de captures MQTT d'impression](annexes/analyse-captures-print-mqtt.md)
- [Politique d'archive](annexes/politique-archive.md)
- [Décisions techniques](annexes/decisions-techniques.md)

## Données publiques synthétiques

- [Politique et fixtures de référence](reference-data/README.md)
- [Workflow MQTT synthétique](../reference-data/mqtt/mqtt_synthetic_workflow.md)

## Statuts

- **ACTIF** : utilisé par le runtime courant ;
- **PARTIEL** : utilisable mais incomplet ou dépendant du cloud/imprimante ;
- **EXPÉRIMENTAL** : workflow non finalisé ;
- **RÉFÉRENCE** : matière explicative, pas contrat exécutable ;
- **HISTORIQUE** : snapshot d'investigation conservé pour traçabilité.

Le code compilé par CMake reste la source de vérité runtime. Les fichiers sous `sources-techniques/` ne peuvent pas le remplacer.
