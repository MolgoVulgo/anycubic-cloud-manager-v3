# EndPoints v2 Python — vérification et compléments

Source vérifiée contre le zip `manager_anycubic_cloud.zip` de la v2 Python.

## Verdict rapide

Le fichier `end_points.md` couvre bien l’essentiel, mais il mélange :
- des endpoints **réellement utilisés par la v2 Python**,
- des endpoints **observés dans docs/HAR externes mais non utilisés par la v2**,
- et quelques endpoints dont le **JSON exact n’est pas prouvé** par la v2.

## 1) Endpoints réellement recensés dans la v2 Python

Définis dans `accloud_core/endpoints.py` :

### Auth / session
- `GET https://uc.makeronline.com/login/oauth/authorize`
- `GET https://uc.makeronline.com/api/logout`
- `GET /p/p/workbench/api/v3/public/getoauthToken?code=<oauth_code>`
- `POST /p/p/workbench/api/v3/public/loginWithAccessToken`
- `POST /p/p/workbench/api/work/index/getUserStore`

### Fichiers cloud
- `POST /p/p/workbench/api/work/index/files`
- `POST /p/p/workbench/api/work/index/userFiles`
- `POST /p/p/workbench/api/work/index/getDowdLoadUrl`
- `POST /p/p/workbench/api/work/index/delFiles`
- `POST /p/p/workbench/api/work/index/renameFile`
- `POST /p/p/workbench/api/work/index/getUploadStatus`
- `GET /p/p/workbench/api/api/work/gcode/info?id=<gcode_id>`

### Upload
- `POST /p/p/workbench/api/v2/cloud_storage/lockStorageSpace`
- `PUT <preSignUrl>`
- `POST /p/p/workbench/api/v2/profile/newUploadFile`
- `POST /p/p/workbench/api/v2/cloud_storage/unlockStorageSpace`

### Impression / imprimantes / projets
- `POST /p/p/workbench/api/work/operation/sendOrder`
- `GET /p/p/workbench/api/work/printer/getPrinters`
- `POST /p/p/workbench/api/work/printer/Info`
- `GET /p/p/workbench/api/work/project/getProjects`

## 2) Endpoints présents dans `end_points.md` mais non utilisés directement par la v2 Python

Ils peuvent exister côté Anycubic, mais **ils ne sont pas dans la table d’endpoints de la v2** :
- `GET /p/p/workbench/api/user/profile/userInfo`
- `POST /p/p/workbench/api/user/device_param/getList`
- `GET /p/p/workbench/api/v2/printer/info?id=<printer_id>`
- `GET /p/p/workbench/api/v2/printer/printersStatus`
- `GET /p/p/workbench/api/v2/printer/printersStatus?file_ext=<ext>`
- `GET /p/p/workbench/api/v2/printer/printersStatus?file_id=<id>`
- `GET /p/p/workbench/api/v2/project/info?id=<project_id>`
- `GET /p/p/workbench/api/work/project/report?id=<task_id>`
- `GET /p/p/workbench/api/v2/project/printHistory?...`
- `GET /p/p/workbench/api/v2/message/getMessageCount`
- `POST /p/p/workbench/api/v2/message/getMessages`
- `GET /p/p/workbench/api/portal/index/reason`
- `POST /j/p/buried/buried/report`

Donc :
- **oui pour un catalogue global**,
- **non comme “référencé par la v2 Python”**.

## 3) Méthodes HTTP réellement présentes dans la v2

### GET
- `oauth_authorize`
- `oauth_logout`
- `oauth_token_exchange`
- `gcode_info`
- `printers`
- `projects`

### POST
- `login_with_access_token`
- `session_validate`
- `quota`
- `files`
- `files_alt`
- `download`
- `delete`
- `rename_file`
- `upload_status`
- `upload_lock`
- `upload_register`
- `upload_unlock`
- `print_order`
- `printer_info`

### PUT
- `PUT <preSignUrl>` pour l’upload binaire direct

### DELETE
- **aucun endpoint DELETE natif** dans la v2

### PATCH
- **aucun endpoint PATCH** dans la v2

## 4) Structures JSON exactes / complètes prouvées par les sources v2

## 4.1 `GET /p/p/workbench/api/work/printer/getPrinters`

### JSON complet observé
```json
{
  "code": 1,
  "data": [
    {
      "color": [],
      "create_time": 1708449350,
      "delete": 0,
      "delete_time": 0,
      "description": "A7F6-B0FF-F706-3D49",
      "device_status": 2,
      "features": null,
      "id": 42859,
      "img": "https://cdn.cloud-platform.anycubic.com/php/img/4/m3plus.png",
      "is_clean_plate": 0,
      "is_printing": 1,
      "key": "35b1681ce52f58f18feffd6880a43d36",
      "label_id": 0,
      "label_name": "",
      "last_update_time": 1770662731054,
      "machine_data": {
        "anti_max": 8,
        "format": "pw0Img",
        "name": "Anycubic Photon M3 Plus",
        "pixel": 34.4,
        "res_x": 5760,
        "res_y": 3600,
        "size_x": 197.0,
        "size_y": 122.8,
        "size_z": 245.0,
        "suffix": "pwmb"
      },
      "machine_mac": "",
      "machine_type": 107,
      "material_type": "树脂",
      "material_used": "23127.1ml",
      "max_box_num": 0,
      "model": "Anycubic Photon M3 Plus",
      "msg": "",
      "multi_color_box": [],
      "name": "Anycubic Photon M3 Plus",
      "nonce": "",
      "print_totaltime": "642小时53分",
      "ready_status": 0,
      "reason": "offline",
      "status": 1,
      "type": "LCD",
      "type_function_ids": [7, 22],
      "user_id": 94829,
      "version": null,
      "video_taskid": 0
    }
  ],
  "msg": "请求被接受",
  "pageData": {
    "count": 1,
    "page": 1,
    "page_count": 1000,
    "total": 1
  }
}
```

## 4.2 `GET /p/p/workbench/api/work/project/getProjects`

### JSON complet observé
```json
{
  "code": 1,
  "data": [
    {
      "auto_operation": null,
      "connect_status": 0,
      "create_time": 1770805803,
      "delete": 0,
      "device_message": null,
      "device_status": 1,
      "dual_platform_mode_enable": 0,
      "end_time": 0,
      "estimate": 11038,
      "evoke_from": 0,
      "gcode_id": 71469071,
      "gcode_name": "raven_skull_19_v3",
      "id": 72244987,
      "image_id": "https:....jpg",
      "img": "https:....jpg",
      "is_comment": 0,
      "is_makeronline_file": 0,
      "is_web_evoke": 0,
      "ischeck": 2,
      "key": "35b1681ce52f58f18feffd6880a43d36",
      "last_update_time": 1770808141169,
      "localtask": null,
      "machine_class": 0,
      "machine_name": "Anycubic Photon M3 Plus",
      "machine_type": 107,
      "material": "4.6219687461853",
      "material_type": 10,
      "model": 51422349,
      "monitor": null,
      "pause": 0,
      "post_title": null,
      "print_status": 1,
      "print_time": 38,
      "printed": 1,
      "printer_id": 42859,
      "printer_name": "Anycubic Photon M3 Plus",
      "progress": 14,
      "project_type": 1,
      "reason": 0,
      "remain_time": 218,
      "settings": "{\"filename\":\"demo_job.pwmb\",\"curr_layer\":321,\"total_layers\":1400,\"state\":\"printing\"}",
      "signal_strength": -1,
      "slice_data": null,
      "slice_end_time": 0,
      "slice_param": "{\"layers\":1400}",
      "slice_result": "{\"...\":...}",
      "slice_start_time": 0,
      "slice_status": 0,
      "source": "web",
      "start_time": 1770805803,
      "status": 0,
      "taskid": 72244987,
      "total_time": "???",
      "type": "LCD",
      "user_id": 94829
    }
  ],
  "msg": "请求被接受",
  "pageData": {
    "count": 1,
    "page": 1,
    "page_count": 10
  }
}
```

### Remarque critique
La v2 lit souvent :
- `device_message`
- `settings`
- `slice_param`
comme **JSON stringifié** à parser ensuite.

## 4.3 `POST /p/p/workbench/api/work/index/getUserStore`

```json
{
  "code": 1,
  "data": {
    "total": "2.00GB",
    "total_bytes": 2147483648,
    "used": "1.13GB",
    "used_bytes": 1218090634,
    "user_file_exists": true
  },
  "msg": "请求成功"
}
```

## 4.4 `POST /p/p/workbench/api/v2/cloud_storage/lockStorageSpace`

```json
{
  "code": 1,
  "data": {
    "id": 41514789,
    "preSignUrl": "https://...",
    "url": "https://.../fichier.pwmb"
  },
  "msg": "请求成功"
}
```

## 4.5 `POST /p/p/workbench/api/v2/profile/newUploadFile`

```json
{
  "code": 1,
  "data": {
    "id": 50418549
  },
  "msg": "请求成功"
}
```

## 4.6 `POST /p/p/workbench/api/v2/cloud_storage/unlockStorageSpace`

```json
{
  "code": 1,
  "data": "",
  "msg": "请求成功"
}
```

## 4.7 `POST /p/p/workbench/api/v3/public/loginWithAccessToken`

```json
{
  "code": 1,
  "data": {
    "access_token": "...",
    "id_token": "...",
    "token": "...",
    "refresh_token": "...",
    "expires_in": 3600
  },
  "msg": "..."
}
```

## 4.8 `GET /p/p/workbench/api/v3/public/getoauthToken?code=<oauth_code>`

```json
{
  "code": 1,
  "data": {
    "access_token": "...",
    "id_token": "...",
    "token": "...",
    "expires_in": 3600
  },
  "msg": "..."
}
```

## 5) Structures JSON connues mais non prouvées intégralement dans les captures v2

Ces endpoints sont **présents dans la v2**, mais le JSON exact complet n’est pas capturé dans les sources v2. On garde donc le contrat minimal seulement.

### `POST /p/p/workbench/api/work/index/getDowdLoadUrl`
```json
{
  "code": 1,
  "data": "https://...signed_url..."
}
```
ou parfois
```json
{
  "code": 1,
  "data": {
    "url": "https://...signed_url..."
  }
}
```

### `POST /p/p/workbench/api/work/index/delFiles`
```json
{
  "code": 1,
  "data": ""
}
```

### `POST /p/p/workbench/api/work/index/renameFile`
```json
{
  "code": 1,
  "data": "nouveau_nom.ext"
}
```

### `POST /p/p/workbench/api/work/index/getUploadStatus`
```json
{
  "code": 1,
  "data": {
    "gcode_id": "<gcode_id>",
    "status": 1
  }
}
```

### `GET /p/p/workbench/api/api/work/gcode/info?id=<gcode_id>`
```json
{
  "code": 1,
  "data": {
    "layers": 1234,
    "printTime": 3600,
    "resinVolume": 12.3
  },
  "msg": "..."
}
```

### `POST /p/p/workbench/api/work/operation/sendOrder`
```json
{
  "code": 1,
  "data": {
    "task_id": "70995094"
  },
  "msg": "..."
}
```

### `POST /p/p/workbench/api/work/printer/Info`
- **Aucun JSON complet prouvé** dans les sources v2.
- Endpoint présent dans la table, mais non exploité dans le flux principal.

## 6) PUT / DELETE / PATCH

### PUT
`PUT <preSignUrl>`
- body = binaire fichier
- réponse attendue = HTTP `200` ou `201`
- pas de JSON contractuel Workbench
- pas de `Authorization`
- pas de `XX-*`

### DELETE
- aucun endpoint DELETE dans la v2

### PATCH
- aucun endpoint PATCH dans la v2

## 7) Correction à appliquer au fichier `end_points.md`

### À garder
Le catalogue large est utile.

### À corriger
Il faut séparer trois niveaux :
1. **Utilisé par la v2 Python**
2. **Observé dans HAR/logs/docs mais hors flux v2**
3. **Contrat minimal seulement, JSON complet non prouvé**

### Point dur
Tu ne peux pas écrire “structure exacte et complète” pour tous les endpoints du document actuel.
Pour une partie d’entre eux, ce serait de la décoration cosmique, pas de la preuve.
