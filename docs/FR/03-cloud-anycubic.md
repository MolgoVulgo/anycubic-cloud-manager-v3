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

### Commandes imprimante et impression

Une commande HTTP initie l'action. Le résultat opérationnel final peut arriver plus tard via MQTT. L'acceptation HTTP signifie donc **requête acceptée**, pas **impression confirmée**.

## Cache et fallback

Le cache accélère le démarrage et fournit un fallback étiqueté. Il ne redéfinit ni l'autorité cloud ni l'arbitrage MQTT.

## Diagnostic

En cas d'échec, conserver l'identifiant d'endpoint, le statut HTTP, le code API, le message redacted et la corrélation utile. Ne jamais conserver bearer, cookies, session ou query signée.

La [Matrice runtime des endpoints cloud](annexes/endpoints-cloud-runtime.md) est dérivée de `EndpointRegistry.cpp` et des propriétaires API actifs. Toute évolution du registre impose sa mise à jour dans le même correctif.
