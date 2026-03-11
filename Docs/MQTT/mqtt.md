# MQTT

## 1. But du document

Définir l’intégration MQTT de l’application pour les imprimantes **résine** Anycubic.

Le rôle du MQTT n’est pas de remplacer toute la logique applicative, mais de fournir un **flux temps réel** pour suivre l’état de l’imprimante et de l’impression sans faire de refresh permanent de l’interface.

Ce document sert de base de travail pour :

- cadrer le périmètre MQTT utile ;
- séparer clairement HTTP, MQTT et logique métier ;
- préparer une phase d’exploration large des messages non encore documentés ;
- définir une architecture extensible pour les modèles récents, notamment **Photon M7**.

---

## 2. Contexte

L’écosystème Anycubic expose deux canaux principaux :

- **HTTP / Cloud API** : utilisé pour les actions explicites, les snapshots complets, la récupération initiale de données et la resynchronisation ;
- **MQTT** : utilisé pour les retours asynchrones, les changements d’état et la télémétrie temps réel.

Dans l’application cible :

- l’utilisateur déclenche une action via HTTP ;
- l’application récupère ensuite les évolutions de l’état via MQTT ;
- l’interface lit un **store centralisé**, sans parser directement les messages MQTT.

---

## 3. Périmètre

### 3.1 Inclus en priorité

- imprimantes **résine** ;
- suivi temps réel pendant l’impression ;
- corrélation entre `sendOrder` et retours MQTT ;
- capture de messages inconnus pour analyse ;
- support progressif des télémétries spécifiques M7.

### 3.2 Hors périmètre initial

- logique FDM avancée ;
- ACE / multi-color box ;
- gestion complète des périphériques non critiques ;
- exposition immédiate de toutes les nouvelles télémétries dans l’UI.

### 3.3 Règle de cadrage

La priorité n’est pas le support total du protocole.

La priorité est :

1. obtenir un suivi fiable de l’impression résine ;
2. capturer un maximum de données réelles ;
3. promouvoir ensuite les champs utiles vers le domaine métier.

---

## 4. Objectifs fonctionnels

### 4.1 Objectifs MVP

- afficher l’état online/offline de l’imprimante ;
- afficher le statut général libre/occupé ;
- suivre le cycle d’impression sans refresh manuel ;
- suivre la progression de l’impression ;
- refléter pause, reprise, arrêt, fin, erreur ;
- maintenir un état cohérent après reconnexion.

### 4.2 Objectifs d’exploration

- journaliser les messages non documentés ;
- observer les variations propres à la M7 ;
- identifier les champs liés à :
  - niveau de résine ;
  - température du bac ;
  - résistance de levage ;
  - autres capteurs ou états internes.

---

## 5. Principes d’architecture

### 5.1 Séparation des rôles

#### HTTP

Responsable de :

- l’envoi des commandes utilisateur ;
- la récupération initiale des snapshots ;
- la resynchronisation complète après perte de synchro.

#### MQTT

Responsable de :

- la réception temps réel ;
- les retours asynchrones ;
- les changements d’état de l’imprimante ;
- les télémétries d’exécution.

#### Store central

Responsable de :

- consolider l’état courant ;
- exposer une vue stable à l’UI ;
- isoler l’interface des détails du protocole.

### 5.2 Règle de source de vérité

- **MQTT** = vérité temps réel pendant la session ;
- **HTTP** = vérité de resynchronisation et point d’entrée des commandes.

### 5.3 Règle UI

L’UI ne lit jamais directement un topic MQTT.

L’UI lit uniquement un modèle/store applicatif.

---

### 5.4 Connexion au broker Anycubic

#### Paramètres de connexion

Connexion MQTT via le serveur Anycubic avec les paramètres suivants :

- hôte : `mqtt-universe.anycubic.com` ;
- port : `8883` ;
- transport : `TLS` ;
- protocole : MQTT `3.1.1` ;
- `clean_session = true` ;
- `keepalive = 1200` ;
- `QoS = 0` pour les abonnements ;
- `retain = false` pour les publications éventuelles.

#### Pré-requis de session

Avant toute connexion MQTT, l’application doit disposer d’une session cloud valide contenant au minimum :

- `token` ;
- `email` ;
- `user_id` ;
- `mode_auth`.

Le MQTT ne doit pas être initialisé tant que ces éléments ne sont pas présents.

### 5.5 Authentification MQTT

#### Modes à prévoir

Deux modes d’authentification sont à considérer dans l’architecture :

- `ANDROID` ;
- `SLICER`.

Le mode `WEB` ne doit pas être considéré comme le mode normal de connexion MQTT.

#### Construction du `client_id`

- mode Android : `md5(email)` ;
- mode Slicer : `md5(email + "pcf")`.

#### Construction du `username`

Le `username` suit la forme :

`user|<mqtt_app_id>|<email>|<signature_md5>`

avec :

`signature_md5 = md5(client_id + mqtt_token + client_id)`

#### Construction du `password`

Selon le mode d’authentification :

- Android : `bcrypt(md5(auth_token))` ;
- Slicer : chiffrement `RSA PKCS#1 v1.5` du `auth_token` UTF-8 puis encodage Base64.

#### Règle d’implémentation

La logique de calcul des credentials MQTT doit être isolée dans un composant dédié.

L’UI et la logique métier ne doivent jamais reconstruire ces éléments.

### 5.6 Souscriptions MQTT

#### Topics de niveau utilisateur

Souscriptions à prévoir pour les retours liés au compte :

- `anycubic/anycubicCloud/v1/server/app/<user_id>/<user_id_md5>/slice/report`
- `anycubic/anycubicCloud/v1/server/app/<user_id>/<user_id_md5>/fdmslice/report`

Ces topics servent principalement aux flux transverses rattachés au compte utilisateur.

#### Topics de niveau imprimante

Souscriptions à prévoir pour chaque imprimante surveillée :

- `anycubic/anycubicCloud/v1/printer/app/<machine_type>/<printer_key>/#`
- `anycubic/anycubicCloud/v1/+/public/<machine_type>/<printer_key>/#`

Ces topics sont la base du suivi temps réel des imprimantes.

#### Remarque de cadrage

Même si des publications MQTT vers l’imprimante existent dans l’écosystème Anycubic, la stratégie cible de l’application reste :

- commandes utilisateur via HTTP ;
- suivi d’état via MQTT.

### 5.7 Séquence de connexion

#### Démarrage standard

À l’ouverture d’une session applicative ou d’une page imprimante :

1. charger la session cloud ;
2. construire les credentials MQTT ;
3. initialiser le client MQTT ;
4. ouvrir la connexion TLS ;
5. s’abonner aux topics utilisateur ;
6. s’abonner aux topics des imprimantes connues ;
7. activer le routage des messages vers le store central.

#### Règle de timing

La connexion MQTT doit être établie une fois la session cloud validée.

La souscription aux topics imprimante doit pouvoir être étendue dynamiquement quand une nouvelle imprimante est découverte ou sélectionnée.

### 5.8 Resubscribe et reconnexion

#### Reconnexion automatique

Le client MQTT doit prévoir une politique de reconnexion automatique avec temporisation.

En cas de reconnexion :

1. rétablir la connexion broker ;
2. resouscrire tous les topics utilisateur ;
3. resouscrire tous les topics imprimante actifs ;
4. déclencher un snapshot HTTP si la cohérence de l’état est douteuse.

#### Règle de cohérence

Le MQTT redonne le flux temps réel, mais un reconnect ne garantit pas à lui seul la récupération de tous les événements manqués.

Après une coupure réseau, un snapshot HTTP peut être nécessaire pour réhydrater l’état complet.

### 5.9 Routage des messages reçus

#### Pipeline recommandé

Chaque message reçu suit le pipeline suivant :

1. identification de la source ;
2. extraction de l’envelope ;
3. lecture de la signature `type/action/state` ;
4. tentative de corrélation avec une commande en vol ;
5. mise à jour du store métier si la signature est connue ;
6. archivage dans le store discovery si la signature est inconnue ou partiellement comprise.

#### Règle anti-couplage

Le code de souscription, le parseur protocolaire et les mises à jour de store doivent rester séparés.

La couche réseau ne doit pas contenir de logique UI.

### 5.10 Exemple de découpage technique

Exemple de composants à prévoir :

- `MqttSessionManager` : connexion, reconnexion, état du broker ;
- `MqttCredentialBuilder` : calcul `client_id`, `username`, `password` ;
- `MqttTopicFactory` : construction des topics user et printer ;
- `MqttMessageRouter` : dispatch des messages reçus ;
- `OrderResponseTracker` : corrélation `sendOrder -> msgid -> MQTT` ;
- `PrinterRealtimeStore` : état métier consolidé ;
- `TelemetryObservationStore` : capture discovery et signatures inconnues.

## 6. Flux cible

### 6.1 Ouverture d’une page imprimante

1. chargement d’un snapshot HTTP ;
2. création ou activation de la session MQTT ;
3. abonnement aux topics utiles ;
4. mise à jour du store à chaque message reçu ;
5. rafraîchissement visuel automatique.

### 6.2 Envoi d’une commande

1. action utilisateur ;
2. appel HTTP `sendOrder` ;
3. récupération du `msgid` si présent ;
4. attente du retour MQTT ;
5. corrélation de la réponse ;
6. mise à jour du store.

### 6.3 Reconnexion / désynchronisation

1. perte session MQTT ou doute sur l’état ;
2. reconnexion broker ;
3. nouveau snapshot HTTP ;
4. réhydratation du store ;
5. reprise du temps réel.

---

## 7. Commandes prioritaires

### 7.1 Commandes `sendOrder` à supporter en premier

- `order_id=1` : démarrer une impression ;
- `order_id=2` : pause ;
- `order_id=3` : reprise ;
- `order_id=4` : arrêt ;
- `order_id=6` : paramètres / mise à jour de paramètres d’impression.

### 7.2 Rôle des commandes

Ces commandes n’ont pas vocation à fournir leur vérité métier complète dans la réponse HTTP.

Le rôle de la réponse HTTP est principalement :

- valider l’acceptation de la commande ;
- renvoyer un `msgid` ;
- permettre la corrélation avec la suite des événements MQTT.

---

## 8. Événements MQTT prioritaires

### 8.1 Disponibilité imprimante

Exemples attendus :

- `onlineReport / online` ;
- `onlineReport / offline`.

### 8.2 Occupation générale

Exemples attendus :

- `status / workReport / free` ;
- `status / workReport / busy`.

### 8.3 Cycle d’impression

Exemples attendus :

- `print / start / downloading` ;
- `print / start / checking` ;
- `print / start / preheating` ;
- `print / start / printing` ;
- `print / pause / pausing` ;
- `print / pause / paused` ;
- `print / resume / resuming` ;
- `print / resume / resumed` ;
- `print / stop / stopping` ;
- `print / stop / stoped` ;
- `print / start / finished` ;
- `print / start / failed`.

### 8.4 Champs complémentaires à observer

À confirmer sur flux réels :

- progression ;
- temps restant ;
- temps écoulé ;
- nom du job ;
- température ;
- capteurs résine ;
- états mécaniques.

---

## 8.5 Topics observés dans l’application officielle

Les logs officiels confirment que l’application se connecte au broker MQTT, effectue ses souscriptions, puis reçoit des messages sur des topics imprimante réels.

### Pattern observé sur imprimante résine M7

Le pattern observé est de la forme :

`anycubic/anycubicCloud/v1/printer/public/<machine_type>/<printer_key>/<channel>/report`

Exemple de construction :

- `machine_type = 128` pour une M7 Pro ;
- `printer_key = clé machine cloud`.

### Wildcard recommandé

Pour éviter de maintenir une liste figée de canaux, la souscription de base doit être :

`anycubic/anycubicCloud/v1/printer/public/<machine_type>/<printer_key>/#`

Cette souscription permet de capter tous les retours utiles d’une imprimante donnée pendant la phase discovery.

### Canaux effectivement observés

Les canaux suivants ont été vus dans les logs :

- `releaseFilm/report` ;
- `status/report` ;
- `file/report` ;
- `print/report` ;
- `lastWill/report` ;
- `ota/report` ;
- `user/report` ;
- `wifi/report`.

### Conséquence d’architecture

Le routeur MQTT ne doit pas supposer que seuls `print` et `status` existent.

Le domaine doit accepter l’arrivée progressive de nouvelles familles de messages.

## 8.6 Signatures observées sur M7

Les captures plus récentes montrent que la M7 Pro ne se limite pas aux familles génériques déjà identifiées.

Les logs confirment plusieurs flux utiles pour affiner le modèle temps réel.

### `print` — cycle réel observé

Les logs montrent au moins deux signatures distinctes pendant le démarrage et le suivi d’une impression :

- `action = 108`, `state = downloading` ;
- `action = 103`, `state = printing`.

Le même handler officiel traite ces événements sous une forme interne de type `PrintDownloading` / `onPrintStart`.

### Interprétation provisoire

À ce stade, l’interprétation la plus crédible est :

- `108 / downloading` = phase de transfert ou de préparation de fichier ;
- `103 / printing` = entrée ou maintien dans l’état d’impression effective.

Cette interprétation doit rester classée comme **hypothèse validée par observation**, tant que la matrice complète des actions n’est pas figée.

### Données visibles dans les événements `print`

Les événements observés contiennent notamment :

- `fileName` ;
- `action` ;
- `state` ;
- `deviceKey` ;
- `code` ;
- message d’erreur interprété côté application.

### Conséquence de conception

Le domaine d’impression ne doit pas se contenter d’un enum figé du type `downloading / printing / paused / finished`.

Il doit aussi conserver :

- l’`action` numérique brute ;
- le `state` texte ;
- le dernier nom de fichier connu ;
- la dernière transition observée.

## 8.7 Signatures résine observées sur M7

Les nouveaux logs montrent explicitement l’existence d’un flux `ResinReport` dédié.

### Handler observé

L’application officielle traite ces messages via un handler de type :

- `onFeedResin(... Report::ResinReport ...)`

### Signature observée

Les événements vus suivent le schéma :

- `action = 1701`
- `state = start`
- `feed_type = <n>`
- `status = <n>`
- `errMsg = ...`

### Variantes déjà observées

Exemples vus dans les logs :

- `feed_type=2`, `status=2` ;
- `feed_type=1`, `status=3` ;
- `feed_type=0`, `status=0`, avec message de fin manuelle ;
- `feed_type=0`, `status=0`, avec message d’overflow lors du déchargement.

### Interprétation provisoire

Le constructeur semble distinguer plusieurs sous-modes d’alimentation / reprise / retour matière.

Il est donc recommandé de ne pas réduire ce flux à un simple booléen du style `resin_loading = true/false`.

La bonne modélisation initiale est plutôt :

- action brute ;
- state ;
- feed type ;
- status ;
- message associé ;
- timestamp.

### Corrélation probable avec commande utilisateur

Les logs montrent qu’un événement de contrôle imprimante avec `printerOrder = 1224` précède ces retours résine.

Cela suggère qu’au moins une commande de contrôle résine alimente cette famille MQTT.

Cette corrélation doit être documentée comme **probable** tant qu’un mapping exhaustif `printerOrder -> action MQTT` n’est pas reconstitué.

## 8.8 Signatures `releaseFilm` observées

Les nouveaux logs confirment que `releaseFilm` n’est pas un événement isolé, mais une télémétrie répétée pendant l’activité machine.

### Structure observée

- `type = releaseFilm`
- `action = get`
- `state = done`
- `data.times`
- `data.layers`
- `data.status`

### Comportement observé

Les valeurs `times` et `layers` évoluent dans le temps pendant l’impression ou pendant les séquences mécaniques associées.

Cela en fait un bon candidat pour une télémétrie de suivi M7 liée au décollement, à la mécanique de séparation, ou à un compteur interne de cycle.

### Règle de traitement

Dans le store métier, cette famille doit d’abord rester en télémétrie spécialisée M7.

Elle ne doit pas être transformée trop tôt en libellé UX trompeur.

## 8.9 Indices fournis par les DLL MQTT

L’analyse statique des DLL officielles permet d’identifier des éléments de protocole non encore pleinement visibles dans les logs.

### Couche client MQTT

La DLL de transport MQTT s’appuie sur une implémentation asynchrone classique avec TLS.

Cela confirme que l’intégration ne repose pas sur un protocole propriétaire totalement opaque au niveau transport.

### Templates de topics identifiés

Des templates de topics imprimante et slicer ont été retrouvés dans les chaînes internes.

Le plus utile pour le suivi temps réel imprimante est :

`anycubic/anycubicCloud/v1/printer/public/{}/{}/#`

Un second template orienté slicer/publication a également été repéré.

### Réponses métier typées prévues par la DLL

Les DLL exposent des familles de réponses ou structures internes suggérant l’existence de télémétries déjà prévues par le constructeur, notamment :

- réponses d’impression ;
- réponses de monitoring ;
- réponses de préchauffage ;
- réponses de téléchargement ;
- réponses liées à la résine ;
- réponses liées à la température ;
- réponses liées à l’éclairage ;
- réponses liées aux périphériques ;
- réponses de fichiers ;
- réponses OTA.

### Conséquence de conception

Même si tous ces messages ne sont pas encore observés dans les captures actuelles, l’architecture doit être prête à les accueillir sans refonte.

Le mode discovery n’est donc pas un luxe, mais une nécessité.

## 8.10 Format de fichiers et spécificité cloud M7

Les logs cloud les plus récents montrent que la M7 Pro ne doit pas être pensée uniquement comme héritière des anciens flux `pwmb`.

### Format machine observé

La machine M7 Pro exposée par le cloud annonce un suffixe de fichier :

- `suffix = pwsz`

Cela signifie qu’une partie des workflows modernes M7 s’appuie sur des fichiers `pwsz` côté cloud et côté impression.

### Conséquence de conception

Le pipeline Cloud / impression / viewer ne doit donc pas être limité à `pwmb` dans sa couche applicative.

Il doit accepter au minimum :

- `pwmb` ;
- `pwsz`.

### Règle projet

Même si le viewer 3D avancé peut rester focalisé sur `pwmb` dans un premier temps, la couche cloud et la couche MQTT doivent rester neutres vis-à-vis de l’extension du fichier.

## 8.11 Codes de raison M7 enrichis

Les derniers logs cloud montrent un catalogue de raisons plus riche que celui déjà relevé.

### Familles de codes à prendre en compte

Au-delà des raisons d’impression génériques, la M7 expose des familles orientées résine, capteurs et modules intelligents, notamment :

- volume de résine ;
- détection de casse ou chute modèle ;
- détection de plateforme ;
- détection de résidus ;
- anomalies de bouteille pendant alimentation / retour matière ;
- bac intelligent non installé ;
- module load & unload non installé ;
- chauffage anormal ;
- capteur de tension ;
- objet étranger lors d’un mouvement sous pression ;
- absence plateau ;
- warping ;
- obstruction de détection caméra.

### Conséquence de modélisation

Le moteur d’interprétation des erreurs ne doit pas être un simple mapping “print failed -> message”.

Il doit être conçu comme un registre extensible de raisons :

- code numérique ;
- famille ;
- sévérité ;
- catégorie UI ;
- action recommandée ;
- caractère bloquant ou non.

### Règle discovery

Quand un code apparaît dans MQTT ou dans un retour HTTP sans mapping métier complet, il doit être conservé tel quel dans le store debug avec son texte constructeur.

## 8.12 Observations pratiques sur l’impression M7

Les captures montrent que le flux temps réel utile pour une impression M7 active peut combiner :

- disponibilité générale (`free` / `busy`) ;
- phases d’impression (`downloading`, `printing`) ;
- télémétrie `releaseFilm` ;
- événements résine spécialisés.

### Conclusion pratique

Pour la M7, la vérité temps réel ne doit pas être reconstruite depuis une seule famille de messages.

Elle doit résulter d’une fusion de plusieurs flux spécialisés.

## 8.13 Spécificités M7 à surveiller

Les logs officiels montrent que les modèles récents, notamment M7, renvoient déjà des messages plus riches que le simple cycle d’impression standard.

### Familles déjà observées

#### `releaseFilm`

Exemple de structure observée :

- `type = releaseFilm`
- `action = get`
- `state = done`
- `data.times`
- `data.layers`
- `data.status`

Cette famille semble liée au comportement mécanique ou au suivi de relâchement/décollement pendant l’impression.

#### `file`

Exemples d’actions observées :

- `listLocal`
- `listUdisk`

Les retours contiennent notamment des tableaux `records` avec des métadonnées de fichiers.

#### `status`

Des transitions `free` et `busy` sont observées.

#### `print`

Des événements de monitoring ont été vus avec des actions numériques et un état `monitoring`.

Cela indique que le flux d’impression réel contient des variantes supplémentaires qui ne doivent pas être perdues pendant la phase d’analyse.

#### `wifi`

Des messages de signal réseau sont observés, par exemple via une action du type `getSignalStrength`.

#### `user`

Des messages liés au statut de liaison compte/imprimante sont observés, par exemple via une action du type `bindQuery`.

#### `lastWill`

Des événements de présence réseau sont observés avec `onlineReport`.

Ces messages doivent être interprétés avec prudence, car certains champs peuvent être contradictoires selon le contexte ou la logique interne du constructeur.

## 8.7 Indices fournis par les DLL MQTT

L’analyse statique des DLL officielles permet d’identifier des éléments de protocole non encore pleinement visibles dans les logs.

### Couche client MQTT

La DLL de transport MQTT s’appuie sur une implémentation asynchrone classique avec TLS.

Cela confirme que l’intégration ne repose pas sur un protocole propriétaire totalement opaque au niveau transport.

### Templates de topics identifiés

Des templates de topics imprimante et slicer ont été retrouvés dans les chaînes internes.

Le plus utile pour le suivi temps réel imprimante est :

`anycubic/anycubicCloud/v1/printer/public/{}/{}/#`

Un second template orienté slicer/publication a également été repéré.

### Réponses métier typées prévues par la DLL

Les DLL exposent des familles de réponses ou structures internes suggérant l’existence de télémétries déjà prévues par le constructeur, notamment :

- réponses d’impression ;
- réponses de monitoring ;
- réponses de préchauffage ;
- réponses de téléchargement ;
- réponses liées à la résine ;
- réponses liées à la température ;
- réponses liées à l’éclairage ;
- réponses liées aux périphériques ;
- réponses de fichiers ;
- réponses OTA.

### Conséquence de conception

Même si tous ces messages ne sont pas encore observés dans les captures actuelles, l’architecture doit être prête à les accueillir sans refonte.

Le mode discovery n’est donc pas un luxe, mais une nécessité.

## 8.8 Spécificités M7 à surveiller

Les éléments collectés dans les logs et les retours cloud indiquent que les imprimantes M7 disposent de remontées plus riches que les générations précédentes.

Les familles suivantes doivent être surveillées en priorité :

- détection de volume de résine ;
- anomalie de quantité de résine dans le bac ;
- états du bac intelligent ;
- température / chauffage ;
- résistance ou force de relâchement ;
- état du plateau ;
- détection caméra ;
- warnings et erreurs spécifiques au modèle.

### Règle de traitement

Au début du projet, toutes ces remontées doivent être capturées et classées, même si leur sens métier n’est pas encore stabilisé.

La promotion dans le domaine principal ne doit intervenir qu’après validation expérimentale.

## 8.9 Sécurité et hygiène des captures

Les logs officiels peuvent contenir :

- tokens ;
- identifiants de session ;
- emails ;
- identifiants utilisateur ;
- clés machine.

### Règle projet

Tout log partagé, archivé ou intégré dans la documentation doit être nettoyé avant diffusion.

Le système de debug local doit permettre la capture utile du protocole sans exposer inutilement les secrets.

## 9. Mode discovery

### 9.1 But

Le protocole évolue selon les modèles et les générations d’imprimantes.

Pour éviter de bloquer la progression sur une documentation incomplète, l’application doit intégrer un **mode discovery** permettant de capturer et classifier tous les messages observés.

### 9.2 Principe

Pour chaque message reçu, conserver :

- imprimante concernée ;
- topic ;
- timestamp ;
- `type` ;
- `action` ;
- `state` ;
- `msgid` ;
- payload brut ;
- signature normalisée ;
- contexte d’impression courant.

### 9.3 Règle de tolérance

Un message inconnu :

- ne casse pas la session ;
- ne bloque pas l’UI ;
- ne pollue pas immédiatement le modèle métier ;
- est conservé pour analyse.

### 9.4 Cible spécifique M7

La M7 semble renvoyer des informations supplémentaires encore peu documentées.

Le mode discovery devra permettre d’identifier proprement les familles de données liées à :

- résine ;
- bac ;
- mécanique de levage ;
- télémétrie avancée ;
- erreurs et warnings spécifiques modèle.

---

## 10. Stratégie de modélisation

### 10.1 Envelope stricte

Valider uniquement les champs structurels stables :

- origine du message ;
- type ;
- action ;
- state si présent ;
- identité imprimante.

### 10.2 Payload souple

Le contenu détaillé est accepté même si tout n’est pas encore compris.

Les champs non mappés restent disponibles sous forme brute dans la couche d’analyse.

### 10.3 Promotion contrôlée

Un champ ou une signature n’entre dans le domaine métier que si :

1. il apparaît de façon répétable ;
2. son sens est stable ;
3. il apporte une vraie valeur fonctionnelle.

---

## 11. Modèle de données cible

### 11.1 Store métier principal

Exemple de responsabilités :

- état de connexion imprimante ;
- état de disponibilité ;
- statut d’impression ;
- progression ;
- erreur courante ;
- dernier job connu.

### 11.2 Store de découverte

Exemple de responsabilités :

- journal brut ;
- signatures inconnues ;
- fréquence par signature ;
- dernier payload observé ;
- marquage par modèle d’imprimante.

### 11.3 Registre de corrélation

Exemple de responsabilités :

- suivi des commandes en vol ;
- association `sendOrder -> msgid -> retour MQTT` ;
- expiration/timeout ;
- fallback contrôlé si `msgid` absent.

---

## 12. Règles de corrélation

### 12.1 Cas nominal

- une commande HTTP renvoie un `msgid` ;
- le prochain événement MQTT portant ce `msgid` est associé à la commande ;
- le store est mis à jour ;
- la commande est clôturée.

### 12.2 Cas dégradé

Si le `msgid` est absent ou inexploitable :

- utiliser une corrélation par imprimante + type de commande + fenêtre temporelle ;
- limiter le nombre de commandes simultanées dans une même classe ;
- rejeter les rapprochements ambigus.

---

## 13. Observabilité

### 13.1 Logs applicatifs

Prévoir des logs structurés pour :

- connexion broker ;
- abonnement topics ;
- messages reçus ;
- commandes envoyées ;
- corrélations réussies ;
- corrélations échouées ;
- messages inconnus.

### 13.2 Panneau debug

Prévoir un écran ou panneau technique affichant :

- derniers événements MQTT ;
- signatures inconnues ;
- payload brut ;
- état du broker ;
- commandes en attente.

---

## 14. Tests

### 14.1 Tests unitaires

- parsing envelope ;
- tolérance aux champs inconnus ;
- corrélation `msgid` ;
- routing par signature ;
- promotion vers le store métier.

### 14.2 Tests d’intégration

- ouverture de page imprimante ;
- snapshot HTTP puis flux MQTT ;
- start / pause / resume / stop ;
- reconnexion broker ;
- resync HTTP après perte de messages.

### 14.3 Tests discovery

- réception d’une signature non connue ;
- conservation du payload brut ;
- absence d’impact UI ;
- journalisation correcte ;
- visibilité dans le panneau debug.

---

## 15. Plan d’implémentation

### Étape 1 — Base technique

- client MQTT ;
- gestion connexion/reconnexion ;
- souscription aux topics imprimante ;
- log minimal.

### Étape 2 — Domaine minimal résine

- disponibilité ;
- occupation ;
- cycle d’impression ;
- progression.

### Étape 3 — Corrélation commandes

- registre de commandes ;
- suivi `msgid` ;
- timeouts ;
- fallback contrôlé.

### Étape 4 — Discovery M7

- capture large ;
- classification des signatures ;
- analyse des payloads ;
- premiers regroupements sémantiques.

### Étape 5 — Promotion métier

- sélection des champs stables ;
- intégration dans le store principal ;
- ajout contrôlé à l’UI.

---

## 16. Points ouverts

- topics exacts à figer par modèle ;
- structure détaillée des retours M7 ;
- stratégie de persistance éventuelle des observations brutes ;
- seuil de rétention logs/debug ;
- règles exactes de fallback si `msgid` absent.

---

## 17. Conclusion

L’intégration MQTT doit être pensée comme une couche temps réel fiable, centrée sur l’impression résine, avec une capacité native à absorber l’inconnu.

La première version ne doit pas chercher à tout comprendre immédiatement.

Elle doit :

- suivre correctement l’impression en live ;
- résister aux évolutions de protocole ;
- capturer le maximum d’informations réelles ;
- permettre une montée en couverture méthodique, notamment pour la M7.

