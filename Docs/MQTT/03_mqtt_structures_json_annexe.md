# Annexe MQTT — structures JSON

Cette annexe regroupe les structures JSON observées ou normalisées pour les principaux messages MQTT du projet.

But :

- éviter de charger le document principal avec trop de détail wire ;
- conserver des exemples complets et réutilisables ;
- fournir une base de mapping parseur / routeur / store.

Les exemples ci-dessous sont construits à partir des captures réelles et des règles documentaires du projet.

---

# 1. Envelope commune

## 1.1 Structure minimale

```json
{
  "type": "string",
  "action": "string"
}
```

## 1.2 Structure enrichie typique

```json
{
  "type": "string",
  "action": "string",
  "state": "string",
  "code": 0,
  "msg": "string",
  "msgid": "string",
  "timestamp": 0,
  "data": {}
}
```

## 1.3 Règles pratiques

- `type` et `action` sont obligatoires ;
- `state` est en pratique attendu sur les printer-topics connus ;
- `data` peut être `null` ;
- `code` peut être absent selon le message ;
- `msgid` n’est pas garanti sur tous les messages ;
- des champs additionnels inconnus doivent être tolérés.

---

# 2. `status/report`

## 2.1 Structure observée

```json
{
  "type": "status",
  "action": "workReport",
  "state": "free",
  "data": null
}
```

## 2.2 Variante occupée

```json
{
  "type": "status",
  "action": "workReport",
  "state": "busy",
  "data": null
}
```

## 2.3 Schéma utile

```json
{
  "type": "status",
  "action": "workReport",
  "state": "free|busy",
  "data": null
}
```

## 2.4 Mapping métier recommandé

- `free` -> imprimante disponible
- `busy` -> imprimante occupée

---

# 3. `print/report`

`print/report` est le topic le plus riche. Il existe plusieurs combinaisons `action/state`.

---

## 3.1 Téléchargement vers la machine

### Exemple observé

```json
{
  "type": "print",
  "action": "update",
  "state": "downloading",
  "code": 200,
  "data": {
    "filename": "butterfly.pwsz",
    "progress": 37,
    "taskid": "task-id"
  }
}
```

### Schéma utile

```json
{
  "type": "print",
  "action": "update",
  "state": "downloading",
  "code": 200,
  "data": {
    "filename": "string",
    "progress": 0,
    "taskid": "string"
  }
}
```

---

## 3.2 Monitoring initial

### Exemple observé

```json
{
  "type": "print",
  "action": "monitor",
  "state": "monitoring",
  "code": 200,
  "data": {
    "taskid": "task-id"
  }
}
```

### Schéma utile

```json
{
  "type": "print",
  "action": "monitor",
  "state": "monitoring",
  "code": 200,
  "data": {
    "taskid": "string"
  }
}
```

---

## 3.3 Démarrage impression — état `printing`

### Exemple complet type

```json
{
  "type": "print",
  "action": "start",
  "state": "printing",
  "code": 200,
  "data": {
    "taskid": "task-id",
    "filename": "butterfly.pwsz",
    "curr_layer": 814,
    "total_layers": 1928,
    "progress": 42,
    "remain_time": 5367,
    "print_time": 4812,
    "model_hight": 96.4,
    "z_thick": 0.05,
    "anti_count": 4,
    "supplies_usage": 87.2,
    "slicer": "PhotonWorkshop",
    "settings": {
      "bottom_exposure_time": 30,
      "normal_exposure_time": 2.4,
      "light_off_time": 0.5,
      "bottom_layers": 5
    },
    "settings_adv": {
      "lift_distance": 6,
      "lift_speed": 2,
      "retract_speed": 3
    }
  }
}
```

### Schéma utile

```json
{
  "type": "print",
  "action": "start",
  "state": "printing",
  "code": 200,
  "data": {
    "taskid": "string",
    "filename": "string",
    "curr_layer": 0,
    "total_layers": 0,
    "progress": 0,
    "remain_time": 0,
    "print_time": 0,
    "model_hight": 0,
    "z_thick": 0,
    "anti_count": 0,
    "supplies_usage": 0,
    "slicer": "string",
    "settings": {},
    "settings_adv": {}
  }
}
```

---

## 3.4 Préchauffe / préparation — état `preheating`

### Exemple observé

```json
{
  "type": "print",
  "action": "start",
  "state": "preheating",
  "code": 200,
  "data": {
    "taskid": "task-id",
    "filename": "butterfly.pwsz",
    "curr_layer": 0,
    "total_layers": 1928,
    "progress": 0,
    "remain_time": 0,
    "print_time": 0,
    "heating_remain_time": -1,
    "heating_skip_allowed": true
  }
}
```

### Schéma utile

```json
{
  "type": "print",
  "action": "start",
  "state": "preheating",
  "code": 200,
  "data": {
    "taskid": "string",
    "filename": "string",
    "curr_layer": 0,
    "total_layers": 0,
    "progress": 0,
    "remain_time": 0,
    "print_time": 0,
    "heating_remain_time": 0,
    "heating_skip_allowed": true
  }
}
```

---

## 3.5 Fin explicite — état `finished`

### Exemple observé

```json
{
  "type": "print",
  "action": "start",
  "state": "finished",
  "code": 200,
  "data": {
    "taskid": "task-id",
    "filename": "butterfly.pwsz",
    "curr_layer": 1928,
    "total_layers": 1928,
    "progress": 100,
    "remain_time": 0,
    "print_time": 10231
  }
}
```

### Schéma utile

```json
{
  "type": "print",
  "action": "start",
  "state": "finished",
  "code": 200,
  "data": {
    "taskid": "string",
    "filename": "string",
    "curr_layer": 0,
    "total_layers": 0,
    "progress": 100,
    "remain_time": 0,
    "print_time": 0
  }
}
```

---

## 3.6 Pause / reprise

### Pause — `pausing`

```json
{
  "type": "print",
  "action": "pause",
  "state": "pausing",
  "code": 200,
  "data": {
    "taskid": "task-id"
  }
}
```

### Pause — `paused`

```json
{
  "type": "print",
  "action": "pause",
  "state": "paused",
  "code": 200,
  "data": {
    "taskid": "task-id"
  }
}
```

### Reprise — `resuming`

```json
{
  "type": "print",
  "action": "resume",
  "state": "resuming",
  "code": 501,
  "data": {
    "taskid": "task-id"
  }
}
```

### Reprise — `resumed`

```json
{
  "type": "print",
  "action": "resume",
  "state": "resumed",
  "code": 200,
  "data": {
    "taskid": "task-id"
  }
}
```

---

## 3.7 Blocage / attente

### Exemple observé avec défaut machine

```json
{
  "type": "print",
  "action": "start",
  "state": "waiting",
  "code": 1306,
  "data": {
    "taskid": "task-id",
    "filename": "file.pwsz",
    "curr_layer": 1,
    "total_layers": 1928,
    "progress": 0,
    "remain_time": 0,
    "print_time": 0
  }
}
```

### Schéma utile

```json
{
  "type": "print",
  "action": "start",
  "state": "waiting",
  "code": 1306,
  "data": {
    "taskid": "string",
    "filename": "string",
    "curr_layer": 0,
    "total_layers": 0,
    "progress": 0,
    "remain_time": 0,
    "print_time": 0
  }
}
```

---

## 3.8 Arrêt / arrêt explicite

### Arrêt manuel observé

```json
{
  "type": "print",
  "action": "stop",
  "state": "stoped",
  "code": 601,
  "data": {
    "taskid": "task-id"
  }
}
```

### Autre arrêt observé

```json
{
  "type": "print",
  "action": "stop",
  "state": "stoped",
  "code": 603,
  "data": {
    "taskid": "task-id"
  }
}
```

### Schéma utile

```json
{
  "type": "print",
  "action": "stop",
  "state": "stoped|stopping|failed",
  "code": 0,
  "data": {
    "taskid": "string"
  }
}
```

---

## 3.9 `getSliceParam / done`

Structure documentaire minimale à supporter :

```json
{
  "type": "print",
  "action": "getSliceParam",
  "state": "done",
  "code": 200,
  "data": {}
}
```

---

# 4. `releaseFilm/report`

## 4.1 Structure observée

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

## 4.2 Schéma utile

```json
{
  "type": "releaseFilm",
  "action": "get",
  "state": "done",
  "data": {
    "layers": 0,
    "status": 0,
    "times": 0
  }
}
```

## 4.3 Lecture métier prudente

- `times` ressemble à un compteur de cycle / tentative ;
- `layers` ressemble à un cumul de couches traitées ;
- `status` reste à interpréter selon couverture future.

## 4.4 Point observé utile

Lors d’une tentative avortée :

- `times` peut s’incrémenter ;
- `layers` peut rester inchangé.

---

# 5. `autoOperation/report`

Dans les captures, les sous-statuts de contrôle remontent via `print` avec `action = autoOperation`.

## 5.1 Structure observée

```json
{
  "type": "print",
  "action": "autoOperation",
  "state": "monitoring",
  "code": 200,
  "data": {
    "taskid": "task-id",
    "checkStatus": [
      { "name": "platform", "status": 1206 },
      { "name": "residual", "status": -1 },
      { "name": "resin", "status": -1 },
      { "name": "levelling", "status": -1 }
    ]
  }
}
```

## 5.2 Schéma utile

```json
{
  "type": "print",
  "action": "autoOperation",
  "state": "monitoring",
  "code": 200,
  "data": {
    "taskid": "string",
    "checkStatus": [
      {
        "name": "string",
        "status": 0
      }
    ]
  }
}
```

## 5.3 Sous-statuts observés importants

```json
[
  { "name": "platform", "status": 1206 },
  { "name": "residual", "status": -1 },
  { "name": "resin", "status": -1 },
  { "name": "levelling", "status": -1 }
]
```

## 5.4 Variante après validations progressives

```json
[
  { "name": "platform", "status": 1206 },
  { "name": "residual", "status": 0 },
  { "name": "resin", "status": 0 },
  { "name": "levelling", "status": 0 }
]
```

## 5.5 Règle de parsing recommandée

Le parseur doit produire un snapshot indexé :

```json
{
  "platform": 1206,
  "residual": 0,
  "resin": 0,
  "levelling": 0
}
```

et conserver l’évolution temporelle.

---

# 6. `wifi/report`

## 6.1 Structure observée

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

## 6.2 Schéma utile

```json
{
  "type": "wifi",
  "action": "getSignalStrength",
  "state": "done",
  "data": {
    "signal_strength": 0
  }
}
```

## 6.3 Mapping métier

- champ utile principal : `signal_strength`
- usage : diagnostic réseau et corrélation stabilité live

---

# 7. `lastWill/report`

Ce type apparaît dans certaines captures de fin de session.

## 7.1 Exemple observé

```json
{
  "type": "lastWill",
  "action": "onlineReport",
  "state": "online",
  "msg": "device offline"
}
```

## 7.2 Schéma documentaire minimal

```json
{
  "type": "lastWill",
  "action": "onlineReport",
  "state": "online|offline",
  "msg": "string"
}
```

## 7.3 Note

Le message texte peut être contre-intuitif. Il faut privilégier une lecture prudente et corrélée au reste du contexte.

---

# 8. Structures documentaires supplémentaires à supporter

Les documents normatifs imposent aussi un support minimal pour d’autres familles wire, même si elles ne sont pas encore toutes validées par capture terrain.

---

## 8.1 `user`

```json
{
  "type": "user",
  "action": "bindQuery",
  "state": "done",
  "data": {}
}
```

---

## 8.2 `ota`

```json
{
  "type": "ota",
  "action": "version",
  "state": "done",
  "data": {
    "version": "string",
    "progress": 0
  }
}
```

---

## 8.3 `tempature`

```json
{
  "type": "tempature",
  "action": "report",
  "state": "done",
  "data": {
    "hotbed_current": 0,
    "hotbed_target": 0,
    "nozzle_current": 0,
    "nozzle_target": 0
  }
}
```

---

## 8.4 `fan`

```json
{
  "type": "fan",
  "action": "report",
  "state": "done",
  "data": {
    "speed_percent": 0
  }
}
```

---

## 8.5 `file`

### Liste fichiers locaux

```json
{
  "type": "file",
  "action": "listLocalFile",
  "state": "done",
  "data": {
    "files": []
  }
}
```

### Suppression fichier local

```json
{
  "type": "file",
  "action": "deleteLocalFile",
  "state": "done",
  "data": {
    "filename": "string"
  }
}
```

### Liste fichiers UDisk

```json
{
  "type": "file",
  "action": "listUdiskFile",
  "state": "done",
  "data": {
    "files": []
  }
}
```

### Suppression fichier UDisk

```json
{
  "type": "file",
  "action": "deleteUdiskFile",
  "state": "done",
  "data": {
    "filename": "string"
  }
}
```

---

## 8.6 `peripherie`

```json
{
  "type": "peripherie",
  "action": "report",
  "state": "done",
  "data": {
    "camera": true,
    "multiColorBox": false,
    "udisk": true
  }
}
```

---

## 8.7 `multiColorBox`

```json
{
  "type": "multiColorBox",
  "action": "report",
  "state": "done",
  "data": {
    "slots": [],
    "drying": {},
    "feed": {}
  }
}
```

---

## 8.8 `extfilbox`

```json
{
  "type": "extfilbox",
  "action": "report",
  "state": "done",
  "data": {
    "boxes": []
  }
}
```

---

# 9. États impression à supporter côté enums internes

États wire observés ou documentés :

```text
downloading
checking
preheating
monitoring
printing
pausing
paused
resuming
resumed
finished
stoped
stopping
updated
waiting
failed
```

Action wire observées ou documentées :

```text
start
update
monitor
autoOperation
pause
resume
stop
getSliceParam
```

---

# 10. Mapping minimal recommandé vers un snapshot interne

Exemple de snapshot interne consolidé :

```json
{
  "printerId": 0,
  "deviceKey": "string",
  "machineType": "string",
  "connectionState": "Connected",
  "online": true,
  "busy": true,
  "printState": "Printing",
  "printCode": 200,
  "filename": "butterfly.pwsz",
  "currLayer": 814,
  "totalLayers": 1928,
  "progress": 42,
  "remainTime": 5367,
  "printTime": 4812,
  "wifiSignalStrength": 55,
  "checkStatus": {
    "platform": 1206,
    "residual": 0,
    "resin": 0,
    "levelling": 0
  },
  "releaseFilm": {
    "layers": 23636,
    "status": 0,
    "times": 20
  },
  "lastMessageAt": "timestamp"
}
```

---

# 11. Règles de prudence d’interprétation

- un code isolé ne doit pas être surinterprété ;
- il faut corréler `action`, `state`, `code` et `checkStatus[]` ;
- `platform = 1206` n’est pas à lui seul un hard-stop garanti ;
- `releaseFilm/report` ne doit pas piloter seul l’état principal du job ;
- `status/report` seul ne suffit pas à expliquer un blocage ;
- les transitions sont plus importantes que les snapshots isolés.

---

# 12. Règle d’entretien

Toute nouvelle capture ou nouveau type de payload doit être ajouté ici :

- sous forme de structure ;
- avec distinction claire entre **observé**, **documenté** et **interprété** ;
- sans polluer le document principal de référence.

