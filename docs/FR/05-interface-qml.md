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

Les contrôles de compatibilité d'impression distante par identifiant de fichier cloud et par extension s'exécutent dans des tâches de fond de `CloudBridge`. Chaque requête porte un identifiant de corrélation ; QML ne consomme que le signal de fin correspondant et ignore les réponses obsolètes. Les méthodes synchrones restent disponibles pour les mocks de test simples et les adaptateurs historiques, mais le bridge QObject de production utilise le chemin asynchrone.

Le routage MQTT, la corrélation des ordres et le store temps réel normalisé continuent lorsque les pages de diagnostic sont masquées. Les notifications réservées au diagnostic, le formatage du texte de télémétrie et les resets du modèle de flux brut ne sont activés que pendant l'affichage de la page MQTT ; les messages reçus en arrière-plan restent dans l'historique C++ borné puis sont publiés en une synchronisation lorsque la page est rouverte. La page des imprimantes diffère la projection du cache déclenchée par MQTT lorsqu'elle est masquée, effectue un rattrapage asynchrone unique à sa réouverture et arrête son rafraîchissement cloud périodique hors de l'onglet actif. La page de logs ne relit son snapshot JSONL que lorsqu'elle est active et visible.

Les listes de sélection des imprimantes sont portées par des modèles C++ au lieu de reconstruire des payloads `ListModel` en QML. Les imprimantes compatibles utilisent `PrintersModel` ; les fichiers cloud à imprimer et les fichiers locaux de l'imprimante utilisent `PrinterFilesModel`. Les identités stables sont mises à jour par `dataChanged`, les ajouts/suppressions de fin utilisent des deltas de lignes, et seul un réordonnancement d'identités déclenche un reset complet. Les métadonnées brutes restent accessibles par `get()` pour la préparation d'impression distante.

Le dialogue de confirmation d'impression distante reçoit directement la ligne complète déjà présente dans `CloudFilesModel` lorsque l'action **Imprimer** est déclenchée depuis la liste des fichiers. Le nom, la durée estimée et la consommation de résine sont ainsi disponibles dès l'ouverture, sans attendre une nouvelle synchronisation cloud. Pendant le contrôle asynchrone de compatibilité, le sélecteur affiche l'imprimante préférée issue du modèle principal ; il bascule ensuite vers le modèle filtré des imprimantes compatibles tout en resynchronisant l'identifiant sélectionné. Le dialogue ne propose pas de changer de fichier : le fichier est fixé par l'action d'origine.

La complétion des aperçus PWSZ est contrôlée par deux réglages persistés : la complétion elle-même est activée par défaut, et la confirmation avant remplacement permanent du fichier local est activée par défaut. La modal explique que `preview_1.png` est copié vers `preview_2.png`, que la version préparée est envoyée, puis que le fichier local n’est remplacé qu’après succès cloud. « Ne plus demander » désactive uniquement la confirmation ; les deux réglages restent accessibles depuis le menu Paramètres.

## Sélection multiple des fichiers cloud

Chaque ligne de fichier cloud expose une case à cocher indépendante. Les identifiants et noms d’affichage sélectionnés sont conservés dans l’état de la page, séparément de la ligne unique utilisée par la vue de détails. Dès qu’au moins un fichier est sélectionné, une action destructive `Supprimer (N)` apparaît entre Rafraîchir et Envoyer.

L’action exige toujours une confirmation explicite. Les suppressions sont ensuite soumises séquentiellement via l’opération asynchrone existante du bridge afin de ne pas bloquer le thread GUI et de préserver le contrat courant de suppression cloud/cache. Les éléments supprimés avec succès sortent de la sélection ; les éléments en échec restent sélectionnés. La liste est rafraîchie une seule fois à la fin et la barre d’état distingue réussite complète, réussite partielle et échec.

## Destination de téléchargement d’un fichier cloud

L’action de téléchargement utilise le composant interne `DownloadFileDialog.qml` au lieu du sélecteur natif du bureau. Le dialogue suit ainsi la palette et les contrôles ACM actifs quel que soit l’environnement desktop. Il s’ouvre dans le dossier Téléchargements standard lorsqu’il existe. La carte de gauche fournit une arborescence de dossiers dépliable et chargée à la demande ; la vue **Contenu** regroupe les sous-dossiers du répertoire courant puis uniquement les fichiers portant l’extension du fichier cloud. Les raccourcis Dossier personnel et Téléchargements restent disponibles, sans bouton Parent ni rappel textuel redondant de la destination.

Le nom cloud complet, extension d’origine incluse, est prérempli avant la demande d’URL signée. Si l’utilisateur retire l’extension, le suffixe d’origine est restauré lors de la construction de la destination. Le champ éditable n’accepte qu’un nom de base : les séparateurs de chemin sont retirés avant l’envoi du chemin local final à `CloudBridge::startDownload()`. Un double-clic sur un dossier ouvre ce dossier ; un double-clic sur un fichier compatible reprend son nom et déclenche la confirmation de remplacement lorsque nécessaire.

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

Lorsqu’un rafraîchissement complet des miniatures détecte des placeholders PWSZ invalides, le bridge émet une proposition unique contenant uniquement identifiants, noms d’affichage et tailles. La modal indique le nombre et le volume total des fichiers concernés et exige une confirmation explicite. La progression puis les totaux de fichiers modifiés, déjà conformes, en échec ou partiellement modifiés sont transmis par des signaux du bridge ; QML n’implémente ni les transferts ni la séquence de suppression. La modale de progression expose une action d’annulation qui se limite à positionner le token du bridge. Le workflow C++ reste responsable de l’arrêt des transferts actifs, de la conservation de l’original et du retour d’un résultat annulé ou partiel.

Les phases de progression traversent le bridge sous forme de clés stables (`pwsz.update.*`) et sont traduites uniquement dans QML. La boîte de résultat distingue le succès, l’annulation et la fin avec incidents. Pour chaque élément en échec, partiel ou annulé, elle affiche le nom du fichier, le statut, le détail backend, l’identifiant cloud original et l’identifiant de remplacement lorsqu’il existe. Un inventaire cloud incomplet est signalé comme avertissement et bloque la proposition groupée.

## Détails d’un fichier cloud

La modal de détails privilégie les informations utiles à l’impression : aperçu local, format, taille, date de téléversement, état traduit, machine, matériau, durée, profil de couches, exposition, consommation et imprimantes compatibles. La miniature et les huit valeurs principales sont regroupées dans un résumé compact de hauteur fixe afin de préserver en permanence la zone d’onglets. La miniature utilise uniquement la source déjà résolue par le cache du bridge ; QML n’effectue aucun téléchargement distant direct.

Le réglage persistant `ui.cloudFiles.showAdvancedDetails`, accessible depuis **Paramètres > Afficher les détails techniques des fichiers**, ajoute un onglet réservé aux identifiants du fichier, au code d’état, aux dates techniques, à la région et au MD5 de tranche. Il est désactivé par défaut en production et activé par défaut avec `--debug-ui`, sauf valeur utilisateur déjà persistée.

Un onglet distinct **Métadonnées cloud** est visible uniquement dans les builds de développement avec `ACCLOUD_DEBUG` actif. Il expose l’identité cloud brute, les dates, la région, le bucket et le chemin objet nécessaires au diagnostic. Les URL signées de téléchargement et les chemins locaux du cache de miniatures ne sont jamais affichés. Chaque onglet utilise deux cartes de largeur identique qui occupent toute la hauteur disponible. Les lignes de détails conservent leur hauteur naturelle, avec un espacement compact fixe, et restent regroupées en haut de chaque carte ; seul l’espace résiduel sous la dernière ligne s’étire. Le panneau ne défile que lorsque le contenu dépasse réellement. L’action Renommer se trouve dans l’en-tête ; Supprimer reste isolé à gauche du pied de page, tandis que les actions Fermer, Télécharger et Imprimer, de taille homogène, sont ordonnées à droite afin qu’Imprimer reste l’action principale finale.
