# Annexe — Écrans client cloud

Statut : `IMPLEMENTE` pour les écrans actifs, `PARTIEL` pour les vues draft.

Main Window : control room, navigation globale, statut runtime/session, accès Files/Printers/Logs. Les actions header ne doivent pas court-circuiter le flux produit.

Files : liste fichiers cloud, refresh, métadonnées, download/delete, entrée vers print. Sections attendues : toolbar, liste/table/cards, statut inline, dialog détails, erreurs déterministes.

File Details : metadata général, slicing, cloud, URLs redacted si nécessaire.

Printers : cartes/tabs imprimantes, détails actifs, recent jobs, overlay MQTT, action print gardée. Contenu obligatoire : nom/id/key safe, machine type, status cloud, MQTT, task id, stage/progress, résine/autoload, compatibilité, reason messages.

Logs : debug-only, absent ou panneau explicatif en prod.

Session Settings : import HAR, rappel sécurité, cible session, résultat sans tokens bruts.

Draft views : Upload, direct print payload et viewer restent draft ou cachés jusqu’à fermeture backend.
