# Interface QML et internationalisation

## En bref

QML porte l'affichage, la navigation et les interactions utilisateur. Les bridges et modèles Qt exposent les opérations et les états structurés. Les protocoles réseau, la persistance et les traitements lourds restent en C++.

## Vues actives

Le shell courant contient connexion/session cloud, fichiers cloud, imprimantes, état MQTT, paramètres et viewer expérimental. Les builds debug peuvent ajouter les pages de logs et de diagnostic.

Le fichier principal chargé par le runtime desktop est :

```text
qrc:/qml/MainWindow.qml
```

## Répartition des responsabilités

```text
QML         affichage, état visuel, navigation, saisie
bridges Qt  opérations et signaux exposés à l'UI
use cases   coordination métier
infra       HTTP, MQTT, stockage, cache, formats et logs
modèles     données structurées consommées par QML
```

QML ne doit pas lancer d'appels HTTP, parser de gros payloads, ouvrir des transactions SQLite, générer des credentials MQTT ou définir les retries réseau. Les composants image consomment uniquement des sources locales `file://`, `qrc:/` ou inline `data:image` préparées par le bridge ; les URLs distantes de miniature ne sont jamais retentées directement par QML.

## Opérations longues

Uploads, téléchargements, synchronisation cloud, cache et décodage de formats ne doivent pas bloquer le thread GUI. Busy state, progression, annulation et erreurs passent par les propriétés et signaux des bridges.

La complétion des aperçus PWSZ est contrôlée par deux réglages persistés : la complétion elle-même est activée par défaut, et la confirmation avant remplacement permanent du fichier local est activée par défaut. La modal explique que `preview_1.png` est copié vers `preview_2.png`, que la version préparée est envoyée, puis que le fichier local n’est remplacé qu’après succès cloud. « Ne plus demander » désactive uniquement la confirmation ; les deux réglages restent accessibles depuis le menu Paramètres.

## Ressources et séparation production

`resources.qrc` contient l'UI normale. `resources_debug.qrc` contient les pages debug. La production ne peut dépendre d'objets debug ou de vues de payload brut.

## Internationalisation

Catalogues actifs :

```text
i18n/accloud_en.ts
i18n/accloud_fr.ts
```

Ce sont les seuls catalogues TS actifs. Des copies sous `accloud/i18n/` sont invalides car CMake ne les charge pas.

Les textes utilisateur utilisent le mécanisme Qt existant. La source et les deux catalogues sont vérifiés ensemble. Les textes debug restent exclus de la production lorsque les outils debug sont désactivés.

## Principes de performance

- éviter le téléchargement eager des miniatures au démarrage ;
- ne pas reconstruire un gros modèle pour un changement local ;
- limiter le travail logs/MQTT sur le thread GUI ;
- charger à la demande les pages coûteuses et le viewer ;
- mesurer avant de refondre.

Les détails restent dans l'annexe performance UI.

## Proposition de modification des PWSZ cloud

Lorsqu’un rafraîchissement complet des miniatures détecte des placeholders PWSZ invalides, le bridge émet une proposition unique contenant uniquement identifiants, noms d’affichage et tailles. La modal indique le nombre et le volume total des fichiers concernés et exige une confirmation explicite. La progression puis les totaux de fichiers modifiés, déjà conformes, en échec ou partiellement modifiés sont transmis par des signaux du bridge ; QML n’implémente ni les transferts ni la séquence de suppression.
