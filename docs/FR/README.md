# Documentation

Ce dossier est le point d’entrée français de la documentation projet. Il décrit le produit, les comportements attendus, les analyses, les décisions et les limites connues.

## Ordre de lecture

1. `01-architecture.md` — rôle du projet, sources de vérité et frontières modules.
2. `02-client-cloud.md` — comportement Anycubic Cloud, sessions, endpoints, downloads, uploads et sync.
3. `03-runtime-mqtt.md` — connexion MQTT, topics, parsing/routing et état realtime.
4. `04-workflow-impression-resine.md` — impression distante et interprétation résine/autoload.
5. `05-ui-qml.md` — shell UI, pages, dialogs, règles visuelles et décisions écrans.
6. `06-performance-ui.md` — analyse latence, causes racines et plan de correction.
7. `07-viewer-photon-formats.md` — cible viewer Photon/PWMB, parsing formats et vérité géométrique.
8. `08-i18n.md` — architecture traduction, règles et plan de migration.
9. `09-operations-securite.md` — logs, redaction, cache, chemins runtime et diagnostics.
10. `10-developpement-codex.md` — règles de travail assisté par Codex.
11. `11-decisions-points-ouverts.md` — décisions projet et points ouverts.

Les annexes contiennent le détail des écrans, les structures JSON MQTT, les extensions de fichiers et les analyses issues de captures.

## Statuts documentaires

| Statut | Sens |
| --- | --- |
| `IMPLEMENTE` | Visible dans le code actuel ou validé par un flux runtime. |
| `PARTIEL` | Démarré et utile, mais pas encore fermé comme flux produit complet. |
| `SPEC` | Contrat cible, règle de design ou comportement futur. |
| `ANALYSE` | Diagnostic ayant servi à prendre des décisions. |
| `PLAN` | Plan d’exécution ordonné. |
| `SNAPSHOT` | Matière issue de captures ou historique ; utile mais non normative seule. |

## Sources de vérité

1. Code courant du dépôt.
2. Cette documentation.
3. Annexes et analyses issues de captures.
4. Traces historiques, HAR, logs MQTT bruts et fixtures binaires.

Si une documentation contredit le code, le code gagne. Si une trace contredit une hypothèse produit, l’hypothèse doit être revue et documentée.

## Politique d’archive publique

L’archive documentaire exclut volontairement les HAR et les échantillons PWMB binaires. Les HAR peuvent contenir des tokens et URLs signées ; les PWMB sont des fixtures, pas de la documentation. Les analyses JSON/CSV MQTT peuvent être incluses lorsqu’elles ne contiennent pas de secrets.
