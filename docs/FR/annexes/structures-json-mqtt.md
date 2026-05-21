# Annexe — Structures JSON MQTT

Statut : `SNAPSHOT`.

## Envelope commune

```json
{
  "action": "...",
  "state": "...",
  "type": "...",
  "taskid": "...",
  "data": {},
  "code": 0,
  "msg": "..."
}
```

Règles : ne pas supposer tous les champs présents, préserver inconnus en diagnostic, mapper connus vers domaine, conserver topic + payload ensemble.

## `status/report`

État global imprimante : free/busy/degraded, taskid éventuel, status code, disponibilité, reasons. Ne suffit pas seul pour expliquer blocage.

## `print/report`

Transitions print : downloading, checking, preheating, printing, pausing, paused, resuming, resumed, finished, stopping, stopped, failed, blocked. Champs utiles : taskid, fichier, progress, layer courant/total, temps restant, reason/code, check maps.

## `autoOperation/report`

Checks automatiques et opérations hardware/autoload. Interprétation dépendante de phase.

## `releaseFilm/report`

Télémétrie film/release/lift. Les seuils ne sont pas universels sans preuve modèle/matériau.

## `wifi/report`

État réseau, possible explication d’un mode degraded, pas échec print seul.

## Entretien

Pour chaque nouveau payload : topic, sample redacted, modèle, contexte, décision mapping, champs ignorés, fixture test si possible.
