# Anycubic 1.1.27 - Inventaire des topics MQTT

Date: 2026-03-13  
Source APK decompilee: `~/apk/Anycubic_1.1.27_apkcombo.com`

## 1) Topics litteraux (definis en ressources)

1. `anycubic/anycubicCloud/v1/+/public/`
2. `anycubic/anycubicCloud/v1/printer/app/`
3. `anycubic/anycubicCloud/v1/app/`
4. `anycubic/anycubicCloud/v1/server/app/`

Preuve:
- `~/apk/Anycubic_1.1.27_apkcombo.com/res/values/strings.xml:2570`
- `~/apk/Anycubic_1.1.27_apkcombo.com/res/values/strings.xml:2571`
- `~/apk/Anycubic_1.1.27_apkcombo.com/res/values/strings.xml:2572`
- `~/apk/Anycubic_1.1.27_apkcombo.com/res/values/strings.xml:2573`

## 2) Topics dynamiques derives du code

1. `anycubic/anycubicCloud/v1/server/app/{userId}/{md5Lower(userId)}/slice/report`
2. `anycubic/anycubicCloud/v1/server/app/{userId}/{md5Lower(userId)}/fdmslice/report`
3. `anycubic/anycubicCloud/v1/printer/app/{machineType}/{deviceKey}/#`
4. `anycubic/anycubicCloud/v1/+/public/{machineType}/{deviceKey}/#`
5. `anycubic/anycubicCloud/v1/app/public/{machineType}/{deviceId}/video/report`
6. `anycubic/anycubicCloud/v1/app/{src}/{deviceType}/{deviceId}/response`

Preuve:
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/fragment/WorkbenchFragment.smali:2550`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/fragment/WorkbenchFragment.smali:2628`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/fragment/WorkbenchFragment.smali:2707`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/fragment/WorkbenchFragment.smali:2769`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/fragment/WorkbenchFragment.smali:2888`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/fragment/WorkbenchFragment.smali:2916`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/fragment/WorkbenchFragment.smali:2981`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/anycubic/lib/video/agora/AgoraConnectionClient$mUpInfoTopic$2.smali:76`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/anycubic/lib/video/agora/AgoraConnectionClient$mUpInfoTopic$2.smali:82`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/anycubic/lib/video/agora/AgoraConnectionClient$mUpInfoTopic$2.smali:106`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/internal/Connection.smali:502`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/internal/Connection.smali:519`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/internal/Connection.smali:541`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/internal/Connection.smali:559`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/internal/Connection.smali:572`

## 3) Gabarit de parsing des topics recus (TopicUtils)

`TopicUtils` lit les segments comme suit:

- index 1: `projectName`
- index 3: `src`
- index 4: `dst`
- index 5: `deviceType`
- index 6: `deviceId`
- index 7: `commandType`

Gabarit implicite:

`{root}/{projectName}/{version?}/{src}/{dst}/{deviceType}/{deviceId}/{commandType}`

Preuve:
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/util/TopicUtils.smali:27`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/util/TopicUtils.smali:223`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/util/TopicUtils.smali:288`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/util/TopicUtils.smali:377`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/util/TopicUtils.smali:463`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/util/TopicUtils.smali:548`

## 4) Gabarit de publication via SendMqttMsg.Builder (a surveiller)

Topic construit:

`anycubic/{projectName}/{src}/{dst}/{modelId}/{deviceId}/{type}`

Valeurs par defaut dans le builder:
- `projectName = anycubicCloud`
- `src = android`

Preuve:
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/util/SendMqttMsg$Builder.smali:132`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/util/SendMqttMsg$Builder.smali:140`
- `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/util/SendMqttMsg$Builder.smali:538`

Note:
- Aucune occurrence d'usage directe de `SendMqttMsg$Builder` n'a ete retrouvee via recherche texte globale sur l'APK decompile (possible code peu/plus utilise, ou appel indirect).

## 5) Dedoublonnage final (topics/patterns uniques)

1. `anycubic/anycubicCloud/v1/+/public/`
2. `anycubic/anycubicCloud/v1/printer/app/`
3. `anycubic/anycubicCloud/v1/app/`
4. `anycubic/anycubicCloud/v1/server/app/`
5. `anycubic/anycubicCloud/v1/server/app/{userId}/{md5Lower(userId)}/slice/report`
6. `anycubic/anycubicCloud/v1/server/app/{userId}/{md5Lower(userId)}/fdmslice/report`
7. `anycubic/anycubicCloud/v1/printer/app/{machineType}/{deviceKey}/#`
8. `anycubic/anycubicCloud/v1/+/public/{machineType}/{deviceKey}/#`
9. `anycubic/anycubicCloud/v1/app/public/{machineType}/{deviceId}/video/report`
10. `anycubic/anycubicCloud/v1/app/{src}/{deviceType}/{deviceId}/response`
11. `anycubic/{projectName}/{src}/{dst}/{modelId}/{deviceId}/{type}` (builder generique)

### Correspondances APK (a quoi servent ces topics)

| Topic/pattern | Sens | Usage observe dans l'APK | Lien HTTP possible |
|---|---|---|---|
| `anycubic/anycubicCloud/v1/+/public/` | Prefix subscribe | Base `TOPIC_PLUS`, utilisee pour construire `.../{machineType}/{deviceKey}/#` (events publics par machine) | Peut porter les retours async de commandes lancees via HTTP |
| `anycubic/anycubicCloud/v1/printer/app/` | Prefix subscribe | Base `TOPIC_PRINTER`, utilisee pour `.../{machineType}/{deviceKey}/#` (canal principal device -> app) | Oui, retours metier apres `sendOrder` |
| `anycubic/anycubicCloud/v1/app/` | Prefix publish | Base `TOPIC_PUBLISH` pour publier `.../response` (ack MQTT) et `.../public/{machineType}/{deviceId}/video/report` | Oui, canal de reponse/confirmation cote app |
| `anycubic/anycubicCloud/v1/server/app/` | Prefix subscribe | Base `TOPIC_SERVER`, utilisee pour les reports de slicing par utilisateur | Alimente des updates async de jobs demarres via API |
| `anycubic/anycubicCloud/v1/server/app/{userId}/{md5Lower(userId)}/slice/report` | Subscribe | Ajoute dynamiquement au login/workbench; suivi d'avancement/resultat slice | Lie aux flux HTTP de slicing (job lance en HTTP, suivi en MQTT) |
| `anycubic/anycubicCloud/v1/server/app/{userId}/{md5Lower(userId)}/fdmslice/report` | Subscribe | Meme logique que `slice/report` pour FDM slice | Idem |
| `anycubic/anycubicCloud/v1/printer/app/{machineType}/{deviceKey}/#` | Subscribe | Recoit la majorite des messages imprimante. Parsing topic -> `commandType/deviceType/deviceId`, puis routage vers `MqttMessageDispenseExt` (`type/action`) | Oui, principal retour async apres `api/work/operation/sendOrder` |
| `anycubic/anycubicCloud/v1/+/public/{machineType}/{deviceKey}/#` | Subscribe | Canal wildcard public par machine/device, route dans le meme pipeline MQTT | Oui, peut aussi porter des reponses async |
| `anycubic/anycubicCloud/v1/app/public/{machineType}/{deviceId}/video/report` | Publish | Topic de reporting video (Agora/peer) construit cote app | Peut etre declenche apres actions/API video |
| `anycubic/anycubicCloud/v1/app/{src}/{deviceType}/{deviceId}/response` | Publish | Ack auto genere en reception MQTT (payload = `msgid`), sauf types filtres par `getMqttFilter` | Oui, reponse MQTT pilotee par une config chargee en HTTP |
| `anycubic/{projectName}/{src}/{dst}/{modelId}/{deviceId}/{type}` | Publish (generique) | Pattern du `SendMqttMsg.Builder`; pas d'usage direct retrouve par recherche texte globale | Non confirme dans cette version |

### Correlation HTTP <-> MQTT (confirmee dans l'APK)

1. Le filtre HTTP `GET api/work/index/getMqttFilter` est charge puis stocke dans `MqttServiceConstants.MQTT_TYPE_LIST`.
2. A la reception MQTT, `Connection.dispenseMessageByDeviceId(...)` compare `commandType` a cette liste.
3. Si `commandType` est filtre, pas d'ack auto; sinon publication d'un ack sur `.../response` avec `msgid`.
4. Les commandes utilisateur passent par `sendMqttOrder(...)` -> `SendCommandViewModel` -> `SendOrderRepository` -> `POST api/work/operation/sendOrder`.
5. En succes HTTP, un timeout MQTT est arme (`startMqttMsgCountDown`).
6. A la reception MQTT correspondante, le routeur `h/b.f(...)` annule le timeout et poste l'event UI (`FlowBus.postAppKeyEvent`).

### Preuves (fichiers clefs)

- Initialisation des bases topic depuis les ressources:
  - `~/apk/Anycubic_1.1.27_apkcombo.com/res/values/strings.xml:2570`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/res/values/strings.xml:2573`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/e/a.smali:130`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/e/a.smali:193`
- Construction des subscriptions dynamiques:
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/fragment/WorkbenchFragment.smali:2550`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/fragment/WorkbenchFragment.smali:2628`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/fragment/WorkbenchFragment.smali:2769`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/fragment/WorkbenchFragment.smali:2888`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/fragment/WorkbenchFragment.smali:2981`
- Topic video report:
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/anycubic/lib/video/agora/AgoraConnectionClient$mUpInfoTopic$2.smali:106`
- Dispatch MQTT et ack `/response`:
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/internal/Connection.smali:204`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/internal/Connection.smali:236`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/internal/Connection.smali:357`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/internal/Connection.smali:366`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/internal/Connection.smali:572`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/internal/Connection.smali:748`
- Chaine de routage metier:
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes3/ac/cloud/reposervice/service/a.smali:201`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/ac/cloud/common/CommonServiceImpl.smali:91`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/h/b.smali:1093`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/h/b.smali:3280`
- Types/actions MQTT reconnus (extraits):
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/h/b.smali:1196` (`tempature`)
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/h/b.smali:1706` (`video`)
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/h/b.smali:1815` (`print`)
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/h/b.smali:2725` (`fdmslice`)
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/h/b.smali:2804` (`aiSettings`)
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/ac/cloud/common/ext/r.smali:1`
- Lien HTTP filtre MQTT:
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/z/a.smali:154`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/z/a.smali:169`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/viewmodel/a$k.smali:219`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes4/ac/cloud/workbench/main/viewmodel/WorkbenchViewModel$n.smali:128`
- Lien HTTP envoi commande + attente MQTT:
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/ac/cloud/common/base/BaseMqttHandleActivity.smali:5345`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/ac/cloud/common/base/BaseMqttHandleActivity.smali:5353`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/ac/cloud/common/base/BaseMqttHandleActivity.smali:5532`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/ac/cloud/common/base/BaseMqttHandleActivity.smali:5572`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali/ac/cloud/common/base/BaseMqttHandleActivity$sendMqttOrder$1$1.smali:141`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/apiservice/CommandApiService.smali:55`
  - `~/apk/Anycubic_1.1.27_apkcombo.com/smali_classes6/com/cloud/mqttservice/apiservice/SendOrderRepository$sendCommand$2.smali:232`
