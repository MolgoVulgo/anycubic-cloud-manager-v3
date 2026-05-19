# UI / QML

Statut : `PARTIEL`. L’UI principale existe ; certains dialogs restent draft/debug ou incomplets produit.

## Position produit

L’UI est une control room cloud-first : fichiers cloud, imprimantes, session, impression distante avec garde-fous, état MQTT, logs debug, entrée future viewer Photon/PWMB.

## Vues actives

| Vue | Statut | Rôle |
| --- | --- | --- |
| Main Window | Implémentée | Navigation, header, session/actions, onglets. |
| Files | Implémentée | Fichiers cloud, refresh, détails, download/delete, entrée print. |
| Printers | Implémentée | Dashboard imprimantes, détails, jobs, garde-fous print. |
| Logs | Debug | Tail JSONL et filtres en build `ACCLOUD_DEBUG=ON`. |
| Session Settings | Implémentée | Import HAR, cible session, sécurité, validation/save. |
| File Details | Implémentée | Métadonnées fichier/slicing/cloud lisibles. |
| Upload / print direct | Partiel/draft | Non central tant que backend incomplet. |
| Viewer PWMB | Partiel/draft | Structure UI présente, pipeline produit non fermé. |

## Système visuel

Palette papier chaude, panneaux arrondis, bordures douces, accent teal. Rôles stables : primary/action, danger, warning, ok/success, diagnostic/logs monospace. Le thème peut changer les valeurs, pas le sens.

## Décision onglets

Chaque zone majeure a un onglet stable. Les pages cachées ne doivent pas conserver des refresh lourds. Les panes coûteux doivent être lazy. Logs/debug sont gated.

## Files

Flux cache-first puis refresh cloud async : afficher cache, rafraîchir cloud, mettre à jour modèle, afficher statut inline déterministe. Pas d’appels bloquants depuis QML.

## Printers

Contenu obligatoire : identité imprimante, machine type, status cloud, état MQTT, task id actif, stage/progress, résine/autoload, compatibilité, reason messages. Le flux print doit identifier le fichier sélectionné, vérifier compatibilité, envoyer order, puis observer MQTT.

## Logs

Debug-only. En production, remplacer par panneau explicatif. Filtres : level, source, component, event, `op_id`, recherche texte. Buffer borné.

## Overflow

Toute zone longue doit scroller : texte, logs, metadata, détails imprimante, dumps JSON, listes.

## Règles UI

Texte visible via i18n. Payload technique autorisé en debug. Pas de logique métier dans QML. Erreurs visibles avec contexte et `op_id` si disponible. Pas de fallback silencieux.

## Décision

QML reste une couche interactive fine. Le chemin produit est Files -> Printers -> Print/MQTT. Les dialogs draft restent non centraux jusqu’à fermeture backend.
