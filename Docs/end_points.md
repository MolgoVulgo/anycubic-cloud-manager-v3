# EndPoints — Anycubic Cloud (Workbench)

## 0) Contrat transverse (valable pour tous les endpoints Workbench)

### 0.1 Hosts
- **Workbench API** : `https://cloud-universe.anycubic.com`
- **Compte / OAuth** : `https://uc.makeronline.com`

> Les endpoints `https://uc.makeronline.com/...` sont des endpoints “web account” (redirect/HTML) et ne suivent pas toujours le contrat JSON Workbench.

### 0.2 Construction d’une requête Workbench
**Base** : URL = `BASE_URL + path` (sauf endpoints OAuth en URL absolue).

**En-têtes communs** (posés par le client HTTP)
- `User-Agent`: identifiant applicatif
- `X-Client-Version`: version client
- `X-Region`: ex `global`
- `X-Device-Id`: identifiant device
- `X-Request-Id`: UUID par requête (corrélation logs)

**En-tête d’auth session** (si dispo)
- `Authorization: Bearer <access_token>`

**En-têtes “public signature” (Workbench uniquement)**
Ajoutés pour toute URL dont le path contient `/p/p/workbench/api/`.
- `XX-Device-Type: web`
- `XX-IS-CN: 2`
- `XX-Version: <public_version>`
- `XX-Nonce: <uuid1>`
- `XX-Timestamp: <epoch_ms>`
- `XX-Token: <token>` (optionnel, issu de la session)
- `XX-Signature: <md5>`

**Signature (implémentée)**
- `XX-Signature = md5(app_id + timestamp + version + app_secret + nonce + app_id)`

### 0.3 Body / Query
- Les endpoints Workbench “métier” utilisent majoritairement **POST + JSON**.
- Certains endpoints utilisent **GET + query string** (`?id=...`, `?code=...`).
- Un cas spécial utilise **POST + form** (`sendOrder`).
- Les **URLs signées** (download/upload S3) se consomment via **GET/PUT directs** et **sans en-têtes d’auth Workbench**.

### 0.4 Retry / erreurs
- **Retry** borné sur erreurs transport + statuts retryables (typiquement `429`/`5xx`).
- `401/403` déclenchent une **tentative de récupération d’auth** (re-login via `loginWithAccessToken`) si des tokens candidats existent.

### 0.5 Contrat de réponse Workbench
**Enveloppe la plus fréquente** :
```json
{ "code": 1, "msg": "...", "data": <object|list|string> }
```
- `code == 1` : succès métier.
- `code != 1` : erreur métier (même si HTTP 200).
- `code` absent : traité comme succès “best-effort” (rare).

**Pièges récurrents**
- Champs parfois **JSON-stringifiés** (ex: `settings`, `slice_param`, `monitor`, `auto_operation`) → parse best-effort.
- Pagination parfois dans `pageData` (au lieu d’un wrapper `data.items`).

**Règles de sécurité/logging**
- Ne jamais logger un **token** / `Authorization` / `XX-*` complet.
- Ne jamais logger une **URL signée complète** (query sensible).

---

## 1) Auth / OAuth

### 1.1 OAuth authorize
**Endpoint** : `GET https://uc.makeronline.com/login/oauth/authorize`

**But**
- Démarrer l’auth “compte” côté navigateur (obtention d’un `code` OAuth via redirect).

**Construction de l’envoi**
- **GET** avec paramètres typiques OAuth : `client_id`, `redirect_uri`, `response_type=code`, `scope`, `state`.

**Retour**
- Généralement **redirect HTTP** + HTML (pas un JSON stable).

**Analyse**
- Endpoint UI web.
- Le `code` s’obtient via l’URL de callback.

---

### 1.2 OAuth logout
**Endpoint** : `GET https://uc.makeronline.com/api/logout`

**But**
- Invalider la session côté domaine compte.

**Construction de l’envoi**
- **GET** (souvent cookie-based côté web).

**Retour**
- Réponse web (JSON non garanti) / redirect.

**Analyse**
- Optionnel dans un client purement token-based.

---

### 1.3 Exchange OAuth `code` → tokens
**Endpoint** : `GET /p/p/workbench/api/v3/public/getoauthToken?code=<oauth_code>`

**But**
- Échanger un **code OAuth** contre des tokens utilisables.

**Construction de l’envoi**
- **GET**
- Query : `code=<oauth_code>`
- Headers Workbench : oui (namespace `/p/p/workbench/api/`).

**Retour JSON (typique)**
```json
{ "code": 1, "data": { "access_token": "...", "id_token": "...", "token": "...", "expires_in": 3600 }, "msg": "..." }
```

**Analyse**
- Endpoint “public” mais signé (`XX-*`).

---

### 1.4 Login applicatif via access token
**Endpoint** : `POST /p/p/workbench/api/v3/public/loginWithAccessToken`

**But**
- Convertir/valider un token et récupérer un set de tokens normalisés côté Workbench.

**Construction de l’envoi**
- **POST** JSON
- Payload (variantes acceptées) :
```json
{ "access_token": "<token>", "device_type": "web" }
```
ou
```json
{ "accessToken": "<token>", "device_type": "web" }
```

**Retour JSON (typique)**
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

**Analyse**
- Endpoint pivot : utilisé aussi pour la **récupération automatique** après `401/403`.
- Côté client :
  - `Authorization = Bearer <access_token>`
  - conserver `token` (alimentation de `XX-Token`).

---

## 2) Profil utilisateur

### 2.1 User info
**Endpoint** : `GET /p/p/workbench/api/user/profile/userInfo`

**But**
- Récupérer le profil compte (identité, affichage, flags).

**Construction de l’envoi**
- **GET**
- Headers Workbench + `Authorization`.

**Retour JSON (structure)**
```json
{ "code": 1, "data": { "id": 0, "avatar": "", "casdoor_user": { "display_name": "...", "email": "..." } }, "msg": "..." }
```

**Analyse**
- Données sensibles (email, tokens dérivés) → redaction stricte.

---

### 2.2 Device params list
**Endpoint** : `POST /p/p/workbench/api/user/device_param/getList`

**But**
- Récupérer des presets / paramètres user-scoped (profils de settings), typiquement paginés.

**Construction de l’envoi**
- **POST** JSON
```json
{ "limit": 20, "page": 1 }
```

**Retour JSON (structure)**
```json
{
  "code": 1,
  "msg": "Operation successful",
  "data": [
    {
      "id": 2937,
      "user_id": 94829,
      "settings_name": "test",
      "deleted": 0,
      "create_time": 1741913222,
      "update_time": 1741913222
    }
  ],
  "pageData": { "page": 1, "page_count": 20, "total": 1 }
}
```

**Analyse**
- Endpoint **user-scoped** (pas lié à une imprimante précise).
- `pageData` est un pattern à supporter proprement.

---

## 3) Quota / capacité

### 3.1 Store quota
**Endpoint** : `POST /p/p/workbench/api/work/index/getUserStore`

**But**
- Lire quota total + utilisé (stockage cloud utilisateur).

**Construction de l’envoi**
- **POST**
- Body souvent vide `{}`.

**Retour JSON (structure)**
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
  "msg": "..."
}
```

**Analyse**
- Les valeurs “lisibles” et les bytes peuvent coexister.

---

## 4) Fichiers cloud

### 4.1 Listing fichiers (variante A)
**Endpoint** : `POST /p/p/workbench/api/work/index/files`

**But**
- Lister les fichiers stockés dans le cloud.

**Construction de l’envoi**
- **POST** JSON
```json
{ "page": 1, "limit": 20 }
```

**Retour JSON (structure)**
```json
{
  "code": 1,
  "data": [
    {
      "id": 0,
      "old_filename": "x.pwmb",
      "size": 123,
      "gcode_id": 0,
      "thumbnail": "...",
      "path": "file/.../x.pwmb",
      "slice_param": { "...": "..." }
    }
  ],
  "msg": "..."
}
```

**Analyse**
- `data` est souvent **une liste directe**.
- `slice_param` peut être objet **ou** string JSON → parse best-effort.

---

### 4.2 Listing fichiers (variante B)
**Endpoint** : `POST /p/p/workbench/api/work/index/userFiles`

**But**
- Alternative au listing “files” (backend variable).

**Construction de l’envoi**
- **POST** JSON
```json
{ "page": 1, "limit": 20 }
```

**Retour JSON**
- Variable (parfois incompatible avec un listing).

**Analyse**
- Prévoir fallback automatique vers `files`.
- Valider le type de `data` avant d’interpréter.

---

### 4.3 URL de téléchargement signée
**Endpoint** : `POST /p/p/workbench/api/work/index/getDowdLoadUrl`

**But**
- Obtenir une **URL signée** (S3/CDN) pour télécharger un fichier.

**Construction de l’envoi**
- **POST** JSON (variantes fréquentes)
```json
{ "id": 53095239 }
```
ou
```json
{ "file_id": "53095239" }
```
ou
```json
{ "ids": [53095239] }
```

**Retour JSON (structure)**
```json
{ "code": 1, "data": "https://...<signed_url>..." }
```
(ou `{ "code": 1, "data": { "url": "https://..." } }` selon backend)

**Analyse**
- Étape 1 du download.
- Download réel = `GET` direct sur l’URL signée, **sans** `Authorization` / `XX-*`.

---

### 4.4 Suppression fichier
**Endpoint** : `POST /p/p/workbench/api/work/index/delFiles`

**But**
- Supprimer un ou plusieurs fichiers.

**Construction de l’envoi**
- **POST** JSON
```json
{ "idArr": [53095239] }
```

**Retour JSON (structure)**
```json
{ "code": 1, "data": "" }
```

**Analyse**
- `idArr` peut parfois être reçu comme string côté serveur → tolérance utile.

---

### 4.5 Renommage fichier
**Endpoint** : `POST /p/p/workbench/api/work/index/renameFile`

**But**
- Renommer un fichier (nom d’affichage).

**Construction de l’envoi**
- **POST** JSON
```json
{ "id": 53095239, "name": "nouveau_nom.<ext>" }
```

**Retour JSON (structure)**
```json
{ "code": 1, "data": "nouveau_nom.<ext>" }
```

**Analyse**
- La règle de validation serveur sur l’extension peut être stricte (machine/format).

---

### 4.6 Statut post-upload
**Endpoint** : `POST /p/p/workbench/api/work/index/getUploadStatus`

**But**
- Récupérer l’état serveur d’un fichier après upload.

**Construction de l’envoi**
- **POST** JSON
```json
{ "id": 53095239 }
```

**Retour JSON (structure)**
```json
{ "code": 1, "data": { "gcode_id": "<gcode_id>", "status": 1 }, "msg": "..." }
```

**Analyse**
- `status` est numérique (pratique pour afficher “processing/ready/failed”).

---

## 5) GCode / infos modèle

### 5.1 Gcode info
**Endpoint** : `GET /p/p/workbench/api/api/work/gcode/info?id=<gcode_id>`

**But**
- Récupérer des infos de slicing (couches, temps, résine, etc.).

**Construction de l’envoi**
- **GET**
- Query : `id=<gcode_id>`

**Retour JSON (structure)**
```json
{ "code": 1, "data": { "layers": 1234, "printTime": 3600, "resinVolume": 12.3 }, "msg": "..." }
```

**Analyse**
- Les clés peuvent varier (`print_time`, `printTime`, `estimate`, …).

---

## 6) Upload (workflow multi-étapes)

### 6.1 Lock storage space
**Endpoint** : `POST /p/p/workbench/api/v2/cloud_storage/lockStorageSpace`

**But**
- Réserver de l’espace + obtenir une URL signée d’upload.

**Construction de l’envoi**
- **POST** JSON
```json
{ "name": "fichier.<ext>", "size": 1234567, "is_temp_file": 0 }
```

**Retour JSON (structure)**
```json
{ "code": 1, "data": { "id": 41514789, "preSignUrl": "https://...", "url": "https://.../fichier.<ext>" }, "msg": "..." }
```

**Analyse**
- `id` = lock id → requis pour `newUploadFile` et `unlockStorageSpace`.

---

### 6.2 Upload binaire direct
**Endpoint** : `PUT <preSignUrl>`

**But**
- Envoyer les octets du fichier vers le storage.

**Construction de l’envoi**
- **PUT**
- Body = bytes du fichier (stream)
- **SANS** `Authorization` et **SANS** headers `XX-*`.

**Retour**
- HTTP `200/201`.

**Analyse**
- Timeout/retry doivent être maîtrisés (upload volumineux).

---

### 6.3 Register uploaded file
**Endpoint** : `POST /p/p/workbench/api/v2/profile/newUploadFile`

**But**
- Enregistrer le fichier uploadé côté Workbench (création `file_id`).

**Construction de l’envoi**
- **POST** JSON
```json
{ "user_lock_space_id": 41514789 }
```

**Retour JSON (structure)**
```json
{ "code": 1, "data": { "id": 50418549 }, "msg": "..." }
```

**Analyse**
- `data.id` = `file_id` Workbench.

---

### 6.4 Unlock storage space
**Endpoint** : `POST /p/p/workbench/api/v2/cloud_storage/unlockStorageSpace`

**But**
- Libérer le lock.

**Construction de l’envoi**
- **POST** JSON
```json
{ "id": 41514789, "is_delete_cos": 0 }
```

**Retour JSON (structure)**
```json
{ "code": 1, "data": "" }
```

**Analyse**
- À appeler en **finally** même si une étape échoue.

---

## 7) Impression / projets

### 7.1 Send print order
**Endpoint** : `POST /p/p/workbench/api/work/operation/sendOrder`

**But**
- Démarrer une impression sur une imprimante depuis un fichier cloud.

**Construction de l’envoi**
- Variante la plus compatible : **POST form** avec un champ `data` contenant un JSON string.

Form fields (shape observée) :
- `printer_id`: `<printer_id>`
- `project_id`: `0`
- `order_id`: `1`
- `is_delete_file`: `0`
- `data`: `{"file_id":"<file_id>","matrix":"","filetype":0,"project_type":1,"template_id":-2074360784}`

**Retour JSON (structure)**
```json
{ "code": 1, "data": { "task_id": "70995094" }, "msg": "..." }
```

**Analyse**
- Endpoint fragile : payload très spécifique.
- Si tu ajoutes une variante `POST JSON`, fais-la en fallback, jamais en défaut.

---

### 7.2 Liste projets (jobs)
**Endpoint** : `GET /p/p/workbench/api/work/project/getProjects`

**But**
- Lister les tâches/projets d’impression, filtrables par imprimante et statut.

**Construction de l’envoi**
- **GET**
- Query courante :
  - `printer_id=<printer_id>`
  - `page=<n>`
  - `limit=<n>`
  - `print_status=<int>` (optionnel)

Exemple :
```
GET /p/p/workbench/api/work/project/getProjects?limit=10&page=1&print_status=1&printer_id=525668
```

**Retour JSON (structure)**
```json
{
  "code": 1,
  "msg": "Request accepted",
  "data": [
    {
      "id": 76363144,
      "taskid": 76363144,
      "printer_id": 525668,
      "model": 54623303,
      "gcode_id": 75592054,
      "estimate": 1854,
      "remain_time": 22,
      "progress": 6,
      "print_status": 1,
      "machine_type": 128,
      "printer_name": "Anycubic Photon Mono M7 Pro",
      "settings": "{...}",
      "slice_param": "{...}",
      "auto_operation": "[...]",
      "monitor": "[...]",
      "signal_strength": 48
    }
  ],
  "pageData": { "page": 1, "page_count": 10, "count": 1 }
}
```

**Analyse**
- `settings`, `slice_param`, `monitor`, `auto_operation` peuvent être **des strings JSON**.
- `pageData` doit être supporté (pagination).

---

### 7.3 Détail projet
**Endpoint** : `GET /p/p/workbench/api/v2/project/info?id=<project_id>`

**But**
- Détails d’un job (progress, couches, erreurs, device_message, etc.).

**Construction de l’envoi**
- **GET**
- Query : `id=<project_id>`

**Retour JSON (structure)**
```json
{ "code": 1, "data": { "id": 0, "printer_id": 0, "device_message": { "action": "start", "curr_layer": 155, "progress": 12, "err_message": "" } }, "msg": "..." }
```

**Analyse**
- `device_message` est le meilleur signal UI (progress/erreurs/temps).

---

## 8) Imprimantes

### 8.1 Liste imprimantes
**Endpoint** : `GET /p/p/workbench/api/work/printer/getPrinters`

**But**
- Lister les imprimantes associées au compte + état.

**Construction de l’envoi**
- **GET**

**Retour JSON (structure)**
```json
{ "code": 1, "data": [ { "id": 0, "name": "...", "available": 1, "status": 1, "settings": { ... }, "device_message": { ... }, "project": { ... } } ], "msg": "..." }
```

**Analyse**
- La hiérarchie (`settings`, `device_message`, `project`, `base`) varie selon modèle.
- Pour l’UI : dériver `online/state` via `available/device_status/status/is_printing` + `reason` (quand présent).

---

### 8.2 Info imprimante (endpoint historique)
**Endpoint** : `POST /p/p/workbench/api/work/printer/Info`

**But**
- Détails imprimante via un endpoint plus ancien.

**Construction de l’envoi**
- **POST** JSON
```json
{ "id": <printer_id> }
```

**Retour JSON**
- Backend variable.

**Analyse**
- À garder en option / fallback.

---

### 8.3 Info imprimante v2
**Endpoint** : `GET /p/p/workbench/api/v2/printer/info?id=<printer_id>`

**But**
- Détails “v2” (métadonnées machine, image, versions, etc.).

**Construction de l’envoi**
- **GET**
- Query : `id=<printer_id>`

**Retour JSON (structure)**
```json
{ "code": 1, "data": { "id": 0, "name": "...", "img": "https://...png", "version": { ... } }, "msg": "..." }
```

**Analyse**
- Plus stable que `work/printer/Info` pour une fiche printer.

---

### 8.4 Status imprimantes (bulk)
**Endpoint** : `GET /p/p/workbench/api/v2/printer/printersStatus`

**But**
- Récupérer un état consolidé (liste d’imprimantes + état).

**Construction de l’envoi**
- **GET**
- Query : none (souvent) / ou paramètres de compatibilité (voir ci-dessous).

**Retour JSON (structure minimale)**
```json
{ "code": 1, "data": [ { "id": 0, "name": "...", "device_status": 1, "available": 1, "reason": "free" } ], "msg": "..." }
```

**Analyse**
- Le couple `available` + `reason` sert aussi de signal de compatibilité (pas seulement online/offline).

---

### 8.5 Compatibilité par extension (file_ext)
**Endpoint** : `GET /p/p/workbench/api/v2/printer/printersStatus?file_ext=<ext>`

**But**
- Lister les imprimantes et **indiquer** si elles acceptent un type de fichier donné.

**Construction de l’envoi**
- **GET**
- Query : `file_ext=pwsz` (exemple), ou `file_ext=pwmb`.

**Retour JSON (structure)**
```json
{
  "code": 1,
  "msg": "Request accepted",
  "data": [
    {
      "id": 525668,
      "name": "Anycubic Photon Mono M7 Pro",
      "machine_type": 128,
      "type": "LCD",
      "device_status": 1,
      "is_printing": 1,
      "available": 1,
      "reason": "free"
    },
    {
      "id": 42859,
      "name": "Anycubic Photon M3 Plus",
      "machine_type": 107,
      "device_status": 2,
      "available": 2,
      "reason": "unavailable reason:Slice file does not match machine type"
    }
  ]
}
```

**Analyse**
- `available/reason` matérialisent la compatibilité **fichier ↔ machine**.
- À utiliser avant d’afficher un bouton “Print” sur un fichier.

---

### 8.6 Compatibilité par identifiant (file_id)
**Endpoint** : `GET /p/p/workbench/api/v2/printer/printersStatus?file_id=<id>`

**But**
- Même idée que `file_ext`, mais basée sur un identifiant de fichier/modèle côté backend.

**Construction de l’envoi**
- **GET**
- Query : `file_id=<id>`

**Retour JSON**
- Même structure que `file_ext`.

**Analyse**
- Permet au front de dire : “ce fichier cloud-là, sur quelles machines je peux le lancer ?”.

---

## 9) Messages / notifications

### 9.1 Liste messages
**Endpoint** : `POST /p/p/workbench/api/v2/message/getMessages`

**But**
- Récupérer les messages/notifications (système, prints, etc.).

**Construction de l’envoi**
- **POST** JSON (pagination typique)
```json
{ "page": 1, "limit": 20 }
```

**Retour JSON (structure)**
```json
{ "code": 1, "data": [ { "type": 3, "title": "", "content": "...", "count": 0, "newcount": 0, "create_time": 0 } ], "msg": "..." }
```

**Analyse**
- Liste plate.
- Champs souvent vides/0.

---

## 10) Télémétrie / analytics

### 10.1 Buried report
**Endpoint** : `POST /j/p/buried/buried/report`

**But**
- Endpoint de tracking (événements UI / stats). Non requis pour le flux fonctionnel.

**Construction de l’envoi**
- **POST** JSON

**Retour JSON**
- Généralement `{ "code": 1, "data": "" }` ou équivalent.

**Analyse**
- Hors scope dans un client desktop.

---

## 11) Portail / raisons (table d’état)

### 11.1 Reasons catalog
**Endpoint** : `GET /p/p/workbench/api/portal/index/reason`

**But**
- Récupérer une table `reason/desc/help_url` utilisée côté UI (messages d’état, popups, préflight).

**Construction de l’envoi**
- **GET**
- Headers Workbench + `Authorization`.

**Retour JSON (structure)**
```json
{
  "code": 1,
  "msg": "...",
  "data": [
    {
      "id": 1,
      "reason": "free",
      "desc": "...",
      "type": "LCD",
      "push": 0,
      "ui_style": 0,
      "popup": 0,
      "help_url": "..."
    }
  ]
}
```

**Analyse**
- Utile pour traduire un `reason` brut en message UI.
- Certaines réponses peuvent contenir des caractères “sales” : parser JSON strict + fallback robustes (ex: rejet propre si invalide).

---

## 12) Photon Mono M7 Pro — différences utiles (intégration)

### 12.1 Format & compat
- Extension observée : **`pwsz`** (à traiter comme format “slice” distinct de `pwmb`).
- `printersStatus?file_ext=pwsz` est le mécanisme direct pour valider la compatibilité d’un fichier.

### 12.2 Machine profile (exemples de champs)
- `machine_type`: **128** (ex: M7 Pro) vs **107** (ex: M3 Plus)
- Résolution LCD (ex): **11520×5120** (peut impacter l’affichage côté UI)
- `type_function_ids` (ex): `[1,2,3,26,27,29,31,33,34,35]` (mapping incomplet tant que l’UI/API ne donne pas le dictionnaire des IDs)

### 12.3 Garde-fous UV (mode “safe”)
But : éviter les actions susceptibles d’activer l’UV (ex: nettoyage bac) tant que le mapping des fonctions n’est pas entièrement prouvé.
- Bloquer par défaut les actions associées aux IDs suspects tant que le mapping est incomplet (ex: `{3,29,33}`).
- Bloquer toute action dont la sémantique ressemble à du “cleaning” / “vat cleaning” / “exposure detection” tant que non validée.

