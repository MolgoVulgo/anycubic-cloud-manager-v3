# Logging - Reference technique

## 1) Objectif
Cette reference documente le systeme de logs runtime de `accloud`:
- logs console
- logs fichiers JSONL
- logs erreurs/fault
- logs des points clefs (boot, CLI, import HAR, Qt/QML)

Le but est d'avoir un format stable, exploitable en debug local et en post-mortem.

---

## 2) Composants

### 2.1 JsonlLogger
Fichier: `accloud/infra/logging/JsonlLogger.{h,cpp}`

Responsabilites:
- initialisation globale du logging
- serialisation JSONL
- ecriture thread-safe
- routage multi-sinks (`app`, `fault`, `<source>`)
- mirroring console sur `stderr`

API principale:
- `initialize(config)`
- `log(...)`
- helpers: `debug/info/warn/error/fatal`
- `shutdown()`

### 2.2 Redactor
Fichier: `accloud/infra/logging/Redactor.{h,cpp}`

Responsabilites:
- masquer les valeurs sensibles dans les `fields`
- nettoyer les messages texte (tokens Bearer et query params sensibles)

Mots-cles sensibles (match partiel, case-insensitive):
- `token`, `authorization`, `cookie`, `signature`, `password`, `secret`, `credential`, `session`, `api_key`, `apikey`, `bearer`

### 2.3 Rotator
Fichier: `accloud/infra/logging/Rotator.{h,cpp}`

Responsabilites:
- rotation par taille
- retention des generations (`.1`, `.2`, ...)

Politique par defaut:
- `maxBytes = 2 MiB`
- `retention = 5`

---

## 3) Ou les logs sont ecrits

Repertoire des logs:
- par defaut: `logs/` (relatif au dossier de lancement du process)
- override: variable d'environnement `ACCLOUD_LOG_DIR`

Fichiers typiques:
- `app.jsonl`: flux global applicatif
- `<source>.jsonl`: flux source specifique (ex: `cloud.jsonl`, `qt.jsonl`)
- `fault.jsonl`: duplication des niveaux `ERROR` et `FATAL`
- `mqtt_topic_capture.jsonl`: capture analytique des messages MQTT recus (`topic` + `payload` redacted)

Important:
- chaque event est toujours ecrit dans `app.jsonl`
- un event source `cloud` est aussi ecrit dans `cloud.jsonl`
- un event `ERROR/FATAL` est aussi ecrit dans `fault.jsonl`
- la capture MQTT est ecrite dans `mqtt_topic_capture.jsonl` (ou chemin override via `ACCLOUD_MQTT_CAPTURE_PATH`)

---

## 4) Format d'une ligne JSONL

Chaque ligne est un JSON unique:

```json
{
  "ts": "2026-03-03T13:51:06.393+01:00",
  "level": "INFO",
  "source": "app",
  "component": "bootstrap",
  "event": "startup",
  "message": "Application bootstrap started",
  "fields": {
    "argc": "2",
    "mode": "default"
  }
}
```

Champs:
- `ts`: timestamp ISO-8601 local avec offset
- `level`: `DEBUG|INFO|WARN|ERROR|FATAL`
- `source`: origine logique (`app`, `cloud`, `qt`, ...)
- `component`: sous-module
- `event`: type d'evenement
- `message`: message lisible
- `fields`: donnees structurees (string->string)

`component`, `event`, `message`, `fields` peuvent etre absents si vides.

---

## 5) Sortie console

Par defaut, chaque log est aussi affiche sur `stderr`:

Format console:
`<timestamp> <source> <level> <component>.<event> - <message> key=value ...`

Exemple:
`2026-03-03T13:51:06.393+01:00 app INFO bootstrap.startup - Application bootstrap started argc=2 mode=default`

---

## 6) Integration runtime

### 6.1 Initialisation globale
Dans `app/main.cpp`:
- `logging::initialize()` au demarrage
- `std::set_terminate(...)` pour logguer les crashs non geres
- log de cycle de vie (`startup`, `shutdown`)

### 6.2 Capture Qt/QML
Quand Qt est actif:
- `qInstallMessageHandler(...)`
- capture `QtDebug/Info/Warning/Critical/Fatal`
- emission dans `source=qt`

### 6.3 Points clefs instrumentes
- `app/App.cpp`
  - entree CLI
  - mode smoke
  - import HAR start/success/fail
- `app/SessionImportBridge.cpp`
  - import HAR demande depuis QML
  - success/fail
- `infra/cloud/HarImporter.cpp`
  - load/save session
  - parse HAR
  - merge session
  - persistance session
  - erreurs detaillees

---

## 7) Redaction et securite

Le logger masque les secrets pour reduire les risques de fuite:
- `fields`: masque base sur le nom de cle
- `message`: remplacement des valeurs:
  - `Bearer <token>` -> `Bearer <redacted>`
  - query params sensibles (`access_token=...`, `id_token=...`, `refresh_token=...`, `signature=...`, `token=...`)

Recommandations:
- ne jamais logguer volontairement une session complete brute
- ne pas committer `logs/*.jsonl`
- conserver `session.json` hors partage public

---

## 8) Exploitation rapide

Commandes utiles:

```bash
# voir les derniers evenements applicatifs
tail -n 50 accloud/logs/app.jsonl

# suivre en live les erreurs
tail -f accloud/logs/fault.jsonl

# filtrer les erreurs cloud
rg '"source":"cloud".*"level":"ERROR"' accloud/logs/app.jsonl
```

---

## 9) Limites actuelles

Etat actuel:
- rotation locale simple (pas de compression gzip asynchrone)
- pas d'upload distant des logs
- pas de correlation id global force (op_id possible via `fields` selon les appels)

Ces points peuvent etre etendus sans casser le format JSONL actuel.
