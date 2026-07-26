# Workflow résine Anycubic — auto-load, remplissage cuve et erreurs bouteille

## 1. Objectif

Ajouter un workflow métier capable de distinguer correctement :

1. le remplissage / contrôle résine avant `preheating` ;
2. les tentatives de remplissage pendant le print ;
3. une bouteille de résine vide ;
4. une cuve réellement vide ou une suspicion d’échec matière.

Le point critique : une erreur `feedResin` pendant le print ne signifie pas automatiquement que la cuve est vide. Elle indique surtout que la source de remplissage automatique, typiquement la bouteille, est vide ou indisponible.

---

## 2. Définitions métier

### 2.1 Cuve / VAT

La cuve contient la résine disponible pour l’exposition en cours. Tant qu’il reste assez de résine dans la VAT, le print peut continuer même si la bouteille d’auto-remplissage est vide.

### 2.2 Bouteille / réservoir auto-load

La bouteille sert au système de remplissage automatique. Si elle est vide, la machine peut remonter une erreur `feedResin`, mais cela ne veut pas dire que la VAT est déjà vide.

### 2.3 Auto-load résine

Sur M7 Pro, et même principe sur M3 Pro, le système ne calcule pas précisément une quantité de résine à ajouter. Il remplit la cuve jusqu’au repère / niveau maximum détecté.

### 2.4 failDetection

Un problème réel de résine dans la cuve pendant impression devrait plutôt être détecté par un signal de type `failDetection`, `fault`, ou par un passage du print en `failed` / `stopped`.

---

## 3. Les deux workflows à distinguer

## 3.1 Workflow A — remplissage / contrôle avant preheating

### Phase

Avant le passage à `preheating`.

### But

Préparer la machine avant le print :

- vérifier la présence / disponibilité résine ;
- remplir la cuve si auto-load disponible ;
- autoriser ensuite la chauffe résine / chauffe machine.

### Signaux à surveiller

- `autoOperation/report`
- état / capacité `resinAutoLoad`
- état / capacité `resin`
- état / capacité `resinHeat`
- `print/start` ou équivalent avec `state=preheating`
- éventuel `resin/report action=feedResin`

### Règle d’inférence

Si :

- l’auto-load résine est disponible / supporté ;
- le contrôle résine est accepté ;
- la machine passe ensuite en `preheating` ;

alors le remplissage / contrôle pré-print doit être considéré comme réussi.

Le passage en `preheating` est le signal de validation implicite. Si le remplissage pré-print avait échoué de manière bloquante, la machine ne devrait pas continuer vers `preheating`.

### Résultat métier

```json
{
  "phase": "before_preheating",
  "workflow": "pre_print_resin_fill",
  "status": "success_inferred",
  "blocking_print": false,
  "reason": "machine_reached_preheating_after_resin_autoload_check"
}
```

### Cas d’échec bloquant

Si une erreur `feedResin` apparaît avant `preheating` et que la machine ne passe pas en `preheating`, alors l’erreur doit être considérée comme bloquante.

```json
{
  "phase": "before_preheating",
  "workflow": "pre_print_resin_fill",
  "status": "failed",
  "blocking_print": true,
  "reason": "feedResin_error_before_preheating_and_no_preheating_transition"
}
```

---

## 3.2 Workflow B — remplissage pendant impression

### Phase

Pendant `printing`, après le début effectif des couches.

### But

Maintenir le niveau de résine dans la cuve jusqu’au repère maximum.

### Signaux à surveiller

- `resin/report`
- `action=feedResin`
- `code`
- progression couche avant / après événement
- `failDetection`
- `fault/report`
- état final du print : `printing`, `finished`, `failed`, `stopped`

### Règle d’inférence

Si `feedResin code=1501` apparaît pendant `printing`, alors :

- interpréter comme bouteille vide / source auto-load indisponible ;
- ne pas interpréter comme cuve vide ;
- ne pas marquer automatiquement le print comme bloqué ;
- vérifier si les couches continuent après l’événement.

### Résultat métier

```json
{
  "phase": "during_print",
  "workflow": "runtime_resin_topup",
  "action": "feedResin",
  "code": 1501,
  "meaning": "bottle_empty_or_autoload_source_unavailable",
  "vat_empty": false,
  "blocking_print": false
}
```

### Cas d’échec réel possible

Si après un `feedResin` en erreur on observe :

- un `failDetection` ;
- un `fault/report` lié résine ;
- un passage `print.state=failed` ;
- un passage `print.state=stopped` ;
- un arrêt durable de progression couche ;

alors créer une suspicion d’échec matière / cuve insuffisante.

```json
{
  "phase": "during_print",
  "workflow": "runtime_resin_topup",
  "status": "suspected_vat_or_resin_failure",
  "blocking_print": true,
  "reason": "feedResin_error_followed_by_failDetection_or_print_failure"
}
```

---

## 4. Mapping minimal des codes

## 4.1 Code 1501

### Contexte : avant preheating

`code=1501` avant `preheating` peut être bloquant si la machine ne continue pas vers `preheating`.

Interprétation :

```json
{
  "code": 1501,
  "phase": "before_preheating",
  "meaning": "resin_autoload_source_empty_or_unavailable",
  "blocking_print": true,
  "condition": "no_transition_to_preheating"
}
```

### Contexte : pendant printing

`code=1501` pendant impression indique une tentative de remplissage impossible côté source auto-load.

Interprétation :

```json
{
  "code": 1501,
  "phase": "during_print",
  "meaning": "bottle_empty_or_autoload_source_unavailable",
  "vat_empty": false,
  "blocking_print": false,
  "condition": "layers_continue_and_no_failDetection"
}
```

---

## 5. États internes recommandés

Ajouter un état résine séparé de l’état print.

```json
{
  "resin_state": {
    "autoload_supported": null,
    "autoload_enabled_or_ready": null,
    "pre_print_fill_status": "unknown",
    "runtime_topup_status": "unknown",
    "bottle_status": "unknown",
    "vat_status": "unknown",
    "last_feedResin_code": null,
    "last_feedResin_at": null,
    "last_feedResin_phase": null,
    "blocking_print": false
  }
}
```

Valeurs recommandées :

```text
pre_print_fill_status:
- unknown
- success_inferred
- failed
- not_applicable

runtime_topup_status:
- unknown
- attempted
- failed_bottle_empty
- success

bottle_status:
- unknown
- ok
- empty_or_unavailable

vat_status:
- unknown
- assumed_ok
- suspected_empty
- failed_detection
```

---

## 6. Détection de phase

La phase doit être calculée avant d’interpréter `feedResin`.

```text
Si print pas encore lancé :
  phase = idle_or_setup

Si print lancé mais pas encore preheating :
  phase = before_preheating

Si state == preheating :
  phase = preheating

Si state == printing et curr_layer > 0 :
  phase = during_print

Si state == finished :
  phase = finished

Si state == failed ou stopped :
  phase = terminal_error
```

---

## 7. Pseudo-logique d’intégration

```pseudo
on_event(event):
    update_print_state(event)
    update_auto_operation_state(event)

    phase = compute_phase(current_print_state)

    if event.topic == "autoOperation/report":
        update_resin_capabilities(event)

    if transition_to("preheating"):
        if resin_autoload_supported_or_resin_check_seen:
            resin_state.pre_print_fill_status = "success_inferred"
            resin_state.vat_status = "assumed_ok"
            resin_state.blocking_print = false

    if event.topic == "resin/report" and event.action == "feedResin":
        resin_state.last_feedResin_code = event.code
        resin_state.last_feedResin_at = event.timestamp
        resin_state.last_feedResin_phase = phase

        if phase == "before_preheating":
            if event.code != 0:
                mark_pending_preheat_blocking_check(event)

        if phase == "during_print":
            if event.code == 1501:
                resin_state.runtime_topup_status = "failed_bottle_empty"
                resin_state.bottle_status = "empty_or_unavailable"
                resin_state.vat_status = "assumed_ok"
                resin_state.blocking_print = false

    if pending_preheat_blocking_check and no_transition_to_preheating:
        resin_state.pre_print_fill_status = "failed"
        resin_state.blocking_print = true

    if event.topic contains "failDetection" or print_state in ["failed", "stopped"]:
        if recent_feedResin_error_during_print:
            resin_state.vat_status = "suspected_empty"
            resin_state.blocking_print = true
```

---

## 8. Règles de corrélation temporelle

### 8.1 Avant preheating

Une erreur `feedResin` pré-print doit rester en état pending jusqu’à observer soit :

- transition vers `preheating` → erreur non bloquante ou résolue ;
- absence de transition + arrêt workflow → erreur bloquante.

### 8.2 Pendant printing

Après une erreur `feedResin` pendant print, vérifier :

- la couche suivante est-elle reçue ?
- la progression continue-t-elle ?
- le print finit-il en `finished` ?
- y a-t-il un `failDetection` ?

Si les couches continuent et que le print finit, `blocking_print=false`.

---

## 9. Cas de référence — 06/05/2026

### Résumé

- Fichier imprimé : `A1-v2.pwsz`
- Print lancé : 12:05:07
- Passage téléchargement : 12:05:11 → 12:05:26
- Passage `preheating` : 12:13:30
- Début impression effective : 12:18:12, couche 1
- Fin impression : 15:18:09, `finished`

### Pré-print

Le remplissage / contrôle résine avant chauffe est considéré comme réussi, car la machine passe ensuite en `preheating`.

```json
{
  "date": "2026-05-06",
  "workflow": "pre_print_resin_fill",
  "status": "success_inferred",
  "reason": "autoload_resin_check_then_transition_to_preheating",
  "blocking_print": false
}
```

### Pendant print

Cinq événements `feedResin code=1501` sont observés pendant l’impression.

```json
[
  {
    "time": "13:06:36",
    "layer_context": "597 -> 598",
    "code": 1501,
    "meaning": "bottle_empty_or_autoload_source_unavailable",
    "vat_empty": false,
    "blocking_print": false
  },
  {
    "time": "13:33:04",
    "layer_context": "1065 -> 1066",
    "code": 1501,
    "meaning": "bottle_empty_or_autoload_source_unavailable",
    "vat_empty": false,
    "blocking_print": false
  },
  {
    "time": "14:12:40",
    "layer_context": "1761 -> 1762",
    "code": 1501,
    "meaning": "bottle_empty_or_autoload_source_unavailable",
    "vat_empty": false,
    "blocking_print": false
  },
  {
    "time": "14:41:32",
    "layer_context": "2269 -> 2270",
    "code": 1501,
    "meaning": "bottle_empty_or_autoload_source_unavailable",
    "vat_empty": false,
    "blocking_print": false
  },
  {
    "time": "14:55:54",
    "layer_context": "2520 -> 2521",
    "code": 1501,
    "meaning": "bottle_empty_or_autoload_source_unavailable",
    "vat_empty": false,
    "blocking_print": false
  }
]
```

### Conclusion cas 06/05

```json
{
  "date": "2026-05-06",
  "pre_print_resin_fill": "success_inferred",
  "runtime_feedResin_errors": 5,
  "runtime_error_code": 1501,
  "bottle_status": "empty_or_unavailable",
  "vat_empty_observed": false,
  "failDetection_observed": false,
  "print_blocked": false,
  "final_print_state": "finished"
}
```

---

## 10. Points à ne pas faire

Ne pas faire :

```text
feedResin code=1501 => cuve vide
```

Ne pas faire :

```text
feedResin state=start => remplissage réussi
```

Ne pas faire :

```text
feedResin error pendant print => print bloqué
```

Règle correcte :

```text
feedResin doit toujours être interprété avec la phase machine.
```

---

## 11. Résumé opérationnel minimal

```text
Avant preheating :
- auto-load résine actif + passage en preheating
  => remplissage / contrôle pré-print réussi.

Avant preheating :
- feedResin erreur + pas de passage en preheating
  => erreur résine bloquante probable.

Pendant printing :
- feedResin code=1501
  => bouteille/source auto-load vide ou indisponible.
  => ne pas conclure cuve vide.
  => non bloquant tant que les couches continuent.

Pendant printing :
- feedResin erreur + failDetection / print failed / print stopped
  => suspicion cuve vide ou échec matière.
  => bloquant.
```

