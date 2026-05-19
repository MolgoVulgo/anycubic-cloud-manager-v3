# Workflow impression distante et résine

Statut : `PARTIEL` pour la couverture complète modèles, `IMPLEMENTE` pour les règles d’interprétation retenues.

## Modèle bout en bout

```text
Phase HTTP cloud
  -> fichier sélectionné
  -> compatibilité imprimante/fichier
  -> ordre print
  -> acceptation cloud

Phase observation MQTT
  -> téléchargement vers imprimante
  -> checks
  -> préchauffe / préparation
  -> impression
  -> fin / échec / stop
```

Un succès HTTP signifie que le cloud a accepté la commande. Le résultat réel de l’impression vient de MQTT.

## Parent de `start`

`start` seul n’a pas de sens. Son parent réel est le contexte topic + `type/action/state`. Exemples : `print/start/preheating`, `resin/feedResin/start`, `autoOperation/...`. Le parser doit conserver topic, action, state, type, taskid et phase.

## Vocabulaire résine

Cuve/VAT = réservoir de résine imprimante. Bouteille = source externe autoload. Auto-load = opération hardware qui nourrit la cuve. `feedResin` = action MQTT dépendante de la phase. `failDetection` = chemin pouvant transformer une condition résine en échec bloquant.

## Deux workflows à distinguer

### Avant préchauffe

Workflow de préparation/autoload. Si un autoload est observé puis que l’imprimante passe en préchauffe, le remplissage préalable est considéré réussi. Sinon, `feedResin` remonte normalement comme échec bloquant de préparation.

```text
autoload observé + preheating observé ensuite => préparation résine OK
```

### Pendant impression

Une tentative de remplissage ne veut pas dire que l’imprimante a calculé une quantité précise à ajouter. Sur comportement M7 Pro / M3 Pro, la machine remplit jusqu’au repère max de cuve.

Pendant print, `feedResin` ou `code=1501` est d’abord un problème source bouteille/autoload. Cela devient faute matière bloquante seulement si l’état suivant confirme échec print ou failDetection.

```text
printing actif + feedResin/code=1501 + pas d’échec print ensuite => warning source/autoload, pas échec print immédiat
```

## États internes recommandés

`unknown`, `preprint_fill_running`, `preprint_fill_ok`, `preprint_fill_failed_blocking`, `runtime_refill_requested`, `runtime_refill_warning`, `runtime_material_failure`.

## Corrélation

Corréler avec taskid, phase print, timestamp, dernier `print/report`, dernier `status/report` et dernier event résine. Ne pas attacher un event à la mauvaise tâche entre deux starts.

## Cas 06/05/2026

Le cas contient un remplissage avant préchauffe puis une erreur type bouteille vide pendant print. La transition vers préchauffe prouve que le premier remplissage a réussi. Le signal runtime ne prouve pas une cuve vide. L’échec résine ne doit être retenu que si un état ultérieur confirme failDetection ou print failed.

## Interdits

Ne pas traiter tous les `feedResin` comme bloquants. Ne pas confondre bouteille vide et cuve vide. Ne pas ignorer la phase. Ne pas fusionner autoload pré-print et refill runtime.

## Décision

L’UI affiche la résine comme état contextualisé. Elle escalade en erreur bloquante uniquement lorsque la phase et l’état imprimante le justifient.
