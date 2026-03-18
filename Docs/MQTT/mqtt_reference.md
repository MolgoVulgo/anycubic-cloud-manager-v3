# MQTT — référence unifiée

**Statut : base de référence opérationnelle**

Ce document fusionne :

- la ligne **repo-first** ;
- le **CDC MQTT** ;
- la **source de vérité MQTT** ;
- la **synthèse de workflow MQTT d’impression** issue des captures réelles.

Il devient le **tronc principal** pour le périmètre MQTT du projet.

---

# 1. Règle de vérité

## 1.1 Ordre de priorité

1. **Le dépôt courant** reste la vérité primaire.
2. **Le présent document** est la vue consolidée à utiliser pour coder, relire et arbitrer.
3. **Les captures et analyses réelles** servent à confirmer, corriger ou nuancer le comportement runtime.
4. **Les anciens documents MQTT** ne doivent plus être lus comme contrat principal autonome.

## 1.2 Conséquence directe

Quand il y a divergence :

- le **code réel** gagne ;
- à défaut, la **présente référence** gagne ;
- les docs historiques complètent, mais ne pilotent plus seules le projet.

---

# 2. Objet

Définir une vue cohérente, unique et directement exploitable du sous-système MQTT pour `anycubic-cloud-manager-v3`, avec une séparation claire entre :

- le **transport et l’authentification** ;
- le **modèle de topics** ;
- le **routing** ;
- le **store temps réel** ;
- la **corrélation HTTP / MQTT** ;
- la **lecture métier des payloads d’impression observés**.

Ce document couvre à la fois les imprimantes résine de la marque Anycubic, et plus précisément :

- la structure cible du module MQTT ;
- les règles d’intégration C++ / Qt ;
- la lecture métier des topics réellement observés sur imprimantes résine.

---

# 3. Positionnement architectural

## 3.1 Rôle exact de MQTT

MQTT n’est **pas** le canal principal de pilotage métier.

Le modèle à retenir est :

- **HTTP** = authentification cloud, lecture catalogue, commandes explicites, opérations cloud classiques ;
- **MQTT** = temps réel imprimante, télémétrie, événements, retours asynchrones, état live, réponses différées à certaines commandes.

## 3.2 Conséquence d’architecture

Il ne faut pas :

- faire piloter l’imprimante directement depuis l’UI via MQTT en v1 ;
- laisser l’UI parser les topics ou les payloads ;
- mélanger sans règle l’état REST et l’état temps réel.

## 3.3 Flux cible

```text
Session cloud valide
-> génération des credentials MQTT
-> chargement TLS / mTLS
-> connexion broker
-> plan de souscriptions
-> réception messages
-> parsing / normalisation / routing
-> mise à jour store temps réel
-> exposition contrôlée vers l’application et l’UI
```

## 3.4 Réparation après perte de synchro

```text
reconnexion MQTT
-> resouscription complète
-> refresh REST complet
-> consolidation store
-> reprise du flux live
```

MQTT donne l’instantané temps réel. HTTP reste la source de resynchronisation complète.

---

# 4. Périmètre retenu

## 4.1 Inclus

- connexion MQTT temps réel pour imprimantes résine de la marque Anycubic ;
- authentification MQTT **SLICER** ;
- transport **TLS / mTLS** ;
- souscriptions imprimante et souscriptions serveur utiles au live ;
- parseur, routeur et store temps réel ;
- corrélation HTTP / MQTT ;
- mode discovery / observabilité ;
- gestion des messages réels observés pour :
  - `status/report`
  - `print/report`
  - `releaseFilm/report`
  - `autoOperation/report`
  - `wifi/report`

## 4.2 Hors périmètre nominal v1

- pilotage métier principal via publication MQTT ;
- couverture complète de tous les payloads historiques non validés ;
- dépendance directe QML ↔ MQTT ;
- interprétation métier exhaustive de tous les types wire non encore confirmés.

---

# 5. Connexion MQTT normative

## 5.1 Broker

Le broker de référence est :

```text
host = mqtt-universe.anycubic.com
port = 8883
mqtt = 3.1.1
transport = TLS
clean_session = true
keepalive = 1200
qos v1 = 0
retain publish = false
```

## 5.2 Session

Règles normatives :

- session locale non persistante ;
- credentials reconstruits à chaque nouvelle connexion ;
- resouscription complète après reconnexion ;
- refresh REST après reconnexion réussie ;
- aucun rattrapage d’historique supposé côté broker.

## 5.3 États runtime exposés

États minimaux à conserver :

- `Disconnected`
- `Connecting`
- `Connected`
- `Subscribed`
- `Degraded`
- `Reconnecting`

---

# 6. Authentification et TLS

## 6.1 Mode retenu

Le mode normatif du projet est **SLICER**.

Le mode ANDROID reste documentaire ou secondaire. Il ne pilote pas le runtime nominal.

## 6.2 Construction SLICER

Entrées :

- `email`
- `auth_token`

Construction :

```text
client_id = md5(email + "pcf")
mqtt_token = Base64(RSA_PKCS1_v1_5(UTF8(auth_token), mqtt_public_key))
signature = md5(client_id + mqtt_token + client_id)
username = "user|pcf|" + email + "|" + signature
password = mqtt_token
```

## 6.3 Contraintes crypto

- encodage texte : UTF-8 ;
- MD5 : hex lowercase ;
- padding RSA : PKCS#1 v1.5 ;
- email utilisé tel quel ;
- aucun secret brut ne remonte à l’UI.

## 6.4 TLS / mTLS

La connexion valide attend :

- une **CA** ;
- un **certificat client** ;
- une **clé privée client**.

Fichiers attendus dans le répertoire officiel du projet :

```text
accloud/resources/mqtt/tls/anycubic_mqtt_tls_ca.crt
accloud/resources/mqtt/tls/anycubic_mqtt_tls_client.crt
accloud/resources/mqtt/tls/anycubic_mqtt_tls_client.key
```

## 6.5 Compatibilité OpenSSL

Le projet doit conserver un mode de compatibilité TLS encapsulé dans la couche technique.

Ce mode doit :

- être explicite ;
- être activable ;
- ne jamais être dispersé dans l’application ;
- ne jamais masquer silencieusement une dégradation de sécurité.

---

# 7. Modèle de topics

## 7.1 Convention interne

Le code doit utiliser les noms internes suivants :

- `machineType` = type / famille imprimante ;
- `deviceKey` = identifiant topic de l’imprimante ;
- `userId` = identifiant cloud utilisateur.

## 7.2 Souscriptions nominales

Pour une imprimante donnée :

```text
anycubic/anycubicCloud/v1/printer/public/<machineType>/<deviceKey>/#
anycubic/anycubicCloud/v1/server/printer/<machineType>/<deviceKey>/#
```

## 7.3 Souscriptions discovery / compatibilité

En mode élargi uniquement :

```text
anycubic/anycubicCloud/v1/+/printer/<machineType>/<deviceKey>/#
anycubic/anycubicCloud/+/printer/<machineType>/<deviceKey>/#
anycubic/anycubicCloud/printer/public/<machineType>/<deviceKey>/online/status
```

## 7.4 Règles de conservation des variantes de topics

Attention aux variantes suivantes :

```text
anycubic/anycubicCloud/+/printer/<machineType>/<deviceKey>/#
anycubic/anycubicCloud/printer/public/<machineType>/<deviceKey>/#
```

Sur le périmètre observé, ces deux familles remontent les mêmes informations utiles côté application.

Lecture recommandée :

- `anycubic/anycubicCloud/+/printer/...` est très probablement une variante plus générale permettant aussi à l’imprimante d’émettre via ce canal ;
- pour l’application, il faut conserver le canal explicite et stable :
  - `anycubic/anycubicCloud/printer/public/...`

Décision recommandée :

- conserver `anycubic/anycubicCloud/printer/public/...` ;
- ne pas retenir `anycubic/anycubicCloud/+/printer/...` comme canal principal si les deux doublonnent.

## 7.5 Règle issue des captures réelles

L’abonnement suivant a aussi été identifié comme **redondant** sur les imprimantes concernées :

```text
anycubic/anycubicCloud/v1/+/public/<machineType>/<deviceKey>/#
```

Il provoque une **duplication quasi parfaite** des messages quand il coexiste avec :

```text
anycubic/anycubicCloud/v1/printer/public/<machineType>/<deviceKey>/#
```

Décision recommandée :

- **supprimer l’abonnement redondant** ;
- conserver le pattern nominal `v1/printer/public/.../#`.

---

# 8. Topics réellement importants

Tous les topics suivants sont importants et doivent être pris en compte :

- `.../status/report`
- `.../print/report`
- `.../releaseFilm/report`
- `.../autoOperation/report`
- `.../wifi/report`

Le piège n’est pas leur importance, mais leur **rôle sémantique différent**.

## 8.1 Répartition des responsabilités

### `status/report`

Rôle :

- état global machine ;
- heartbeat simple ;
- disponibilité générale.

### `print/report`

Rôle :

- cœur du workflow impression ;
- progression ;
- couches ;
- temps ;
- paramètres d’impression ;
- transitions start / pause / resume / finish / stop.

### `releaseFilm/report`

Rôle :

- report spécifique `releaseFilm` ;
- compteur ou télémétrie machine observée ;
- état séparé du sous-système concerné.

### `autoOperation/report`

Rôle :

- statuts automatiques internes ;
- sous-états machine ;
- séquence de préconditions et de contrôles.

### `wifi/report`

Rôle :

- état / métrique réseau ;
- diagnostic connectivité.

## 8.2 Conséquence de modélisation

Tous les topics doivent être :

- parsés ;
- normalisés ;
- stockés ou agrégés ;
- exploitables côté application.

Mais ils ne pilotent pas tous le même niveau d’état :

- **état principal du job** : surtout `print/report`
- **état global machine** : surtout `status/report`
- **sous-états de contrôle** : surtout `autoOperation/report`
- **télémétrie / compteurs machine** : `releaseFilm/report`
- **santé réseau** : `wifi/report`

---

# 9. Pipeline de parsing et de routing

## 9.1 Pipeline cible

```text
topic brut
-> parsing topic
-> classification famille / channel / printer
-> parsing JSON
-> validation envelope
-> normalisation interne
-> routing handler connu ou discovery
-> mise à jour store temps réel
```

## 9.2 Envelope minimale

Les payloads observés utilisent au minimum :

- `type`
- `action`

et souvent :

- `state`
- `data`
- `msg`
- `timestamp`
- `msgid`
- `code`

## 9.3 Règle d’inconnu

Un topic inconnu ou un payload inconnu ne doit jamais :

- fermer la session MQTT ;
- casser le store ;
- bloquer les messages déjà supportés.

Comportement attendu :

- log structuré ;
- compteur ;
- stockage discovery si activé ;
- promotion ultérieure vers un handler dédié.

---

# 10. Store temps réel et exposition applicative

## 10.1 Store central

Le store temps réel consolidé est la source interne de :

- disponibilité imprimante ;
- état live ;
- progression ;
- overlay temps réel ;
- télémétrie utile à l’UI.

## 10.2 Règles d’exposition

Interdictions :

- UI → broker ;
- UI → payload MQTT brut ;
- QML → parsing topic ;
- bridge UI → logique de corrélation HTTP / MQTT.

Le rôle de l’UI est de **consommer un état consolidé**.

## 10.3 Principe de priorité d’état

- **MQTT** prévaut pour l’instantané temps réel ;
- **HTTP** prévaut pour la resynchronisation complète ;
- en cas de dérive persistante : refresh REST complet.

---

# 11. Corrélation HTTP / MQTT

## 11.1 Principe

Les commandes partent en HTTP.
MQTT sert à récupérer :

- un retour corrélé par `msgid` ;
- ou un changement d’état cohérent avec la commande en cours.

## 11.2 Clé primaire

```text
msgid
```

## 11.3 Fallback strict

Clé logique de fallback :

```text
printer_id + correlation_class
```

Règles :

- pas d’auto-matching large ;
- exactement un candidat ouvert ;
- une seule commande en vol par clé de fallback si nécessaire ;
- gestion explicite des doublons, hors ordre et timeouts.

## 11.4 Timeouts recommandés

- commande simple : 10 s
- contrôle standard : 15 s
- start / pause / resume / stop : 30 s
- opération lourde : 60 s

---

# 12. Machine d’états observée sur les captures réelles

Les captures brutes apportent une lecture runtime plus précise que les documents seuls.

## 12.1 Workflow nominal d’impression

La séquence observée est :

1. `status = free`
2. ordre d’impression accepté
3. `status = busy`
4. `print/update` avec `state = downloading`
5. `print/start` avec `state = printing`
6. `print/start` avec `state = preheating`
7. `print/start` avec `state = printing`
8. progression réelle de couches
9. `print/start` avec `state = finished`
10. `status = free`
11. mise à jour `releaseFilm/report`

## 12.2 Lecture fonctionnelle par topic

### `status/report`

Exemple utile :

```json
{
  "type": "status",
  "action": "workReport",
  "state": "busy",
  "data": null
}
```

Lecture :

- `free` = disponible ;
- `busy` = occupée.

### `print/report`

C’est le topic principal du job.

Exemple réduit :

```json
{
  "type": "print",
  "action": "start",
  "state": "printing",
  "code": 200,
  "data": {
    "filename": "butterfly.pwsz",
    "curr_layer": 814,
    "total_layers": 1928,
    "progress": 42,
    "remain_time": 5367
  }
}
```

États observés utiles :

- `downloading`
- `monitoring`
- `preheating`
- `printing`
- `pausing`
- `paused`
- `resuming`
- `resumed`
- `finished`
- `stoped`

### `releaseFilm/report`

Exemple réduit :

```json
{
  "type": "releaseFilm",
  "action": "get",
  "state": "done",
  "data": {
    "layers": 23636,
    "status": 0,
    "times": 20
  }
}
```

Lecture observée :

- `times` ressemble à un compteur de cycle / tentative ;
- `layers` ressemble à un compteur cumulé de couches traitées.

### `autoOperation/report`

Exemple réduit :

```json
{
  "type": "print",
  "action": "autoOperation",
  "state": "monitoring",
  "data": {
    "checkStatus": [
      { "name": "platform", "status": 1206 },
      { "name": "residual", "status": -1 },
      { "name": "resin", "status": -1 },
      { "name": "levelling", "status": -1 }
    ]
  }
}
```

Lecture :

- expose les sous-statuts de contrôle ;
- utile pour expliquer les blocages et les sorties de blocage.

### `wifi/report`

Exemple réduit :

```json
{
  "type": "wifi",
  "action": "getSignalStrength",
  "state": "done",
  "data": {
    "signal_strength": 55
  }
}
```

Lecture :

- télémétrie réseau ;
- utile pour diagnostic.

---

# 13. Lecture métier des défauts et blocages

## 13.1 Défaut plateau observé

Une capture dédiée montre la séquence suivante :

- `platform = 1206`
- puis `print/start` avec `code = 1306`, `state = waiting`
- puis arrêt manuel avec `print/stop`, `code = 601`, `state = stoped`
- puis `status = free`

Lecture correcte :

- `platform = 1206` = anomalie ou condition plateau ;
- `1306 / waiting` = job bloqué en attente ;
- `601 / stoped` = arrêt manuel du job dans ce scénario précis.

## 13.2 Point de prudence

`platform = 1206` ne doit pas être traité comme un hard-stop fatal isolé.

Une autre capture montre que le print peut repartir alors que `platform = 1206` reste présent dans les reports.

## 13.3 Contrôles réellement bloquants observés

Dans la capture avec validation progressive, la sortie du blocage suit plutôt l’évolution :

- `residual : -1 -> 0`
- `resin : -1 -> 0`
- `levelling : -1 -> 0`

puis le print repart réellement.

Lecture recommandée :

- le blocage doit être interprété sur **combinaison de statuts** ;
- pas sur un seul code persistant.

## 13.4 Conséquence de parsing

Il faut remonter explicitement :

- `print.action`
- `print.code`
- `print.state`
- `print.data.checkStatus[]`

et conserver l’historique d’évolution des sous-statuts.

---

# 14. Règles métier consolidées

## 14.1 États principaux à exposer

États UI / métier recommandés :

- `Available`
- `Busy`
- `Downloading`
- `Preheating`
- `Printing`
- `Paused`
- `Resuming`
- `Finished`
- `Waiting`
- `Stopped`
- `Degraded`

## 14.2 Sous-états et télémétrie à conserver

- `platform_status`
- `residual_status`
- `resin_status`
- `levelling_status`
- `wifi_signal_strength`
- `release_film_layers_total`
- `release_film_times_total`

## 14.3 Règle importante

Tous les topics observés sont importants, mais l’état principal du job doit être dérivé de plusieurs dimensions :

- `print/report` pour la progression et les transitions de job ;
- `status/report` pour la disponibilité globale ;
- `autoOperation/report` pour les causes et préconditions ;
- `releaseFilm/report` pour la télémétrie cumulée ;
- `wifi/report` pour la couche réseau.

---

# 15. Découpage logiciel recommandé

## 15.1 Couche transport

- `MqttTlsConfig` / `TlsMaterialProvider`
- `MqttCredentialProvider`
- `MqttClientAdapter` / `MqttSessionManager`

## 15.2 Couche technique MQTT

- `MqttSubscriptionPlanner`
- `MqttTopicBuilder`
- `MqttTopicParser`
- `MqttMessageRouter`
- `MqttDiscoverySink`
- `MqttTelemetry`

## 15.3 Couche application

- `PrinterRealtimeStore`
- `ApplyRealtimeOverlayUseCase`
- `OrderResponseTracker`
- `ResyncCloudStateUseCase`
- `MqttBridge`
- `CloudBridge`

## 15.4 Frontière stricte

- transport / crypto / TLS dans l’infra ;
- normalisation et routing dans la couche technique MQTT ;
- store et overlays dans l’application ;
- UI en lecture seule via bridges propres.

---

# 16. Discovery et observabilité

Le mode discovery existe pour absorber les payloads encore partiellement couverts sans bloquer l’intégration.

Il doit permettre de conserver au minimum :

- signature message ;
- topic ;
- `deviceKey` ;
- payload redacté ;
- fréquence ;
- dernière occurrence ;
- décision prise par le routeur.

Le discovery ne remplace pas le flux métier. Il complète la couverture protocolaire.

---

# 17. Journalisation et sécurité

## 17.1 À journaliser

- ouverture / fermeture connexion ;
- souscriptions ;
- reconnexions ;
- erreurs de parsing JSON ;
- messages inconnus ;
- transitions majeures ;
- doublons ;
- timeouts.

## 17.2 À masquer

- token cloud ;
- mot de passe MQTT ;
- username complet ;
- clé privée ;
- `deviceKey` complet si le niveau de logs l’exige.

---

# 18. Tests et garde-fous

Le socle MQTT doit être gardé sur :

- génération des credentials ;
- compatibilité TLS ;
- construction topics ;
- parsing envelope ;
- routing ;
- messages inconnus ;
- corrélation HTTP / MQTT ;
- redaction des secrets ;
- non-régression sur les topics réellement utilisés ;
- absence de duplication de flux après suppression de l’abonnement redondant.

---

# 19. Décision finale

La stratégie retenue est :

1. **SLICER + mTLS** comme base de connexion ;
2. **HTTP pour les commandes**, **MQTT pour le temps réel et les retours asynchrones** ;
3. **store central** comme point d’exposition applicatif ;
4. **routing multi-topics** intégrant réellement :
   - `status/report`
   - `print/report`
   - `releaseFilm/report`
   - `autoOperation/report`
   - `wifi/report`
5. **suppression de l’abonnement redondant** qui duplique le trafic ;
6. **conservation de `anycubic/anycubicCloud/printer/public/...`** comme variante explicite à privilégier quand elle doublonne avec `anycubic/anycubicCloud/+/printer/...` ;
7. **lecture métier fondée sur les transitions réelles observées**, pas sur des suppositions mono-code.

Ce document doit être utilisé comme référence de travail principale pour toute évolution MQTT du projet.

