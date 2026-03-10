# Plan d'integration MQTT (core-web)

## Objectif

Integrer MQTT dans l'architecture `core-web` sans casser les flux HTTP existants:
- HTTP reste le canal de commande.
- MQTT apporte le temps reel et les retours asynchrones.
- La logique MQTT reste hors UI.

## Alignement core-web (post-refonte)

- `CloudBridge` reste un adaptateur QML fin: pas de connexion broker, pas de parsing MQTT, pas de correlation.
- L'orchestration MQTT vit dans `app/usecases/cloud` et/ou un module applicatif dedie.
- Les composants techniques MQTT vivent en `infra/mqtt/*`, mais s'integrent avec:
  - `infra/cloud/core/SessionProvider` pour le contexte session/token,
  - `infra/cloud/api/PrintOrderApi` via les usecases existants,
  - `infra/logging/Redactor` pour la protection des secrets.
- La sortie exposee a l'UI passe uniquement par un store metier (etat consolide).

## Prerequis

- Session cloud valide (`SessionProvider`).
- Certificats presents dans `accloud/resources/mqtt/tls`.
- Module Qt MQTT disponible sur la machine de build (`Qt6::Mqtt` / `Qt6MqttConfig.cmake`).
- Branche de travail: `MQTT`.

## Lot 1 - Build et dependances MQTT

Perimetre:
- Choisir et integrer la bibliotheque client MQTT C++ (Qt MQTT ou wrapper dedie).
- Declarer les dependances dans `vcpkg.json` et ajuster `accloud/CMakeLists.txt`.
- Ajouter un flag de build/feature `mqtt_v1_enabled` (desactive par defaut en prod initiale).

Livrables:
- mise a jour `accloud/vcpkg.json`
- mises a jour `accloud/CMakeLists.txt`
- option runtime/feature flag documentee.

Definition of Done:
- Build local/CI OK avec et sans MQTT active.
- Aucun impact sur les flux cloud HTTP existants.

## Lot 2 - Socle securite et configuration

Perimetre:
- Ajouter `TlsMaterialProvider` (chargement/validation CA, cert client, key).
- Ajouter config runtime des chemins TLS (env + settings), fallback dev explicite.
- Ajouter redaction des secrets dans les logs.

Livrables:
- `infra/mqtt/core/TlsMaterialProvider.h/.cpp`
- mapping config MQTT/TLS.

Definition of Done:
- Echec explicite si un materiel TLS est invalide.
- Aucune fuite de token/key dans les logs.

## Lot 3 - Session MQTT et lifecycle

Perimetre:
- Ajouter `MqttSessionManager` (connect/disconnect/reconnect).
- Respect strict CDC: MQTT 3.1.1, clean session=true, QoS 0, retain=false.
- Resubscribe automatique et trigger de refresh REST apres reconnexion.

Livrables:
- `infra/mqtt/core/MqttSessionManager.h/.cpp`
- hooks d'etat `connected/disconnected/reconnecting`.

Definition of Done:
- Reconnexion stable avec backoff/jitter.
- Refresh REST automatique apres recovery reseau.

## Lot 4 - Credentials et authentification

Perimetre:
- Ajouter `MqttCredentialProvider`.
- Support Android/Slicer selon CDC.
- Strategie Android A->B->C en mode compatibilite uniquement.

Livrables:
- `infra/mqtt/core/MqttCredentialProvider.h/.cpp`
- cache profil gagnant par broker/mode_auth/compte.

Definition of Done:
- Auth MQTT fonctionnelle sur comptes Android et Slicer de test.
- Aucun secret brut expose a l'UI.

## Lot 5 - Routing, parsing et modele evenementiel

Perimetre:
- Ajouter `MqttTopicBuilder` + `MqttMessageRouter`.
- Parser envelope et normaliser vers evenements metier.
- Gestions `InvalidEnvelope` / `UnknownMessage` non bloquantes.

Livrables:
- `infra/mqtt/routing/*`
- `domain/realtime/PrinterRealtimeEvent.*`

Definition of Done:
- Aucun crash sur payload inconnu ou invalide.
- Erreurs de parsing comptees/tracees.

## Lot 6 - Store temps reel et integration bridge

Perimetre:
- Ajouter `PrinterRealtimeStore` (etat par imprimante).
- Exposer un flux interne consomme par usecases/bridge.
- Interdire acces MQTT direct depuis QML/UI (`CloudBridge` lit le store uniquement).

Livrables:
- `app/realtime/PrinterRealtimeStore.*`
- usecase(s) de lecture d'etat consolide + adaptation `CloudBridge`.

Definition of Done:
- UI alimentee par etat interne, sans parsing MQTT cote UI.

## Lot 7 - Correlation HTTP/MQTT

Perimetre:
- Ajouter `OrderResponseTracker`.
- Correlation primaire `msgid`, fallback strict `printer_id + correlation_class`.
- Timeouts, doublons, hors-ordre, `AmbiguousFallback`.

Livrables:
- `app/usecases/cloud/OrderResponseTracker.*`
- integration `SendPrintOrderUseCase` -> tracker -> resultats metier.

Definition of Done:
- Resultat metier explicite: `Success|Failure|Timeout|AmbiguousFallback`.
- Une seule commande en vol par cle fallback si necessaire.

## Lot 8 - Resync, robustesse et observabilite

Perimetre:
- Policy de resync REST post-reconnexion (fichiers, quota, etat imprimantes).
- Metriques MQTT (connect errors, parse errors, reconnect count, pending orders).
- Logs structures avec correlation id.

Livrables:
- hooks telemetry + tableau de bord logs.

Definition of Done:
- Rattrapage fonctionnel apres outage reseau.
- Diagnostic possible sans activer des logs verbeux secrets.

## Lot 9 - Tests et rollout progressif

Perimetre:
- Tests unitaires providers/router/tracker.
- Tests integration commande HTTP + retour MQTT.
- Feature flag `mqtt_v1_enabled` et activation progressive.

Livrables:
- suite tests + scenario de rollback.

Definition of Done:
- CI verte sur tests MQTT critiques.
- Activation sur environnements pilotes avant generalisation.

## Ordre d'execution recommande

1. Lots 1-3 pour base build + securite + session stable.
2. Lot 4 pour securiser l'authentification.
3. Lots 5-6 pour brancher le temps reel metier.
4. Lot 7 pour fiabiliser les commandes asynchrones.
5. Lots 8-9 pour productionisation.

## Risques principaux

- Fragilite des profils Android MQTT selon comptes.
- Erreurs de mapping de payloads non documentes.
- Regression si UI contourne le store temps reel.

## Criteres de succes globaux

- Aucune regression sur flux HTTP existants.
- Etat imprimante temps reel coherent apres reconnexions.
- Resultats de commandes asynchrones fiables et tracables.
- Aucun couplage MQTT direct dans `CloudBridge`/QML.

## Etat applique (2026-03-10)

- Lancement MQTT en mode manuel uniquement (pas d'auto-connexion au demarrage).
- Reconnexion automatique desactivee apres erreur client MQTT, nouvel essai via bouton `Connect`.
- Separation du token MQTT (`auth_token`) du token HTTP (`XX-Token`) dans le contexte session.
- Le prefill MQTT expose explicitement le prerequis `auth_token` quand il manque.
