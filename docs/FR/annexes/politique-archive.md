# Annexe — Politique d’archive

Statut : `IMPLEMENTE`.

L’ancien paquet documentaire contenait des documents utiles, une capture HAR et un échantillon PWMB. L’archive documentaire conserve les docs techniques et les données d’analyse MQTT non secrètes, mais exclut les fixtures sensibles ou binaires.

Exclus :

- `uc.makeronline.com.har` — peut contenir tokens, cookies, URLs signées.
- `cube.pwmb` — fixture binaire utile aux tests, pas une documentation.

Si ces fichiers sont nécessaires, les garder dans un emplacement privé de tests et documenter seulement leurs métadonnées/goldens attendus.
