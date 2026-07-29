# Performance UI — annexe technique

> Statut : RÉFÉRENCE. Le runtime actif et les règles principales sont décrits dans `../05-interface-qml.md`.


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
6. MQTT/logs bornés par visibilité. **Implémenté :** le modèle de diagnostic MQTT regroupe les mises à jour masquées et le poller de logs ne tourne que lorsque la page est active/visible.
7. Modèles C++ ou deltas incrémentaux. **Implémenté pour les sélections imprimantes :** imprimantes compatibles et fichiers cloud/locaux utilisent désormais des modèles C++ à identité stable avec deltas de lignes.
8. Documentation des conventions async.

## Définition de terminé

Fenêtre interactive < 1 s, onglets sans gel, cache affiché vite, refresh async, onglets cachés sans travail lourd, aucun appel QML production bloquant réseau/SQLite/log scan/gros buffer.

## Décision

Priorité à la suppression du travail bloquant GUI avant micro-optimisation QML.
