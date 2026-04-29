# MQTT-Final

## 1. Objet

Ce document décrit la cible d’implémentation du module MQTT natif **C++ / Qt** pour la branche `MQTT` du projet `anycubic-cloud-manager-v3`.

Le but est de donner à Codex une spécification claire et exploitable pour coder correctement le module, sans repartir des tests Python ni mélanger la logique réseau avec l’UI.

Le document couvre :

- le périmètre fonctionnel initial ;
- la stratégie d’authentification validée ;
- la connexion TLS/mTLS au broker ;
- la construction des topics ;
- l’architecture logicielle recommandée ;
- le flux runtime ;
- la gestion des erreurs et de la reconnexion ;
- la phase discovery pour les payloads encore inconnus, surtout M7.

---

## 2. Résultat déjà validé expérimentalement

Une connexion réelle au broker Anycubic a été validée avec un script Python de test.

Le chemin qui fonctionne est :

- broker : `mqtt-universe.anycubic.com`
- port : `8883`
- protocole : MQTT `3.1.1`
- transport : TLS avec **certificat client + clé client + CA**
- mode d’authentification validé : **SLICER**
- connexion acceptée : `on_connect rc=0`
- souscriptions acceptées : OK sur topics user et printer

Conclusion de projet :

**la voie d’implémentation à reproduire dans l’application Qt est la voie SLICER + mTLS**.

Il ne faut plus raisonner à partir d’un test TLS nu ou d’un connect MQTT sans certificats client.

---

## 3. Décision de cadrage

### 3.1 Cible initiale

La première implémentation C++/Qt doit supporter uniquement :

- imprimantes **résine** ;
- connexion MQTT temps réel ;
- suivi live de l’impression ;
- souscriptions user et printer ;
- mode discovery large pour messages inconnus.

### 3.2 Hors périmètre initial

- FDM avancé ;
- publication MQTT active vers la machine ;
- multi-color / ACE ;
- optimisation multi-comptes ;
- persistance longue durée des flux bruts.

### 3.3 Règle structurante

Le module MQTT n’est pas une couche UI.

Il doit rester un service technique central, indépendant de QML.

---

## 4. Paramètres de connexion validés

### 4.1 Broker

- host : `mqtt-universe.anycubic.com`
- port : `8883`
- mode : TLS
- version MQTT : `3.1.1`
- keepalive cible : `1200`
- clean session : `true`
- QoS de souscription : `0`

### 4.2 TLS / mTLS

La connexion ne doit pas être faite avec un TLS minimaliste.

Le comportement cible doit reproduire le fonctionnement validé :

- chargement d’une **CA** ;
- chargement d’un **certificat client** ;
- chargement d’une **clé privée client** ;
- TLS `1.2` ;
- configuration SSL compatible avec le comportement du client de référence.

### 4.3 Fichiers TLS

Le module doit charger les fichiers suivants depuis les ressources du projet :

- `anycubic_mqtt_tls_ca.crt`
- `anycubic_mqtt_tls_client.crt`
- `anycubic_mqtt_tls_client.key`

### 4.4 Emplacement recommandé

Les fichiers TLS doivent rester dans une zone de ressources technique, par exemple :

- `accloud/resources/mqtt/tls/`

Le code ne doit pas disperser des chemins absolus dans plusieurs classes.

---

## 5. Authentification validée

### 5.1 Mode à implémenter en priorité

Le mode à implémenter en premier est **SLICER**.

Le module ne doit pas commencer par le mode WEB ni par des variantes expérimentales non validées.

### 5.2 Entrées nécessaires

Le builder de credentials MQTT a besoin de :

- `email`
- `token`

Le `token` correspond au token cloud actuellement disponible dans l’application après authentification.

### 5.3 Construction des credentials SLICER

#### `client_id`

```text
client_id = md5(email + "pcf")
```

#### `mqtt_token`

```text
mqtt_token = Base64( RSA_PKCS1_v1_5_Encrypt( UTF8(token), mqtt_public_key ) )
```

#### `signature`

```text
signature = md5(client_id + mqtt_token + client_id)
```

#### `username`

```text
username = "user|pcf|" + email + "|" + signature
```

#### `password`

```text
password = mqtt_token
```

### 5.4 Résumé

Le chemin cible à reproduire est :

```text
client_id  = md5(email + "pcf")
password   = Base64(RSA_PKCS1_v1_5(UTF8(token)))
signature  = md5(client_id + password + client_id)
username   = "user|pcf|" + email + "|" + signature
```

### 5.5 Règle de sécurité

Les valeurs sensibles ne doivent jamais être loguées en clair en mode runtime normal.

Au maximum, les logs techniques ne doivent afficher que :

- host ;
- port ;
- client_id ;
- hash ou version masquée du username ;
- état connecté / déconnecté.

Le token et le mot de passe MQTT ne doivent pas être imprimés dans les logs applicatifs.

---

## 6. Construction des topics

### 6.1 Topics user

L’application doit toujours être capable de souscrire les topics user.

Le `user_id` doit être obtenu depuis la session cloud déjà connue par l’application.

Construction :

```text
user_id_md5 = md5(user_id)
root = anycubic/anycubicCloud/v1/server/app/<user_id>/<user_id_md5>
```

Topics :

```text
<root>/slice/report
<root>/fdmslice/report
```

### 6.2 Topics printer

Pour chaque imprimante cible :

```text
anycubic/anycubicCloud/v1/printer/public/<machine_type>/<printer_key>/#
```

Le topic `anycubic/anycubicCloud/v1/+/public/<machine_type>/<printer_key>/#` est volontairement désactivé dans `accloud` (doublon observé sur les devices concernés).

### 6.3 Source des données machine

`machine_type` et `printer_key` doivent être récupérés depuis les données HTTP cloud déjà présentes dans l’application.

Le `printer_key` n’est pas l’ID numérique de l’imprimante.

Il faut utiliser la clé cloud réellement renvoyée par la liste des imprimantes.

### 6.4 Règle de souscription

- les topics user sont souscrits dès que la session MQTT est montée ;
- les topics printer sont souscrits dynamiquement selon les imprimantes connues ou sélectionnées ;
- la classe MQTT ne doit pas coder en dur les imprimantes.

---

## 7. Architecture recommandée

### 7.1 Principe général

L’architecture doit séparer clairement :

- la session MQTT ;
- la construction des credentials ;
- la construction des topics ;
- le routage des messages ;
- le store métier ;
- le store discovery ;
- l’UI.

### 7.2 Composants recommandés

#### `MqttCredentialBuilder`

Responsable de :

- construire `client_id` ;
- construire `username` ;
- construire `password` ;
- préparer la matière crypto nécessaire.

#### `MqttTlsConfigFactory`

Responsable de :

- charger CA / cert client / clé client ;
- produire une configuration TLS/SSL Qt cohérente ;
- centraliser tous les réglages SSL.

#### `MqttTopicFactory`

Responsable de :

- construire les topics user ;
- construire les topics printer ;
- éviter les duplications de format dans le code.

#### `MqttSessionManager`

Responsable de :

- créer le client MQTT ;
- connecter / déconnecter ;
- resouscrire ;
- suivre l’état du broker ;
- exposer les signaux de connexion.

#### `MqttMessageRouter`

Responsable de :

- parser l’envelope ;
- identifier `type/action/state` ;
- router vers store métier ou store discovery.

#### `OrderResponseTracker`

Responsable de :

- corréler les commandes HTTP `sendOrder` ;
- suivre `msgid` si présent ;
- associer la réponse MQTT à une commande en vol.

#### `PrinterRealtimeStore`

Responsable de :

- état de connexion imprimante ;
- état libre/occupé ;
- statut impression ;
- progression ;
- erreurs métier connues ;
- télémétries validées.

#### `TelemetryObservationStore`

Responsable de :

- journal brut ;
- signatures inconnues ;
- payloads non promus ;
- fréquence des événements ;
- dernière observation par signature.

### 7.3 Règle de couplage

QML ne doit jamais :

- construire les topics ;
- parser les JSON MQTT ;
- construire les credentials ;
- gérer la reconnexion.

QML doit uniquement consommer un modèle/store propre.

---

## 8. Intégration Qt

### 8.1 Choix technique

Implémenter le client en **C++ Qt natif**.

La priorité est d’utiliser une solution Qt cohérente avec le projet existant, tant qu’elle permet :

- MQTT 3.1.1 ;
- configuration TLS/SSL complète ;
- certificat client + clé + CA ;
- callbacks / signaux propres ;
- reconnexion contrôlée.

### 8.2 Exigences techniques

Le client Qt choisi doit permettre :

- de définir l’hôte et le port ;
- de fournir `client_id`, `username`, `password` ;
- d’injecter une configuration SSL complète ;
- de s’abonner dynamiquement à plusieurs topics ;
- de recevoir les messages bruts et le topic associé.

### 8.3 Règle de thread

Le module MQTT doit vivre dans une couche contrôlée côté C++, avec des signaux Qt clairs.

Il ne faut pas faire de logique lourde dans les callbacks réseau.

Le pattern recommandé :

1. callback réseau léger ;
2. mise en file ou dispatch contrôlé ;
3. parsing ;
4. mise à jour du store.

---

## 9. Flux runtime cible

### 9.1 Démarrage session

1. l’utilisateur se connecte au cloud ;
2. l’application dispose de `email`, `token`, `user_id` ;
3. le module MQTT construit les credentials SLICER ;
4. le module MQTT charge la config TLS ;
5. le client se connecte au broker ;
6. souscription aux topics user ;
7. souscription aux topics printer connus.

### 9.2 Page imprimante

À l’ouverture d’une page imprimante :

1. récupération d’un snapshot HTTP si nécessaire ;
2. vérification que la session MQTT tourne ;
3. souscription au wildcard printer si non déjà actif ;
4. affichage live à partir du store temps réel.

### 9.3 Envoi d’une commande

1. l’utilisateur déclenche une action ;
2. la commande part en HTTP `sendOrder` ;
3. `OrderResponseTracker` enregistre le contexte ;
4. le MQTT reçoit la suite ;
5. la réponse ou l’évolution d’état met à jour le store.

### 9.4 Reconnexion

En cas de coupure :

1. le module marque la session comme dégradée ;
2. tentative de reconnexion avec backoff ;
3. reconstruction des souscriptions ;
4. refresh HTTP complet si risque de perte d’événements.

---

## 10. Parsing et routing

### 10.1 Envelope minimale

Le routeur doit extraire au minimum :

- topic ;
- imprimante concernée si identifiable ;
- `type` ;
- `action` ;
- `state` ;
- `msgid` si présent ;
- payload JSON brut.

### 10.2 Messages connus à supporter en premier

#### Disponibilité

- `lastWill / onlineReport / online|offline`
- `status / workReport / free|busy`

#### Impression

- phases de téléchargement / préparation ;
- `printing` ;
- pause / reprise / arrêt ;
- fin ;
- échec.

#### Fichiers

- `file / listLocal`
- `file / listUdisk`

#### Télémétrie spécifique M7

- `releaseFilm`
- flux résine spécialisés
- autres signatures spécifiques modèle

### 10.3 Règle discovery

Un message inconnu ne doit pas :

- casser la session ;
- casser le parseur ;
- bloquer l’UI.

Il doit être stocké dans `TelemetryObservationStore` avec :

- topic ;
- signature ;
- payload brut ;
- timestamp ;
- imprimante concernée.

### 10.4 Promotion métier

Une signature n’entre dans le domaine stable que si :

1. elle apparaît de façon répétée ;
2. son sens est cohérent ;
3. elle apporte une vraie valeur fonctionnelle.

---

## 11. Spécificités M7

### 11.1 Règle de travail

Pour la M7, le module doit capturer large au départ.

Il ne faut pas réduire trop tôt les payloads à quelques états simplistes.

### 11.2 Signatures déjà observées

Le système doit être prêt à recevoir, entre autres :

- `print`
- `status`
- `file`
- `releaseFilm`
- `wifi`
- `user`
- `ota`
- `lastWill`
- événements résine dédiés

### 11.3 Télémétries à surveiller

- niveau résine ;
- anomalies de bac ;
- chauffage ;
- mécanique de levage ;
- release film ;
- warnings spécifiques M7.

### 11.4 Conséquence de modélisation

Le store principal doit rester conservateur.

Le store discovery doit absorber les nouveautés sans forcer une interprétation prématurée.

---

## 12. Journaux et observabilité

### 12.1 Logs obligatoires

Le module doit journaliser au minimum :

- tentative de connexion ;
- succès échec connexion ;
- souscriptions envoyées ;
- souscriptions acceptées ;
- déconnexion ;
- reconnexion ;
- nombre de messages reçus ;
- signatures inconnues détectées.

### 12.2 Secrets

Interdictions de log :

- token brut ;
- password MQTT ;
- clé privée ;
- certificat complet en clair ;
- payloads sensibles en mode normal.

### 12.3 Mode debug technique

Prévoir un mode debug séparé qui peut afficher davantage d’informations, mais avec redaction automatique des secrets.

---

## 13. Erreurs et reconnexion

### 13.1 Cas à gérer

- échec de chargement des certificats ;
- échec de construction des credentials ;
- échec de connexion broker ;
- déconnexion réseau ;
- subscribe refusé ;
- payload invalide ;
- JSON partiel ou inconnu.

### 13.2 Politique de reconnexion

- backoff progressif ;
- pas de boucle agressive ;
- resouscription complète après reconnexion ;
- refresh HTTP si doute sur l’état.

### 13.3 État exposé à l’UI

Le store doit pouvoir exposer clairement :

- `Disconnected`
- `Connecting`
- `Connected`
- `Subscribed`
- `Degraded`
- `Reconnecting`

---

## 14. Intégration avec le reste de l’application

### 14.1 Répartition HTTP / MQTT

#### HTTP

- login cloud ;
- récupération snapshot ;
- liste imprimantes ;
- commandes utilisateur ;
- resynchronisation.

#### MQTT

- temps réel ;
- événements asynchrones ;
- suivi live ;
- télémétrie.

### 14.2 Source unique pour l’UI

L’UI doit lire le `PrinterRealtimeStore`.

Le module MQTT n’écrit jamais directement dans les composants QML.

### 14.3 Corrélation commande / retour

Le `sendOrder` reste HTTP.

Le MQTT sert à recevoir la conséquence réelle côté imprimante.

Le lien entre les deux doit être fait dans `OrderResponseTracker`.

---

## 15. Proposition d’organisation de fichiers

Cette structure est une proposition de travail pour Codex. Elle peut être adaptée au dépôt réel, mais la séparation des responsabilités doit être conservée.

### 15.1 Dossier recommandé

Exemple :

- `accloud/src/mqtt/`
  - `MqttCredentialBuilder.*`
  - `MqttTlsConfigFactory.*`
  - `MqttTopicFactory.*`
  - `MqttSessionManager.*`
  - `MqttMessageRouter.*`
  - `OrderResponseTracker.*`
  - `PrinterRealtimeStore.*`
  - `TelemetryObservationStore.*`

### 15.2 Ressources

- `accloud/resources/mqtt/tls/`
  - `anycubic_mqtt_tls_ca.crt`
  - `anycubic_mqtt_tls_client.crt`
  - `anycubic_mqtt_tls_client.key`

### 15.3 Tests

- `accloud/tests/mqtt/`
  - tests credentials ;
  - tests topic factory ;
  - tests parsing ;
  - tests routing ;
  - tests reconnexion.

---

## 16. Critères d’acceptation

### 16.1 Connexion

Le module est considéré valide si :

- la connexion broker aboutit ;
- l’état connecté est propagé correctement ;
- les certificats sont chargés sans bricolage externe.

### 16.2 Souscriptions

Le module est considéré valide si :

- les topics user sont souscrits ;
- les topics printer sont souscrits dynamiquement ;
- la resouscription fonctionne après reconnexion.

### 16.3 Messages

Le module est considéré valide si :

- un message reçu met à jour le store ;
- un message inconnu est capturé sans casse ;
- le topic et la signature sont traçables.

### 16.4 Intégration UI

Le module est considéré valide si :

- l’UI n’a aucun parsing MQTT brut ;
- les changements live remontent dans le store ;
- un print en cours met à jour la vue sans refresh manuel.

---

## 17. Pièges à éviter

- coder les topics en dur à plusieurs endroits ;
- loguer les secrets ;
- parser le MQTT dans QML ;
- mélanger HTTP et MQTT dans une seule classe ;
- interpréter trop tôt les payloads M7 ;
- supposer qu’un reconnect reconstruit l’état complet sans refresh HTTP ;
- multiplier plusieurs clients MQTT concurrents sans nécessité.

---

## 18. Décision finale pour Codex

Codex doit implémenter en priorité un module MQTT natif C++/Qt basé sur :

- **SLICER auth** ;
- **mTLS obligatoire** ;
- **broker `mqtt-universe.anycubic.com:8883`** ;
- **topics user + printer** ;
- **store temps réel + store discovery** ;
- **séparation stricte UI / réseau / domaine**.

Le but du premier jet n’est pas d’interpréter tout le protocole Anycubic.

Le but du premier jet est :

1. connecter de façon fiable ;
2. souscrire proprement ;
3. recevoir les messages ;
4. mettre à jour un store central ;
5. capturer le reste pour analyse ;
6. permettre l’évolution progressive du support M7.

---

## 19. État implémenté: onglet MQTT debug

L’onglet MQTT expose désormais un flux debug orienté topics, sans configuration manuelle de connexion.

### 19.1 Connexion et mode

- Le mode runtime affiché est `Slicer`.
- Le bouton `Connect` déclenche la connexion MQTT préparée (profil session + TLS + credentials runtime).
- Le bouton `Disconnect` stoppe la session et nettoie l’état de souscription affiché.

### 19.2 Visibilité des topics

- Une zone dédiée affiche les topics actuellement souscrits.
- Les opérations de souscription sont aussi tracées dans le raw stream (`[SUBSCRIBE] topic=...`).

### 19.3 Messages reçus

- Le raw stream affiche le topic explicite sur chaque entrée (`topic=... | payload=...`).
- Une vue `Messages by topic` permet de sélectionner un topic reçu et d’afficher son historique.
- Les payloads JSON des messages topic sont formatés en multi-lignes (indentation) pour faciliter l’analyse.

### 19.4 Filtrage

- Un filtre par topic est disponible sur le raw stream.
- Le filtre conserve les blocs JSON multi-lignes d’un message topic, sans perdre les lignes du payload.

### 19.5 Nettoyage UI

- La carte `Raw Connection` a été supprimée de l’onglet MQTT.
- Le toggle `Extended debug` a été supprimé.
- La carte MQTT runtime du footer de la fenêtre principale a été retirée.
