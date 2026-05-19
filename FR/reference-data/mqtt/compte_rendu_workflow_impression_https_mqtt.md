# Compte rendu — Workflow complet impression Anycubic Cloud : HTTPS → MQTT

Objectif : fournir une base d’intégration exploitable pour créer le workflow complet d’une impression, depuis l’envoi de la commande HTTPS jusqu’au suivi MQTT des phases de préparation, d’impression et de fin.

Ce document s’appuie sur la consolidation des logs MQTT fournis et sur l’analyse des payloads applicatifs Anycubic.

## 1. Périmètre et consolidation des logs

| Élément | Valeur |
| --- | --- |
| Messages MQTT exploitables uniques | 28770 |
| Doublons supprimés | 27748 |
| Clé de déduplication | direction + topic + ts + canonical(payload) |
| Début capture | 2026-04-21T13:49:50.387 |
| Fin capture | 2026-04-30T13:22:18.084 |
| Direction observée | rx=28770 |

Les deux fichiers `mqtt_topic_capture*.jsonl` se recouvrent : le fichier `mqtt_topic_capture copy.jsonl` est un sous-ensemble exact de `mqtt_topic_capture.jsonl`. Les fichiers `mqtt.jsonl.1` et `mqtt.jsonl` servent surtout au diagnostic transport/app, mais ne portent pas les payloads applicatifs nécessaires à l’analyse des actions.

## 2. Principe d’architecture

Le workflow doit être séparé en deux plans stricts :

```text
HTTPS = commande / intention
MQTT  = observation / vérité d’état
```

La réponse HTTPS indique que la commande a été acceptée ou refusée. Elle ne prouve pas que l’impression a réellement démarré. La vérité opérationnelle vient des reports MQTT.

## 3. Structure JSON à parser

Dans le log brut, `payload` est une chaîne JSON. Il faut parser la ligne, puis parser `payload` une seconde fois.

```text
root
├── direction
├── topic
├── ts
├── payload_bytes
└── payload              // string JSON à parser
    ├── action
    ├── code
    ├── data
    ├── msg
    ├── msgid
    ├── state
    ├── timestamp
    └── type
```

Les champs structurants du payload applicatif sont :

```text
payload.type   = domaine fonctionnel
payload.action = événement / report
payload.state  = état courant
```

### 3.1 Combinaisons `type / action / state` observées

| type | action | state | occurrences |
| --- | --- | --- | --- |
| print | start | printing | 18997 |
| status | workReport | busy | 5513 |
| status | workReport | free | 1962 |
| releaseFilm | get | done | 1962 |
| print | autoOperation | monitoring | 131 |
| print | update | downloading | 62 |
| print | monitor | monitoring | 39 |
| autoOperation | reportStatus | done | 20 |
| wifi | getSignalStrength | done | 20 |
| print | start | preheating | 16 |
| print | start | finished | 11 |
| axis | move | done | 7 |
| lastWill | onlineReport | online | 6 |
| ota | reportVersion | report-success | 6 |
| user | bindQuery | done | 6 |
| file | cloudRecommendList | done | 6 |
| lastWill | onlineReport | offline | 5 |
| resin | feedResin | start | 1 |

## 4. Actions centrales du workflow impression

| Élément | Rôle workflow | Données utiles |
| --- | --- | --- |
| `workReport / status / free\|busy` | Disponibilité globale imprimante | `state=free\|busy`, `data=null` |
| `update / print / downloading` | Téléchargement du job par l’imprimante | `data.taskid`, `data.progress`, `data.task_mode` |
| `start / print / printing` | Report du job chargé ou en impression | `taskid`, `filename`, `curr_layer`, `total_layers`, `progress`, `remain_time` |
| `start / print / preheating` | Phase de préchauffe / préparation | `heating_skip_allowed`, `heating_remain_time`, `curr_layer=0` |
| `start / print / finished` | Fin de job si présent | `progress=100`, `remain_time=0` |
| `monitor / print / monitoring` | Checks matériels liés au job | `data.taskid`, `data.checkStatus` |
| `autoOperation / print / monitoring` | Contrôles automatiques liés au job | `data.taskid`, `data.checkStatus` |

## 5. Workflow nominal complet

```text
[0] MQTT subscribe avant commande
    -> écouter print/report, status/report, releaseFilm/report, autoOperation/report, wifi/report

[1] HTTPS print command
    -> la commande est acceptée ou refusée
    -> créer un job local command_sent

[2] MQTT update/downloading
    -> taskid connu
    -> download_progress 0..100
    -> filename encore inconnu

[3] MQTT start/printing curr_layer=0
    -> filename connu
    -> job chargé / loaded
    -> pas forcément impression physique

[4] MQTT monitor/monitoring
    -> checks matériels rattachés au taskid

[5] MQTT autoOperation/monitoring
    -> contrôles automatiques rattachés au taskid

[6] MQTT workReport/busy
    -> imprimante occupée

[7] MQTT start/preheating
    -> préchauffe / préparation active

[8] MQTT start/printing curr_layer>=1
    -> impression effective

[9] MQTT start/printing répétés
    -> progression d’impression

[10] MQTT start/finished
    -> job terminé si présent

[11] MQTT workReport/free
    -> imprimante libre
```

## 6. Machine d’état recommandée

```text
idle
  <- workReport/free

command_sent
  <- HTTPS print command accepted

downloading
  <- update/downloading

downloaded
  <- update/downloading progress=100

loaded
  <- start/printing curr_layer=0

checking
  <- monitor + autoOperation

preheating
  <- start/preheating

printing
  <- start/printing curr_layer>=1

finished
  <- start/finished

idle
  <- workReport/free
```

## 7. Modèle de données recommandé

```text
PrinterState
  printer_id: string
  availability: free | busy | unknown
  last_seen_ts: datetime

PrintJob
  taskid: string
  file_id: string | null
  filename: string | null
  stage:
    command_sent
    downloading
    downloaded
    loaded
    checking
    preheating
    printing
    finished
    interrupted_or_unknown
  download_progress: int | null
  print_progress: int | null
  curr_layer: int | null
  total_layers: int | null
  print_time: int | null
  remain_time: int | null
  heating_skip_allowed: bool | null
  heating_remaining_time: int | null
  task_mode: int | null
  slicer: string | null
  settings: object | null
  hardware_checks: map<string,int>
  auto_checks: map<string,int>
  last_msgid: string | null
  last_seen_ts: datetime
```

## 8. Règles de corrélation

```text
clé principale du job = payload.data.taskid
filename = payload.data.filename reçu via start, pas via update
workReport ne contient pas de taskid
start est répétitif : ne pas créer un nouveau job à chaque start
update.progress=100 = téléchargement terminé, pas impression terminée
start/printing curr_layer=0 = job chargé
start/printing curr_layer>=1 = impression effective
start/finished peut manquer : prévoir interrupted_or_unknown
```

## 9. Détails par phase

### 9.1 Commande HTTPS

La commande HTTPS doit être envoyée uniquement après initialisation de l’écoute MQTT. La réponse HTTPS doit être traitée comme une acceptation de commande, pas comme un état d’impression.

```text
HTTPS response utile si disponible :
- printer_id
- file_id
- filename
- taskid éventuel
- request_id / op_id

Si taskid absent de la réponse HTTPS :
- attendre update.data.taskid ou start.data.taskid côté MQTT
```

### 9.2 Téléchargement `update/downloading`

```text
payload.type   = print
payload.action = update
payload.state  = downloading
payload.data.taskid
payload.data.progress
payload.data.task_mode
```

Le payload `update` ne contient pas le nom du fichier. La corrélation se fait plus tard avec `start.data.filename` via le même `taskid`.

### 9.3 Chargement job `start/printing curr_layer=0`

```text
payload.type   = print
payload.action = start
payload.state  = printing
payload.data.curr_layer = 0
payload.data.progress = 0
```

Cet état indique que le job est chargé/déclaré côté imprimante. Ce n’est pas encore nécessairement le début physique de l’impression.

### 9.4 Checks matériels `monitor`

| check | valeurs observées |
| --- | --- |
| airCleanerDev | -2×39 |
| fpgaDev | -2×39 |
| motor | -2×39 |
| platformDev | -2×39 |
| pullForce | 0×39 |
| resiBoxDev | -2×39 |
| resinInOutDev | -2×39 |
| screen0 | -2×39 |
| screen1 | -2×39 |
| spiFlashDev | -2×39 |
| usbDev | -2×39 |
| uvBoard | -2×39 |
| wifiDev | 0×39 |

Dans cette capture, `pullForce=0` et `wifiDev=0` sont les deux checks OK constants. Les autres sont à `-2`.

### 9.5 Contrôles automatiques `autoOperation`

| check | valeurs observées |
| --- | --- |
| autoTop | -2×131 |
| dynamicRelease | -2×131 |
| failDetection | -2×131 |
| levelling | -1×115, 0×16 |
| model | -2×131 |
| offLightCompensation | -2×131 |
| platform | 0×131 |
| residual | -1×19, 0×112 |
| resin | -1×35, 0×96 |
| resinAutoLoad | -2×131 |
| resinAutoUnload | -2×131 |
| resinCycle | -2×131 |
| resinCyclePrinting | -2×131 |
| resinHeat | -2×131 |

```text
États dynamiques observés :
residual  : -1 -> 0
resin     : -1 -> 0
levelling : -1 -> 0

Interprétation prudente :
-2 = non applicable / désactivé / non supporté
-1 = en attente / non terminé
 0 = OK / validé
```

### 9.6 Disponibilité `workReport`

| state | occurrences |
| --- | --- |
| free | 1962 |
| busy | 5513 |

| transition compressée | occurrences |
| --- | --- |
| free->busy | 20 |
| busy->free | 19 |

`workReport` est la source de vérité pour la disponibilité imprimante. Il ne porte ni `taskid`, ni `filename`, ni progression.

### 9.7 Préchauffe `start/preheating`

```text
payload.type   = print
payload.action = start
payload.state  = preheating
data.curr_layer = 0
data.progress = 0
data.heating_remain_time = -1
data.heating_skip_allowed = true
```

`heating_remain_time=-1` doit être traité comme inconnu. `remain_time` n’est pas le temps de chauffe ; il correspond au temps estimé du job.

### 9.8 Impression effective

```text
condition :
payload.type = print
payload.action = start
payload.state = printing
payload.data.curr_layer >= 1

Effets :
job.stage = printing
job.curr_layer = data.curr_layer
job.print_progress = data.progress
job.print_time = data.print_time
job.remain_time = data.remain_time
```

### 9.9 Fin d’impression

```text
condition :
payload.type = print
payload.action = start
payload.state = finished

Effets :
job.stage = finished
attendre workReport/free pour confirmer printer.availability=free
```

## 10. Jobs observés

| taskid | fichier | download | start states | layers | premier start | preheat | print effectif | finished |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 87153347 | B3_B4.pwsz | 0→100 | printing:756, finished:1 | 1977 | 2026-04-21T15:34:31.341 | — | 2026-04-21T16:49:05.772 | 2026-04-21T18:15:25.555 |
| 87212137 | B5.pwsz | 0→45→100 | printing:1 | 940 | 2026-04-21T19:42:01.338 | — | — | — |
| 87344227 | B6-B7-B8.pwsz | 0→100 | printing:40, preheating:1 | 445 | 2026-04-22T11:56:34.600 | 2026-04-22T11:58:06.771 | 2026-04-22T12:02:54.897 | — |
| 87363999 | C1.pwsz | 0→66→100 | printing:812, preheating:1, finished:1 | 810 | 2026-04-22T13:59:39.787 | 2026-04-22T14:01:25.391 | 2026-04-22T14:05:55.363 | 2026-04-22T15:07:12.893 |
| 87391271 | C2-C3-C4-C5-C7.pwsz | 0→78→100 | printing:881, preheating:1, finished:1 | 879 | 2026-04-22T16:19:51.146 | 2026-04-22T16:21:35.107 | 2026-04-22T16:25:55.257 | 2026-04-22T17:31:57.001 |
| 87423285 | C6.pwsz | 0→35→100 | printing:1467, preheating:1, finished:1 | 1465 | 2026-04-22T18:37:14.415 | 2026-04-22T18:38:58.579 | 2026-04-22T18:43:20.921 | 2026-04-22T20:22:59.989 |
| 87454686 | D1-D2-D3-D5-D7.pwsz | 0→100 | printing:490, preheating:1, finished:1 | 488 | 2026-04-22T20:51:05.692 | 2026-04-22T20:52:49.233 | 2026-04-22T20:57:02.475 | 2026-04-22T21:39:04.792 |
| 87469595 | D6.pwsz | 0→23→67→100 | printing:1310, preheating:1, finished:1 | 1314 | 2026-04-22T22:02:10.947 | 2026-04-22T22:03:54.824 | 2026-04-22T22:08:09.255 | 2026-04-22T23:38:51.448 |
| 87570661 | D5-D4-C1.pwsz | 0→59→100 | printing:1086, preheating:1 | 1210 | 2026-04-23T12:00:20.510 | 2026-04-23T12:02:05.930 | 2026-04-23T12:06:46.971 | — |
| 87591225 | D6.pwsz | 0→17→56→100 | printing:1663, preheating:1 | 2250 | 2026-04-23T14:04:23.884 | 2026-04-23T14:06:07.631 | 2026-04-23T14:10:23.640 | — |
| 87630035 | E1-E2-E3-E5-E6.pwsz | 0→94→100 | printing:497, finished:1 | 495 | 2026-04-23T17:13:33.489 | — | 2026-04-23T17:19:35.918 | 2026-04-23T18:01:02.790 |
| 87649848 | E4-E7.pwsz | 0→13→49→86→100 | printing:1993, preheating:1, finished:1 | 1991 | 2026-04-23T18:36:33.142 | 2026-04-23T18:38:17.291 | 2026-04-23T18:42:37.271 | 2026-04-23T20:52:04.157 |
| 87793966 | A1-v2.pwsz | 0→12→35→56→77→100 | printing:2911, preheating:1, finished:1 | 2909 | 2026-04-24T11:29:40.999 | 2026-04-24T11:31:27.236 | 2026-04-24T11:36:13.876 | 2026-04-24T14:35:22.087 |
| 88747018 | B1-B2-B5.pwsz | 0→12→42→72→100 | printing:126, preheating:1 | 1082 | 2026-04-28T16:05:54.724 | 2026-04-28T16:07:40.186 | 2026-04-28T16:12:06.235 | — |
| 88790862 | B3-B4-B6-B8-C7-D1-D2-D7-E2-E6.pwsz | 0→42→100 | printing:1979, preheating:1, finished:1 | 1977 | 2026-04-28T19:19:23.933 | 2026-04-28T19:21:09.954 | 2026-04-28T19:25:33.898 | 2026-04-28T22:00:37.907 |
| 88925268 | B5-v2.pwsz | 0→77→100 | printing:1, preheating:1 | 1083 | 2026-04-29T11:35:39.756 | 2026-04-29T11:37:18.893 | — | — |
| 88946284 | E4-E7.pwsz | — | printing:1029, preheating:1 | 1991 | 2026-04-29T13:50:53.543 | 2026-04-29T13:52:26.087 | 2026-04-29T13:56:48.995 | — |
| 88997191 | C2-C3-C4-C5-C7.pwsz | 0→100 | printing:823, finished:1 | 879 | 2026-04-29T18:00:25.172 | — | 2026-04-29T18:21:11.105 | 2026-04-29T19:13:11.317 |
| 89034769 | D3-D4-D5-C6-E1-E5.pwsz | 0→24→77→100 | printing:669, preheating:1 | 1210 | 2026-04-29T20:42:34.761 | 2026-04-29T20:44:00.680 | 2026-04-29T20:48:27.151 | — |
| 89158716 | B7-D4-D5.pwsz | 0→100 | printing:463, preheating:1 | 575 | 2026-04-30T12:29:33.466 | 2026-04-30T12:37:08.424 | 2026-04-30T12:41:53.392 | — |

## 11. Analyse entre deux `start`

| Métrique | Valeur |
| --- | --- |
| segments entre deux start consécutifs | 19023 |
| segments même taskid | 19004 |
| segments taskid différent | 19 |

La majorité des `start` sont des reports périodiques du même job. Les transitions réellement structurantes sont les segments avec changement de `taskid`.

### 11.1 Actions présentes entre deux `start`

| type | action | state | occurrences |
| --- | --- | --- | --- |
| status | workReport | busy | 5511 |
| status | workReport | free | 1955 |
| releaseFilm | get | done | 1955 |
| print | autoOperation | monitoring | 131 |
| print | update | downloading | 60 |
| print | monitor | monitoring | 38 |
| wifi | getSignalStrength | done | 20 |
| autoOperation | reportStatus | done | 19 |
| axis | move | done | 7 |
| lastWill | onlineReport | online | 6 |
| ota | reportVersion | report-success | 6 |
| user | bindQuery | done | 6 |
| file | cloudRecommendList | done | 6 |
| lastWill | onlineReport | offline | 5 |
| resin | feedResin | start | 1 |

### 11.2 Transitions de job entre deux `start` avec changement de `taskid`

| De | Vers | N | workReport | update progress | monitor taskids | autoOp taskids | releaseFilm | principaux events |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 87153347 / B3_B4.pwsz / finished | 87212137 / B5.pwsz / printing | 345 | free:169, busy:2 | 87212137:0 → 87212137:45 → 87212137:100 | 87212137 | — | count=169 ; first={'status': 0, 'times': 36, 'layers': 39494} ; last={'status': 0, 'times': 37, 'layers': 41471} | status/workReport/free×169, releaseFilm/get/done×169, print/update/downloading×3, status/workReport/busy×2, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |
| 87212137 / B5.pwsz / printing | 87344227 / B6-B7-B8.pwsz / printing | 79 | busy:4, free:32 | 87344227:0 → 87344227:100 | 87212137,87344227 | 87212137 | count=32 ; first={'status': 0, 'times': 38, 'layers': 42411} ; last={'status': 0, 'times': 38, 'layers': 42411} | status/workReport/free×32, releaseFilm/get/done×32, status/workReport/busy×4, print/monitor/monitoring×2, print/update/downloading×2, wifi/getSignalStrength/done×1, print/autoOperation/monitoring×1, lastWill/onlineReport/online×1 |
| 87344227 / B6-B7-B8.pwsz / printing | 87363999 / C1.pwsz / printing | 23 | free:8, busy:2 | 87363999:0 → 87363999:66 → 87363999:100 | 87363999 | — | count=8 ; first={'status': 0, 'times': 39, 'layers': 42856} ; last={'status': 0, 'times': 39, 'layers': 42856} | status/workReport/free×8, releaseFilm/get/done×8, print/update/downloading×3, status/workReport/busy×2, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |
| 87363999 / C1.pwsz / finished | 87391271 / C2-C3-C4-C5-C7.pwsz / printing | 297 | busy:2, free:145 | 87391271:0 → 87391271:78 → 87391271:100 | 87391271 | — | count=145 ; first={'status': 0, 'times': 40, 'layers': 43666} ; last={'status': 0, 'times': 40, 'layers': 43666} | status/workReport/free×145, releaseFilm/get/done×145, print/update/downloading×3, status/workReport/busy×2, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |
| 87391271 / C2-C3-C4-C5-C7.pwsz / finished | 87423285 / C6.pwsz / printing | 268 | free:131, busy:1 | 87423285:0 → 87423285:35 → 87423285:100 | 87423285 | — | count=131 ; first={'status': 0, 'times': 40, 'layers': 43666} ; last={'status': 0, 'times': 41, 'layers': 44545} | status/workReport/free×131, releaseFilm/get/done×131, print/update/downloading×3, status/workReport/busy×1, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |
| 87423285 / C6.pwsz / finished | 87454686 / D1-D2-D3-D5-D7.pwsz / printing | 119 | free:57, busy:1 | 87454686:0 → 87454686:100 | 87454686 | — | count=57 ; first={'status': 0, 'times': 41, 'layers': 44545} ; last={'status': 0, 'times': 42, 'layers': 46010} | status/workReport/free×57, releaseFilm/get/done×57, print/update/downloading×2, status/workReport/busy×1, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |
| 87454686 / D1-D2-D3-D5-D7.pwsz / finished | 87469595 / D6.pwsz / printing | 101 | busy:3, free:46 | 87469595:0 → 87469595:23 → 87469595:67 → 87469595:100 | 87469595 | — | count=46 ; first={'status': 0, 'times': 42, 'layers': 46010} ; last={'status': 0, 'times': 43, 'layers': 46498} | status/workReport/free×46, releaseFilm/get/done×46, print/update/downloading×4, status/workReport/busy×3, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |
| 87469595 / D6.pwsz / finished | 87570661 / D5-D4-C1.pwsz / printing | 123 | free:56, busy:2 | 87570661:0 → 87570661:59 → 87570661:100 | 87570661 | — | count=55 ; first={'status': 0, 'times': 43, 'layers': 46498} ; last={'status': 0, 'times': 44, 'layers': 0} | status/workReport/free×56, releaseFilm/get/done×55, print/update/downloading×3, status/workReport/busy×2, lastWill/onlineReport/offline×1, lastWill/onlineReport/online×1, ota/reportVersion/report-success×1, user/bindQuery/done×1 |
| 87570661 / D5-D4-C1.pwsz / printing | 87591225 / D6.pwsz / printing | 146 | free:68, busy:3 | 87591225:0 → 87591225:17 → 87591225:56 → 87591225:100 | 87591225 | — | count=69 ; first={'status': 0, 'times': 45, 'layers': 1210} ; last={'status': 0, 'times': 45, 'layers': 1210} | releaseFilm/get/done×69, status/workReport/free×68, print/update/downloading×4, status/workReport/busy×3, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |
| 87591225 / D6.pwsz / printing | 87630035 / E1-E2-E3-E5-E6.pwsz / printing | 9 | free:1, busy:2 | 87630035:0 → 87630035:94 → 87630035:100 | 87630035 | — | count=1 ; first={'status': 0, 'times': 46, 'layers': 3460} ; last={'status': 0, 'times': 46, 'layers': 3460} | print/update/downloading×3, status/workReport/busy×2, status/workReport/free×1, releaseFilm/get/done×1, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |
| 87630035 / E1-E2-E3-E5-E6.pwsz / finished | 87649848 / E4-E7.pwsz / printing | 154 | busy:3, free:72 | 87649848:0 → 87649848:13 → 87649848:49 → 87649848:86 → 87649848:100 | 87649848 | — | count=72 ; first={'status': 0, 'times': 46, 'layers': 3460} ; last={'status': 0, 'times': 47, 'layers': 3955} | status/workReport/free×72, releaseFilm/get/done×72, print/update/downloading×5, status/workReport/busy×3, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |
| 87649848 / E4-E7.pwsz / finished | 87793966 / A1-v2.pwsz / printing | 660 | busy:4, free:324 | 87793966:0 → 87793966:12 → 87793966:35 → 87793966:56 → 87793966:77 → 87793966:100 | 87793966 | — | count=324 ; first={'status': 0, 'times': 47, 'layers': 3955} ; last={'status': 0, 'times': 48, 'layers': 5946} | status/workReport/free×324, releaseFilm/get/done×324, print/update/downloading×6, status/workReport/busy×4, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |
| 87793966 / A1-v2.pwsz / finished | 88747018 / B1-B2-B5.pwsz / printing | 208 | free:99, busy:3 | 88747018:0 → 88747018:12 → 88747018:42 → 88747018:72 → 88747018:100 | 88747018 | — | count=99 ; first={'status': 0, 'times': 48, 'layers': 5946} ; last={'status': 0, 'times': 52, 'layers': 15543} | status/workReport/free×99, releaseFilm/get/done×99, print/update/downloading×5, status/workReport/busy×3, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |
| 88747018 / B1-B2-B5.pwsz / printing | 88790862 / B3-B4-B6-B8-C7-D1-D2-D7-E2-E6.pwsz / printing | 13 | free:3, busy:2 | 88790862:0 → 88790862:42 → 88790862:100 | 88790862 | — | count=3 ; first={'status': 0, 'times': 53, 'layers': 16625} ; last={'status': 0, 'times': 53, 'layers': 16625} | status/workReport/free×3, releaseFilm/get/done×3, print/update/downloading×3, status/workReport/busy×2, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |
| 88790862 / B3-B4-B6-B8-C7-D1-D2-D7-E2-E6.pwsz / finished | 88925268 / B5-v2.pwsz / printing | 63 | busy:3, free:24 | 88925268:0 → 88925268:77 → 88925268:100 | 88925268 | — | count=24 ; first={'status': 0, 'times': 53, 'layers': 16625} ; last={'status': 0, 'times': 54, 'layers': 18602} | status/workReport/free×24, releaseFilm/get/done×24, status/workReport/busy×3, print/update/downloading×3, axis/move/done×2, lastWill/onlineReport/offline×1, lastWill/onlineReport/online×1, ota/reportVersion/report-success×1 |
| 88925268 / B5-v2.pwsz / preheating | 88946284 / E4-E7.pwsz / printing | 17 | busy:6, free:4 | — | 88946284 | 88925268 | count=4 ; first={'status': 0, 'times': 55, 'layers': 19685} ; last={'status': 0, 'times': 55, 'layers': 19685} | status/workReport/busy×6, status/workReport/free×4, releaseFilm/get/done×4, print/autoOperation/monitoring×1, autoOperation/reportStatus/done×1, print/monitor/monitoring×1 |
| 88946284 / E4-E7.pwsz / printing | 88997191 / C2-C3-C4-C5-C7.pwsz / printing | 874 | busy:2, free:429 | 88997191:0 → 88997191:100 | 88997191 | — | count=429 ; first={'status': 0, 'times': 56, 'layers': 21676} ; last={'status': 0, 'times': 56, 'layers': 21676} | status/workReport/free×429, releaseFilm/get/done×429, status/workReport/busy×2, lastWill/onlineReport/offline×2, lastWill/onlineReport/online×2, ota/reportVersion/report-success×2, user/bindQuery/done×2, file/cloudRecommendList/done×2 |
| 88997191 / C2-C3-C4-C5-C7.pwsz / finished | 89034769 / D3-D4-D5-C6-E1-E5.pwsz / printing | 579 | free:283, busy:2 | 89034769:0 → 89034769:24 → 89034769:77 → 89034769:100 | 89034769 | — | count=283 ; first={'status': 0, 'times': 56, 'layers': 21676} ; last={'status': 0, 'times': 57, 'layers': 22555} | status/workReport/free×283, releaseFilm/get/done×283, axis/move/done×5, print/update/downloading×4, status/workReport/busy×2, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |
| 89034769 / D3-D4-D5-C6-E1-E5.pwsz / printing | 89158716 / B7-D4-D5.pwsz / printing | 16 | busy:4, free:4 | 89158716:0 → 89158716:100 | 89158716 | — | count=4 ; first={'status': 0, 'times': 58, 'layers': 23765} ; last={'status': 0, 'times': 58, 'layers': 23765} | status/workReport/busy×4, status/workReport/free×4, releaseFilm/get/done×4, print/update/downloading×2, print/monitor/monitoring×1, autoOperation/reportStatus/done×1 |

## 12. Pseudo-code d’ingestion

```text
onHttpsPrintAccepted(response):
  job = createPendingJob(
    printerId=response.printer_id,
    fileId=response.file_id,
    filename=response.filename ?? null,
    taskid=response.taskid ?? null,
    stage="command_sent"
  )

onMqttPayload(payload):
  type = payload.type
  action = payload.action
  state = payload.state
  data = payload.data

  if type == "status" and action == "workReport":
    printer.availability = state      # free | busy
    return

  if type == "print" and action == "update" and state == "downloading":
    job = getOrCreateJob(data.taskid)
    job.download_progress = data.progress
    job.stage = "downloaded" if data.progress == 100 else "downloading"
    return

  if type == "print" and action == "monitor":
    job = getOrCreateJob(data.taskid)
    job.hardware_checks = mapCheckStatus(data.checkStatus)
    if job.stage not in ["preheating", "printing", "finished"]:
      job.stage = "checking"
    return

  if type == "print" and action == "autoOperation":
    job = getOrCreateJob(data.taskid)
    job.auto_checks = mapCheckStatus(data.checkStatus)
    if job.stage not in ["preheating", "printing", "finished"]:
      job.stage = "checking"
    return

  if type == "print" and action == "start":
    job = getOrCreateJob(data.taskid)
    job.filename = data.filename
    job.curr_layer = data.curr_layer
    job.total_layers = data.total_layers
    job.print_progress = data.progress
    job.print_time = data.print_time
    job.remain_time = data.remain_time
    job.task_mode = data.task_mode
    job.settings = data.settings

    if state == "preheating":
      job.stage = "preheating"
      job.heating_skip_allowed = data.heating_skip_allowed ?? false
      job.heating_remaining_time = data.heating_remain_time if data.heating_remain_time >= 0 else null
      return

    if state == "printing":
      job.stage = "printing" if data.curr_layer >= 1 else "loaded"
      return

    if state == "finished":
      job.stage = "finished"
      return
```

## 13. Critères d’acceptation intégration

```text
CA-PRINT-01 : l’écoute MQTT est active avant l’envoi de la commande HTTPS.
CA-PRINT-02 : la réponse HTTPS ne force jamais job.stage=printing.
CA-PRINT-03 : update/downloading crée/met à jour un job par taskid.
CA-PRINT-04 : filename est rattaché au job via start.data.filename.
CA-PRINT-05 : workReport pilote uniquement printer.availability.
CA-PRINT-06 : monitor et autoOperation sont rattachés au job par taskid.
CA-PRINT-07 : start/printing curr_layer=0 produit stage=loaded.
CA-PRINT-08 : start/preheating produit stage=preheating.
CA-PRINT-09 : start/printing curr_layer>=1 produit stage=printing.
CA-PRINT-10 : start/finished produit stage=finished.
CA-PRINT-11 : absence de start/finished ne casse pas le workflow ; utiliser interrupted_or_unknown si nécessaire.
CA-PRINT-12 : chaque start répété avec le même taskid met à jour le job, mais ne crée pas un nouveau job.
```

## 14. Points de vigilance

```text
- Le corpus est constitué de reports MQTT rx. Il ne contient pas la commande HTTPS elle-même.
- Les actions MQTT observées ne doivent pas être interprétées comme des commandes envoyées par l’app.
- `start` est le nom de l’action reportée, pas forcément un événement unique de démarrage.
- `preheating` est un state de `start`, pas une action.
- `workReport` n’a pas de taskid : association temporelle seulement, jamais clé métier.
- `heating_remain_time=-1` signifie temps de chauffe inconnu.
- `remain_time` n’est pas le temps de chauffe.
- Des captures tronquées ou des jobs remplacés peuvent ne pas contenir `finished`.
```

## 15. Synthèse finale

```text
Workflow produit recommandé :

1. Initialiser MQTT.
2. Envoyer la commande HTTPS d’impression.
3. Créer un job local command_sent.
4. Attendre update/downloading pour récupérer ou confirmer taskid.
5. Attendre start/printing pour récupérer filename et état initial du job.
6. Utiliser monitor + autoOperation pour afficher les contrôles.
7. Utiliser workReport pour afficher disponibilité imprimante.
8. Utiliser start/preheating pour afficher la préparation.
9. Utiliser start/printing curr_layer>=1 pour basculer en impression effective.
10. Utiliser start/finished puis workReport/free pour clôturer proprement.
```
