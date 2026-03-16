# Connexion au broker

## 1. Objet

Ce document décrit la connexion au broker MQTT Anycubic et les deux constructions d’authentification utiles dans le projet :

- mode **SLICER** ;
- mode **ANDROID**.

L’objectif est de disposer d’une base claire pour implémenter la connexion MQTT dans l’application, sans mélanger cette logique avec le domaine métier.

---

## 2. Paramètres du broker

La connexion MQTT se fait avec les paramètres suivants :

- **Host** : `mqtt-universe.anycubic.com`
- **Port** : `8883`
- **Transport** : `TLS`
- **Protocole MQTT** : `3.1.1`
- **Clean session** : `true`
- **Keepalive** : `1200`
- **QoS de souscription** : `0`
- **Retain** : `false`

### Conséquences techniques

- la session MQTT n’est pas persistée ;
- après reconnexion, il faut **resouscrire tous les topics** ;
- en cas de coupure, il faut prévoir un **refresh HTTP complet** pour resynchroniser l’état ;
- avec `QoS 0`, il ne faut pas supposer que tous les messages intermédiaires seront récupérés après perte réseau.

---

## 3. Pré-requis

Avant d’ouvrir une connexion MQTT, il faut disposer d’une session cloud valide contenant au minimum :

- `email`
- `auth_token`
- `user_id`
- `mode_auth`

Selon le mode, il faut aussi :

- pour **SLICER** : la clé publique / certificat utilisé pour chiffrer le token MQTT ;
- pour **ANDROID** : le support de calcul `bcrypt`.

---

## 4. Structure générale de l’authentification

Quel que soit le mode, la connexion repose sur trois éléments calculés :

- `client_id`
- `username`
- `password`

Le `username` a toujours la forme :

`user|<mqtt_app_id>|<email>|<signature_md5>`

avec :

`signature_md5 = md5(client_id + mqtt_token + client_id)`

Le `password` transmis à MQTT est le `mqtt_token` calculé selon le mode.

---

## 5. Construction SLICER

### 5.1 Identifiant applicatif

Pour le mode SLICER :

- `mqtt_app_id = "pcf"`

### 5.2 Construction du `client_id`

Formule :

```text
client_id = md5(email + "pcf")
```

### 5.3 Construction du `mqtt_token`

Le token MQTT SLICER est construit ainsi :

1. prendre `auth_token` brut ;
2. l’encoder en UTF-8 ;
3. le chiffrer en **RSA PKCS#1 v1.5** avec la clé publique MQTT ;
4. encoder le résultat en **Base64 standard**.

Formule logique :

```text
mqtt_token = Base64( RSA_PKCS1_v1_5_Encrypt( UTF8(auth_token), mqtt_public_key ) )
```

### 5.4 Construction de la signature

```text
signature_md5 = md5(client_id + mqtt_token + client_id)
```

### 5.5 Construction du `username`

```text
username = "user|pcf|" + email + "|" + signature_md5
```

### 5.6 Construction du `password`

```text
password = mqtt_token
```

### 5.7 Résumé SLICER

```text
client_id  = md5(email + "pcf")
mqtt_token = Base64(RSA_PKCS1_v1_5(UTF8(auth_token)))
signature  = md5(client_id + mqtt_token + client_id)
username   = "user|pcf|" + email + "|" + signature
password   = mqtt_token
```

---

## 6. Construction ANDROID

### 6.1 Identifiant applicatif

Pour le mode ANDROID :

- `mqtt_app_id = "app"`

### 6.2 Construction du `client_id`

Formule :

```text
client_id = md5(email)
```

### 6.3 Construction du `mqtt_token`

Le token MQTT Android est construit ainsi :

1. calculer `md5(auth_token)` ;
2. passer le résultat dans **bcrypt**.

Formule logique :

```text
mqtt_token = bcrypt(md5(auth_token))
```

### 6.4 Construction de la signature

```text
signature_md5 = md5(client_id + mqtt_token + client_id)
```

### 6.5 Construction du `username`

```text
username = "user|app|" + email + "|" + signature_md5
```

### 6.6 Construction du `password`

```text
password = mqtt_token
```

### 6.7 Résumé ANDROID

```text
client_id  = md5(email)
mqtt_token = bcrypt(md5(auth_token))
signature  = md5(client_id + mqtt_token + client_id)
username   = "user|app|" + email + "|" + signature
password   = mqtt_token
```

---

## 7. Tableau comparatif

| Élément | SLICER | ANDROID |
|---|---|---|
| `mqtt_app_id` | `pcf` | `app` |
| `client_id` | `md5(email + "pcf")` | `md5(email)` |
| token MQTT | RSA PKCS#1 v1.5 + Base64 | bcrypt(md5(auth_token)) |
| `username` | `user|pcf|email|signature` | `user|app|email|signature` |
| `password` | `mqtt_token` | `mqtt_token` |

---

## 8. Séquence de connexion

### 8.1 Étapes générales

1. récupérer la session cloud ;
2. déterminer le mode d’authentification ;
3. calculer `client_id` ;
4. calculer `mqtt_token` ;
5. calculer `signature_md5` ;
6. construire `username` ;
7. initialiser le client MQTT ;
8. ouvrir la connexion TLS ;
9. souscrire les topics utilisateur ;
10. souscrire les topics imprimante.

### 8.2 Pseudo-flux

```text
load session
 -> select mode
 -> build client_id
 -> build mqtt_token
 -> build signature
 -> build username
 -> connect TLS to mqtt-universe.anycubic.com:8883
 -> subscribe user topics
 -> subscribe printer topics
```

---

## 9. Topics à souscrire

### 9.1 Topics utilisateur

```text
anycubic/anycubicCloud/v1/server/app/<user_id>/<user_id_md5>/slice/report
anycubic/anycubicCloud/v1/server/app/<user_id>/<user_id_md5>/fdmslice/report
```

### 9.2 Topics imprimante

Souscription recommandée par imprimante :

```text
anycubic/anycubicCloud/v1/printer/public/<machine_type>/<printer_key>/#
```

Cette souscription permet de recevoir tous les canaux utiles d’une imprimante donnée.

### 9.3 Canaux déjà observés

Sur les imprimantes résine récentes, plusieurs familles ont déjà été observées :

- `print/report`
- `status/report`
- `file/report`
- `releaseFilm/report`
- `wifi/report`
- `user/report`
- `ota/report`
- `lastWill/report`

---

## 10. Reconnexion

### 10.1 Règles

En cas de coupure ou de reconnexion :

1. reconnecter le client MQTT ;
2. recalculer les credentials si nécessaire ;
3. resouscrire les topics utilisateur ;
4. resouscrire les topics imprimante ;
5. déclencher un snapshot HTTP si la cohérence de l’état est incertaine.

### 10.2 Point critique

Comme la session est non persistante et que le QoS est faible, il ne faut jamais considérer qu’une reconnexion suffit à reconstituer l’état métier.

Le MQTT donne le temps réel.

Le HTTP sert à réparer l’état global après perte de synchro.

---

## 11. Découpage recommandé dans l’application

### 11.1 Composants à isoler

- `MqttCredentialBuilder`
- `MqttSessionManager`
- `MqttTopicFactory`
- `MqttMessageRouter`
- `OrderResponseTracker`

### 11.2 Répartition des responsabilités

#### `MqttCredentialBuilder`

Construit :

- `client_id`
- `username`
- `password`

#### `MqttSessionManager`

Gère :

- connexion
- reconnexion
- état du broker
- callbacks réseau

#### `MqttTopicFactory`

Construit :

- topics user
- topics printer

#### `MqttMessageRouter`

Route :

- messages connus vers le store métier ;
- messages inconnus vers le store discovery.

#### `OrderResponseTracker`

Corrèle :

- `sendOrder` HTTP ;
- `msgid` ;
- réponse MQTT.

---

## 12. Points d’attention

### 12.1 Ne pas mélanger les couches

- l’UI ne doit pas construire les credentials ;
- l’UI ne doit pas connaître les topics ;
- l’UI ne doit pas parser les payloads MQTT bruts.

### 12.2 Secrets

Les logs officiels peuvent contenir :

- tokens ;
- emails ;
- user IDs ;
- clés machine.

Ils doivent être nettoyés avant partage.

### 12.3 Validation progressive

Les constructions décrites ici servent à établir la connexion broker.

La cartographie complète des messages métier doit ensuite être validée séparément par capture et analyse des payloads réels.

---

## 13. Conclusion

La connexion au broker Anycubic repose sur une base stable :

- broker TLS sur `mqtt-universe.anycubic.com:8883` ;
- credentials calculés différemment selon les modes **SLICER** et **ANDROID** ;
- souscriptions user + printer ;
- reconnexion avec resouscription ;
- resynchronisation HTTP en cas de doute.

Le point clé est de garder cette logique de connexion dans une couche technique dédiée, indépendante du domaine métier et de l’interface.

