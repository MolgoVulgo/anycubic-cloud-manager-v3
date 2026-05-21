# Performance UI et latence

Statut : `ANALYSE` et `PLAN`.

## Symptôme

1 à 10 s avant interaction, 1 à 2 s entre onglets, listes bloquantes, réveils périodiques MQTT/logs. Ce n’est pas seulement un problème de delegates QML : trop de travail synchrone reste déclenché sur le thread GUI.

## Causes racines

Réseau depuis chemins UI, SQLite synchrone, gros `QVariantList`, pages invisibles instanciées, bindings/timers cachés, flux logs/MQTT, N+1 cache imprimantes, gros buffers texte QML.

## Startup

Le check session doit devenir asynchrone. Afficher le shell rapidement puis remplir l’état progressivement.

## Onglets

Changement d’onglet peu coûteux. Contenu lourd on demand. Onglets cachés sans refresh lourd.

## Files / Printers

Files : cache rapide puis cloud async. Actions fichier en async. Printers : cache groupé, pas de N+1 jobs, pas de blocage GUI, modèles incrémentaux.

## MQTT / logs

Streams throttlés, bornés et dépendants de visibilité. Ne pas repeindre de gros buffers à chaque event.

## Phases correction

1. Baseline et garde-fous.
2. Check session async.
3. Onglets lazy.
4. Cache async + requêtes imprimantes groupées.
5. Actions cloud async.
6. MQTT/logs bornés par visibilité.
7. Modèles C++ ou deltas incrémentaux.
8. Documentation des conventions async.

## Définition de terminé

Fenêtre interactive < 1 s, onglets sans gel, cache affiché vite, refresh async, onglets cachés sans travail lourd, aucun appel QML production bloquant réseau/SQLite/log scan/gros buffer.

## Décision

Priorité à la suppression du travail bloquant GUI avant micro-optimisation QML.
