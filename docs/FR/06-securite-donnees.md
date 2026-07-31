# Sécurité, logs, cache et données

## En bref

L'application manipule des tokens web réutilisables, des credentials MQTT, du matériel de clé privée et des URLs signées. La sécurité repose sur des frontières de données strictes, une observabilité redacted et un stockage runtime local.

## Matière sensible

Ne jamais commiter, packager ou afficher :

- captures HAR ;
- contenu de `session.json` ;
- access, refresh, ID ou MQTT auth tokens ;
- headers `Authorization` et `Cookie` ;
- clés privées TLS ;
- URLs signées complètes ;
- credentials ou passwords bruts.

Les tests utilisent uniquement des valeurs synthétiques.

## Chemins runtime

La racine par défaut est `~/.local/share/accloud`. Session, paramètres, cache, miniatures, temporaires et logs sont des données runtime exclues des patchs et archives sources.

## Logs et représentation des URLs

Les logs sont structurés et redacted. Les champs utiles comprennent composant, événement, statut, identifiant d'endpoint, clé imprimante, corrélation et erreur bornée.

Une valeur n'est pas sûre parce que sa clé est générique. Toute URL susceptible de contenir des credentials temporaires doit passer par `logging::safeUrlForLogs()` avant journalisation. La forme sûre conserve schéma, host et path, supprime les informations utilisateur de l'URL, puis retire intégralement query string et fragment.

L'URL complète reste utilisée en interne pour la requête et le hash du cache miniature. Seule sa représentation dans les logs est réduite.

## Log brut de trafic en développement

Le preset `dev-debug` (`ACCLOUD_DEBUG=ON`) enregistre en complément des traces orientées protocole HTTP et MQTT dans `logs/log_brut.txt`. Les corps de requête et réponse HTTP restent lisibles lorsqu’ils sont textuels ; les uploads, miniatures et téléchargements binaires sont représentés par leur taille au lieu d’être copiés dans le fichier texte. Les topics et payloads MQTT entrants sont enregistrés avant le parsing applicatif.

Ce fichier n’est jamais produit par les presets `default` ou `prod`. Malgré sa finalité de diagnostic, il ne constitue pas un dump de credentials non protégé : headers d’autorisation, champs JSON de type token, cookies, identifiants persistants de compte et query strings d’URLs signées sont redacted. Le fichier reste une donnée runtime sensible, ne doit entrer dans aucun patch ni archive source et doit être supprimé après diagnostic.

## Trois cas TLS distincts

### HTTPS cloud authentifié

Les APIs Workbench, opérations de session, uploads, commandes imprimante et téléchargements de fichiers utilisateur conservent la vérification HTTPS normale. Il n'existe aucune politique globale `ignoreSslErrors()`.

### Compatibilité du broker MQTT

Le broker utilise TLS 1.2 et mTLS. `VerifyNone` et OpenSSL `SECLEVEL=0` peuvent être nécessaires au contrat de compatibilité Anycubic figé. Cette exception reste confinée au gestionnaire de session MQTT.

### Exception de preview miniature

`ignoreSslErrors()` existe uniquement dans le téléchargement synchrone utilisé pour alimenter le cache local de preview :

```text
URL miniature
-> QNetworkAccessManager local à l'opération
-> téléchargement image
-> validation format/contenu par QImageReader
-> écriture atomique QSaveFile
-> URL miniature locale
```

Il ne doit pas être déplacé vers un manager réseau global ni réutilisé pour les APIs authentifiées, l'import HAR, les commandes d'impression, les uploads, les téléchargements de fichiers utilisateur ou MQTT. La requête possède un timeout borné, valide l’image décodée avant écriture atomique et ne renvoie aucune URL distante de fallback à QML. Les URLs source de miniature signées ou porteuses de credentials ne sont jamais persistées dans SQLite.

## Cache

Le cache accélère le démarrage et fournit un fallback explicite. Les miniatures sont des données dérivées écrites atomiquement après validation du contenu. Une purge cache ne supprime pas la session sans demande explicite. Le schéma typé du cache SQLite est en version 5 ; au démarrage, la version 3 est migrée par ajout des colonnes cloud de la version 4, des colonnes de corrélation cloud/projet dans `jobs` et de la table `pending_direct_prints`. Les opérations directes ne persistent que les identifiants et décisions de nettoyage nécessaires à la reprise d’un flux borné ; la préférence de nettoyage après échec est figée dans l’opération et vaut `false` par défaut. Le remplacement du snapshot des fichiers cloud est transactionnel : les échecs de préparation, insertion et commit sont journalisés par étape, et le snapshot précédent est conservé après rollback.

## Données de référence

Les archives documentaires publiques conservent uniquement des exemples synthétiques, redacted ou agrégés. Les longues captures brutes et identifiants persistants d'imprimante/tâche restent hors de l'archive distribuable.

## Règle d'incident

Lorsqu'une validation sécurité échoue, arrêter la chaîne de validation demandée, rapporter l'échec redacted et qualifier s'il vient du patch, de l'environnement ou d'une dette préexistante. Ne pas neutraliser le contrôle.
