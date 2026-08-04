# Interface QML et internationalisation

## En bref

QML porte l'affichage, la navigation et les interactions utilisateur. Les bridges et modèles Qt exposent les opérations et les états structurés. Les protocoles réseau, la persistance et les traitements lourds restent en C++.

## Vues actives

Le shell courant contient connexion/session cloud, fichiers cloud, imprimantes, état MQTT et paramètres. Les builds debug peuvent ajouter les pages de logs et de diagnostic. Lorsque le viewer expérimental est activé, chaque ligne PWSZ compatible expose une action **3D** qui télécharge le fichier vers un chemin local temporaire puis ouvre une modal dédiée ; aucun cinquième onglet principal n'est ajouté.

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

La préparation de l'impression distante est portée par `PrintWorkflowBridge` et `PrepareRemotePrintUseCase`. Le workflow crée l'identifiant de corrélation, demande la compatibilité par identifiant de fichier cloud, bascule vers l'extension lorsque nécessaire, rejette les réponses obsolètes, filtre les imprimantes compatibles et choisit l'imprimante compatible préférée. `CloudBridge` exécute uniquement les appels cloud asynchrones de compatibilité et renvoie leurs résultats au workflow ; QML reçoit un résultat sémantique unique et ne corrèle plus les callbacks réseau. Le scoring de compatibilité locale possède une seule implémentation C++ dans `PrinterFileCompatibility`, partagée par le use case de préparation et l'adaptateur de compatibilité de `CloudBridge` ; QML ne tokenize ni ne score plus les métadonnées machine.

Le routage MQTT, la corrélation des ordres et le store temps réel normalisé continuent lorsque les pages de diagnostic sont masquées. Les notifications réservées au diagnostic, le formatage du texte de télémétrie et les resets du modèle de flux brut ne sont activés que pendant l'affichage de la page MQTT ; les messages reçus en arrière-plan restent dans l'historique C++ borné puis sont publiés en une synchronisation lorsque la page est rouverte. La page des imprimantes diffère la projection du cache déclenchée par MQTT lorsqu'elle est masquée, effectue un rattrapage asynchrone unique à sa réouverture et arrête son rafraîchissement cloud périodique hors de l'onglet actif. La page de logs ne relit son snapshot JSONL que lorsqu'elle est active et visible.

Les listes de sélection des imprimantes sont portées par des modèles C++ au lieu de reconstruire des payloads `ListModel` en QML. Les imprimantes compatibles utilisent `PrintersModel` ; les fichiers cloud à imprimer et les fichiers locaux de l'imprimante utilisent `PrinterFilesModel`. Les identités stables sont mises à jour par `dataChanged`, les ajouts/suppressions de fin utilisent des deltas de lignes, et seul un réordonnancement d'identités déclenche un reset complet. Les métadonnées brutes restent accessibles par `get()` pour la préparation d'impression distante.

Le dialogue de confirmation d'impression distante reçoit directement la ligne complète déjà présente dans `CloudFilesModel` lorsque l'action **Imprimer** est déclenchée depuis la liste des fichiers. Le nom, la durée estimée et la consommation de résine sont ainsi disponibles dès l'ouverture, sans attendre une nouvelle synchronisation cloud. Pendant la préparation portée par `PrintWorkflowBridge`, le sélecteur peut afficher l'imprimante préférée issue du modèle principal ; lorsque le résultat sémantique arrive, QML remplace le modèle des imprimantes compatibles depuis ce résultat et resynchronise l'identifiant sélectionné. Le dialogue ne propose pas de changer de fichier : le fichier est fixé par l'action d'origine.

Lorsque ce point d'entrée est utilisé avant l'initialisation de la page Imprimantes, il amorce uniquement la liste des imprimantes nécessaire à la compatibilité et à la sélection. Il ne déclenche ni le rafraîchissement initial des détails imprimante ni celui des jobs récents. Les détails complets et l'historique restent chargés lorsque la page Imprimantes est explicitement activée, ce qui empêche la préparation d'impression de provoquer du trafic de miniatures de projets sans rapport.

La liste des tâches récentes dérive son badge du `printStatus` du projet. Une tâche n'est promue **En cours** que si le projet live correspondant porte explicitement le statut `1`, ou si l'identifiant MQTT correspondant est accompagné d'une étape de workflow active. Un projet terminal n'est jamais traité comme live au seul motif qu'il constitue la première ou la plus récente ligne de l'historique.

La page Imprimantes reprend la même hiérarchie visuelle que la page Fichiers : action de rafraîchissement compacte, synthèse dynamique du parc et onglets limités au nom de l’imprimante. L’en-tête de l’imprimante sélectionnée ne répète plus son nom et expose uniquement **Fichiers locaux**, puis l’état de l’imprimante. **Fichiers locaux** est désactivé lorsque l’imprimante sélectionnée ne peut pas accepter l’opération, notamment lorsqu’elle est hors ligne, et son ton désactivé reprend celui de l’état hors ligne. Le tooltip expose alors la raison du blocage. L’ancienne action **Détails** et son dialogue JSON dédié sont supprimés. Le badge d’état conserve son libellé localisé tout en dérivant sa couleur de l’état sémantique de l’imprimante : une imprimante en ligne et prête utilise ainsi la couleur de succès du thème. Les informations de l’appareil restent regroupées en haut de leur carte, sans répartir l’espace inutilisé entre les sections.

Les tâches récentes restent strictement textuelles : aucune miniature n’est résolue et l’affichage ne dépend pas de la présence persistante du fichier cloud d’origine. Le tableau utilise des colonnes stables et alignées pour le fichier, la date d’impression, la durée et le statut. Seule la date de début est affichée, sans heure ni date de fin. L’identifiant technique de tâche n’est instancié que lorsque `accloudBuildDebugEnabled` vaut vrai ; il est totalement absent de l’interface de production. L’ancienne case **UI debug**, les marqueurs de sections QML et le panneau JSON des endpoints sont supprimés de la page Imprimantes.

La barre d'actions distingue **Ajouter au cloud** et **Impression directe**. L'upload standard s'arrête après l'enregistrement cloud et ne propose aucune option de nettoyage après impression. L'impression directe conserve le fichier local sélectionné comme entrée de l'opération, contrôle la compatibilité par extension, le téléverse puis envoie automatiquement l'ordre lorsque le fichier cloud est prêt. Sa case de nettoyage appartient uniquement à cette opération : après une impression réussie confirmée, ACM supprime d'abord le fichier local exact de l'imprimante, puis le fichier cloud uniquement après confirmation MQTT de `deleteLocal`.

Le menu Paramètres propose **Supprimer la copie locale de l'imprimante si une impression directe échoue**, désactivé par défaut. Sa valeur est figée au lancement de l'opération directe et n'a aucun effet sur les uploads standards, les impressions depuis la liste cloud ou les impressions directes sans nettoyage demandé. Pour une tâche directe réellement passée à l'état actif puis terminée avec le statut 3 ou 4, l'activation autorise uniquement la suppression de la copie locale exacte ; le fichier cloud est toujours conservé en cas d'échec, d'arrêt ou d'annulation.

Le menu Paramètres expose aussi **Workers de génération 3D** et **Couleurs 3D** lorsque le viewer est disponible. La valeur persistée `render3d.workerCount` vaut `4` par défaut, est bornée à `1..16` et s'applique à chaque nouveau PWSZ chargé. Une valeur plus élevée augmente la pression CPU et mémoire ; sa modification ne change pas un mesh déjà en cours de génération.

La valeur persistée `render3d.palettePreset` sélectionne l'une des cinq palettes pièce/support/fond : `technical_cyan`, `industrial_amber`, `mineral_ivory`, `night_coral` ou `light_graphite`. Une valeur persistée invalide est normalisée vers `technical_cyan`. L'application d'un préréglage met à jour les trois couleurs ensemble, se propage à un viewer déjà ouvert et ne déclenche qu'un repaint via `meshColor`, `supportColor` et `backgroundColor` ; le PWSZ n'est pas rechargé et le mesh n'est pas reconstruit. La saisie libre de couleurs n'est pas exposée.

Dans la modal 3D, la plage de couches est présentée sous forme d'un double curseur vertical à droite du viewport, borné visuellement par la couche maximale et la couche `1`. Le survol d'une poignée affiche son numéro dans un tooltip. La molette sur ce contrôle ne modifie que la borne haute ; la borne basse reste déplaçable uniquement à la souris. Aucun champ numérique redondant n'est affiché. Une case compacte **Supports** dans l'angle inférieur gauche active ou désactive la couleur estimée des supports dans le shader. Elle ne masque aucune matière et ne reconstruit pas le mesh ; la géométrie ambiguë ou fusionnée conserve la couleur de la pièce.

La persistance des impressions directes, la réconciliation avec les projets et les transitions d'état de nettoyage sont désormais portées par `PrintWorkflowBridge`, adossé à `PendingDirectPrintStore` et au `DirectPrintLifecycleUseCase` typé. QML se limite à enregistrer une impression directe acceptée et à transmettre au workflow les snapshots de projets rafraîchis ; il ne conserve plus de map de nettoyage direct, ne corrèle plus les projets, ne déclenche plus lui-même la suppression locale imprimante, ne corrèle plus la suppression cloud et n'interprète plus les transitions de nettoyage. `CloudBridge` n'expose plus les méthodes de persistance des impressions directes. La corrélation asynchrone des ordres imprimante utilise un contexte `QVariantMap` structuré (`kind` et champs typés) au lieu d'identifiants concaténés analysés par préfixes ou `split(":")`. Les confirmations MQTT `deleteLocal` sont raccordées directement de `MqttBridge` vers `PrintWorkflowBridge` : le bridge porte la corrélation des `msgId` en attente ainsi que les états de suppression locale/cloud en vol. Les intentions de transport pour les suppressions directe locale/cloud sont raccordées de `PrintWorkflowBridge` vers `CloudBridge` dans le bootstrap desktop ; QML ne reçoit plus que des notifications sémantiques de nettoyage et de libération du suivi. Les impressions standards lancées depuis la liste cloud utilisent la même frontière de workflow pour le nettoyage post-print : QML signale seulement la fin de la tâche imprimante, `PrintWorkflowBridge` demande d'abord la suppression du fichier local de l'imprimante, ne demande la suppression cloud qu'après succès de cet ordre local, puis émet des notifications sémantiques de succès ou d'échec. La map QML restante d'impression distante en attente sert uniquement de placeholder visuel temporaire après acceptation de l'ordre, jusqu'à l'arrivée de la télémétrie projet réelle.


La complétion des aperçus PWSZ est contrôlée par deux réglages persistés : la complétion elle-même est activée par défaut, et la confirmation avant remplacement permanent du fichier local est activée par défaut. La modal explique que `preview_1.png` est copié vers `preview_2.png`, que la version préparée est envoyée, puis que le fichier local n’est remplacé qu’après succès cloud. « Ne plus demander » désactive uniquement la confirmation ; les deux réglages restent accessibles depuis le menu Paramètres.

## Sélection multiple des fichiers cloud

Chaque ligne de fichier cloud expose une case à cocher indépendante. Les identifiants et noms d’affichage sélectionnés sont conservés dans l’état de la page, séparément de la ligne unique utilisée par la vue de détails. Dès qu’au moins un fichier est sélectionné, une action destructive `Supprimer (N)` apparaît entre Rafraîchir et Ajouter au cloud.

L’action exige toujours une confirmation explicite. `CloudFilesPage.qml` transmet une seule fois les lignes sélectionnées à `CloudFilesWorkflowBridge` ; la page ne porte plus la file d’attente, l’identifiant courant, l’accumulateur d’échecs ni la corrélation des callbacks par fichier. Le bridge s’appuie sur le `DeleteCloudFilesUseCase` typé pour préserver l’ordre de sélection, ne demander qu’une suppression asynchrone à la fois, ignorer les réponses obsolètes et agréger les échecs. `CloudBridge` reste responsable de la suppression cloud/cache effective. QML ne reçoit que des signaux sémantiques de début, progression, succès et fin, retire de la sélection visuelle les éléments supprimés, rafraîchit la liste une seule fois puis traduit le résumé final en réussite complète, partielle ou échec.

## Destination de téléchargement d’un fichier cloud

L’action de téléchargement utilise le composant interne `DownloadFileDialog.qml` au lieu du sélecteur natif du bureau. Le dialogue suit ainsi la palette et les contrôles ACM actifs quel que soit l’environnement desktop. Il s’ouvre dans le dossier Téléchargements standard lorsqu’il existe. La carte de gauche fournit une arborescence de dossiers dépliable et chargée à la demande ; la vue **Contenu** regroupe les sous-dossiers du répertoire courant puis uniquement les fichiers portant l’extension du fichier cloud. Les raccourcis Dossier personnel et Téléchargements restent disponibles, sans bouton Parent ni rappel textuel redondant de la destination.

Le nom cloud complet, extension d’origine incluse, est prérempli avant la demande d’URL signée. Si l’utilisateur retire l’extension, le suffixe d’origine est restauré lors de la construction de la destination. Le champ éditable n’accepte qu’un nom de base : les séparateurs de chemin sont retirés avant l’envoi du chemin local final à `CloudBridge::startDownload()`. Un double-clic sur un dossier ouvre ce dossier ; un double-clic sur un fichier compatible reprend son nom et déclenche la confirmation de remplacement lorsque nécessaire.

## Ressources et séparation production

`resources.qrc` contient l'UI normale. `resources_debug.qrc` contient les pages debug. La production ne peut dépendre d'objets debug ou de vues de payload brut.

Aucune action de renommage cloud n'est exposée : aucun endpoint Anycubic observé de renommage ne fait partie du contrat runtime. Les dialogues normalisent également l'overlay absent vers `null` afin d'éviter les avertissements QObject pendant les tests ou la destruction de la fenêtre.

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
- charger à la demande les pages coûteuses ;
- mesurer avant de refondre.

Les détails restent dans l'annexe performance UI.

## Proposition de modification des PWSZ cloud

Lorsqu’un rafraîchissement complet des miniatures détecte des placeholders PWSZ invalides, le bridge émet une proposition unique contenant uniquement identifiants, noms d’affichage et tailles. La modal indique le nombre et le volume total des fichiers concernés et exige une confirmation explicite. La progression puis les totaux de fichiers modifiés, déjà conformes, en échec ou partiellement modifiés sont transmis par des signaux du bridge ; QML n’implémente ni les transferts ni la séquence de suppression. La modale de progression expose une action d’annulation qui se limite à positionner le token du bridge. Le workflow C++ reste responsable de l’arrêt des transferts actifs, de la conservation de l’original et du retour d’un résultat annulé ou partiel.

Les phases de progression traversent le bridge sous forme de clés stables (`pwsz.update.*`) et sont traduites uniquement dans QML. La boîte de résultat distingue le succès, l’annulation et la fin avec incidents. Pour chaque élément en échec, partiel ou annulé, elle affiche le nom du fichier, le statut, le détail backend, l’identifiant cloud original et l’identifiant de remplacement lorsqu’il existe. Un inventaire cloud incomplet est signalé comme avertissement et bloque la proposition groupée.

## Détails d’un fichier cloud

La modal de détails privilégie les informations utiles à l’impression : aperçu local, format, taille, date de téléversement, état traduit, machine, matériau, durée, profil de couches, exposition, consommation et imprimantes compatibles. La miniature et les huit valeurs principales sont regroupées dans un résumé compact de hauteur fixe afin de préserver en permanence la zone d’onglets. La miniature utilise uniquement la source déjà résolue par le cache du bridge ; QML n’effectue aucun téléchargement distant direct.

Le réglage persistant `ui.cloudFiles.showAdvancedDetails`, accessible depuis **Paramètres > Afficher les détails techniques des fichiers**, ajoute un onglet réservé aux identifiants du fichier, au code d’état, aux dates techniques, à la région et au MD5 de tranche. Il est désactivé par défaut en production et activé par défaut avec `--debug-ui`, sauf valeur utilisateur déjà persistée.

Un onglet distinct **Métadonnées cloud** est visible uniquement dans les builds de développement avec `ACCLOUD_DEBUG` actif. Il expose l’identité cloud brute, les dates, la région, le bucket et le chemin objet nécessaires au diagnostic. Les URL signées de téléchargement et les chemins locaux du cache de miniatures ne sont jamais affichés. Chaque onglet utilise deux cartes de largeur identique qui occupent toute la hauteur disponible. Les lignes de détails conservent leur hauteur naturelle, avec un espacement compact fixe, et restent regroupées en haut de chaque carte ; seul l’espace résiduel sous la dernière ligne s’étire. Le panneau ne défile que lorsque le contenu dépasse réellement. L’action Renommer se trouve dans l’en-tête ; Supprimer reste isolé à gauche du pied de page, tandis que les actions Fermer, Télécharger et Imprimer, de taille homogène, sont ordonnées à droite afin qu’Imprimer reste l’action principale finale.
