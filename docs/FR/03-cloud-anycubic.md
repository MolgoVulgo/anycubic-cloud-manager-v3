# Cloud Anycubic

## En bref

Le sous-système cloud transforme une session Anycubic importée en requêtes Workbench signées. Il liste fichiers et imprimantes, récupère des URLs de téléchargement signées, charge des fichiers et initie les commandes imprimante.

QML ne construit jamais les requêtes cloud. Il appelle un bridge qui délègue aux use cases puis aux APIs d'infrastructure.

## Session et import HAR

Une capture HAR est sensible. L'importeur extrait les tokens réutilisables et les champs d'identité requis, les normalise et écrit le fichier de session configuré. Tout champ obligatoire manquant produit un échec explicite.

L'implémentation C++ active est la source de vérité runtime. Les captures et anciennes implémentations ne servent qu'à l'analyse.

## Requêtes Workbench

Les endpoints Workbench authentifiés utilisent les headers `XX-*`, le bearer et la signature observés. Ces règles ne doivent pas être remplacées par des conventions REST génériques.

Interprétation :

```text
HTTP 2xx sans code bloquant, ou code == 1  -> succès
HTTP 2xx avec code != 1                    -> erreur métier/API
HTTP 401 ou 403                            -> session expirée ou non autorisée
HTTP 429 ou 5xx                            -> retry borné possible
JSON invalide                              -> erreur API, jamais succès silencieux
```

## Workflows principaux

### Synchronisation initiale

```text
contexte de session
-> requêtes HTTP fichiers / quota / imprimantes
-> modèles UI normalisés
-> mise à jour du cache
-> overlay MQTT appliqué ensuite
```

### Téléchargement

```text
endpoint cloud retourne une URL signée
-> GET direct sur cette URL
-> aucun header Workbench réinjecté
-> validation du fichier local
```

L'URL signée complète ne doit jamais être loggée.


### Previews miniatures

Les miniatures utilisent un chemin de cache séparé et non critique. Un `QNetworkAccessManager` local peut appeler `ignoreSslErrors()` uniquement pour ce téléchargement d’image, puis valide le contenu et l’écrit atomiquement. Cette exception ne concerne ni les APIs cloud authentifiées ni les téléchargements de fichiers utilisateur.

Le listing peut fournir plusieurs références de preview. ACM conserve désormais une liste ordonnée et les essaie successivement : `thumbnail` au niveau racine, `img`/`image`, `slice_param.image_id`, `printer_image_id`, puis `slice_param.image0_id`. Les chemins relatifs de `slice_param` ne sont développés que si le bucket et la région sont disponibles. Chaque tentative, échec et source retenue est journalisé avec une URL redacted.

Le bridge n’expose à QML qu’une URL locale `file://` validée, ou une valeur vide. Un échec backend ne retombe jamais sur l’URL HTTP distante. Les fichiers encore signalés `PROCESSING` ne déclenchent pas de téléchargement de miniature. Les requêtes de miniature ont un timeout borné et un cache négatif mémoire court pour les échecs répétés `403`, `404`, timeout ou transitoires.

Le modèle sépare `thumbnailSourceUrl` de l’URL locale `thumbnailUrl`. Les URLs source contenant informations utilisateur, query ou fragment restent uniquement en mémoire pour la requête courante et ne sont pas persistées. Les logs utilisent `logging::safeUrlForLogs()`, qui conserve schéma, host et path mais supprime informations utilisateur, query et fragment.

### Upload

```text
lockStorageSpace
-> PUT binaire vers preSignUrl
-> newUploadFile
-> unlockStorageSpace
-> polling borné getUploadStatus
```

Le déverrouillage est tenté lorsque le workflow l'exige, même après une erreur partielle. Les erreurs de PUT, d'enregistrement et d'unlock restent distinctes. Le polling commence uniquement après l’unlock. Le statut `1` signifie prêt ; le statut `2` signifie traitement cloud. Les valeurs `gcode_id` `null`, vide, `0` numérique ou chaîne `"0"` sont des sentinelles et ne doivent jamais rendre l’upload prêt. Un upload en traitement reste un transfert réussi, mais l’UI le signale comme en attente et programme des rafraîchissements bornés du listing.

Pour les fichiers `.pwsz`, la complétion de l’aperçu est activée par défaut. Avant l’upload, ACM inspecte le répertoire central ZIP. Si `preview_images/preview_2.png` est absent alors que `preview_1.png` existe, l’UI demande confirmation sauf si l’utilisateur a désactivé cette confirmation. La couche infra crée une archive temporaire dans le même dossier et duplique les octets compressés de `preview_1.png` sous le nom `preview_2.png`, sans décoder ni redimensionner le PNG. Le pipeline d’upload normal lit la taille et le contenu de cette archive préparée tout en conservant le nom de fichier original pour la requête cloud.

Le fichier local source n’est remplacé atomiquement par l’archive préparée qu’après l’enregistrement réussi de l’upload cloud. Un échec de préparation, de session, de lock, de PUT ou d’enregistrement supprime l’archive temporaire et laisse l’original intact. Si l’upload cloud réussit mais que le remplacement local échoue, le fichier préparé est conservé comme copie de récupération et l’UI signale une désynchronisation.

### Commandes imprimante et impression

Une commande HTTP initie l'action. Le résultat opérationnel final peut arriver plus tard via MQTT. L'acceptation HTTP signifie donc **requête acceptée**, pas **impression confirmée**.

## Cache et fallback

Le cache accélère le démarrage et fournit un fallback étiqueté. Il ne redéfinit ni l'autorité cloud ni l'arbitrage MQTT.

## Diagnostic

En cas d'échec, conserver l'identifiant d'endpoint, le statut HTTP, le code API, le message redacted et la corrélation utile. Ne jamais conserver bearer, cookies, session ou query signée.

La [Matrice runtime des endpoints cloud](annexes/endpoints-cloud-runtime.md) est dérivée de `EndpointRegistry.cpp` et des propriétaires API actifs. Toute évolution du registre impose sa mise à jour dans le même correctif.

### Mise à jour des PWSZ cloud avec miniature invalide

Une miniature téléchargée n’est mise en cache que si son payload fait au moins 100 octets et si Qt peut la décoder. Un payload inférieur à 100 octets est classé comme placeholder Anycubic vide, n’est pas écrit dans le cache et marque le fichier `.pwsz` prêt comme candidat à une mise à jour. Un rafraîchissement forcé ignore les miniatures déjà en cache et applique la même validation.

À la fin du rafraîchissement des fichiers, QML peut proposer une modification groupée des entrées concernées. Le use case C++ trie d’abord les candidats par date de création cloud croissante, puis par identifiant cloud numérique croissant lorsque les dates sont identiques, et les traite séquentiellement du fichier le plus ancien au plus récent. Il télécharge ensuite le PWSZ original, duplique `preview_images/preview_1.png` sous le nom `preview_2.png`, envoie une version cloud normale portant directement le nom d’affichage original, attend le traitement cloud, valide la nouvelle miniature, puis seulement supprime l’ancien identifiant. Aucun endpoint de renommage non observé n’est ajouté. Si la validation échoue, la nouvelle version est supprimée lorsque possible et l’original reste la référence. Si la suppression de l’ancien identifiant échoue, les deux versions sont conservées et le résultat est signalé comme modification partielle.

Le téléchargement du PWSZ original est écrit en flux dans un `QSaveFile` atomique par blocs de 64 Kio ; l’archive complète n’est jamais accumulée en mémoire. Après l’enregistrement, `unlockStorageSpace` est retenté un nombre borné de fois et son résultat conditionne toute étape destructive. Si l’unlock n’est pas confirmé, l’ancien fichier n’est jamais supprimé et le batch s’arrête avec un résultat partiel. L’annulation est propagée depuis `CloudBridge` jusqu’au use case, interrompt les téléchargements PWSZ et les PUT présignés actifs, coupe les attentes de polling et arrête le traitement avant la phase destructive suivante. Si l’annulation intervient après l’enregistrement, les deux versions sont conservées plutôt que de risquer la perte de l’original.

La découverte des candidats repose sur un inventaire cloud complet et borné, pas sur la première page affichée. Le listing est parcouru par pages d’au moins 100 entrées, dédupliqué par identité de fichier cloud et limité à 100 pages. Seule une page vide confirme la fin, afin qu’une limite de taille imposée côté serveur ne tronque pas la découverte. Un échec de page, une page pleine répétée sans progression ou l’atteinte de la limite rendent l’inventaire incomplet ; ACM rafraîchit encore la page visible en fallback, mais ne propose aucune opération groupée destructive à partir d’informations partielles.

La validation de la miniature après upload délègue au même flux de cache local que l’affichage normal des fichiers. Elle conserve ainsi l’exception `ignoreSslErrors()` limitée aux miniatures, la validation du contenu, l’écriture atomique du cache et l’annulation, sans étendre le contournement SSL aux APIs cloud authentifiées ni aux téléchargements de fichiers utilisateur.
