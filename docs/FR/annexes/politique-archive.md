# Annexe — Politique d’archive

Statut : `IMPLEMENTE`.

L’archive source distribuable contient le code, les tests, la configuration de build, le packaging et la documentation maintenue. Les données publiques de référence sont limitées à de petites fixtures synthétiques vérifiables dans un diff normal.

## Matière exclue

Ne jamais inclure :

- captures HAR, sessions, tokens, cookies ou URLs signées ;
- clés privées TLS ou fichiers d’environnement locaux ;
- captures brutes du broker MQTT ou historiques complets d’activité utilisateur ;
- identifiants réels persistants d’imprimante, tâche, message ou nom de fichier ;
- fixtures binaires privées dont la redistribution n’est pas établie ;
- outputs de build, logs runtime, caches et bases locales.

Lorsqu’une preuve privée est nécessaire à l’investigation, elle reste hors du dépôt distribuable. La documentation conserve uniquement la conclusion redacted, des statistiques agrégées ou une reproduction synthétique.

## Fixtures publiques

Les fichiers sous [`../../reference-data/`](../../reference-data/README.md) sont synthétiques. Ils expliquent le vocabulaire du parser et du workflow, mais ne prouvent pas un comportement universel du broker.

`tools/check_documentation_contract.py` reste disponible comme garde-fou séparé du dépôt pour la documentation maintenue et les contrats MQTT/SSL figés.

## Archive source pour revue web

Modifier `ARCHIVE_NAME` au début de `make-a.sh`, puis exécuter `./make-a.sh`. Le script crée l’archive à la racine sans lancer le contrat documentaire.

`acm.zip` reste une base projet fournie manuellement. Un agent ne doit pas le régénérer ou le remplacer automatiquement.
