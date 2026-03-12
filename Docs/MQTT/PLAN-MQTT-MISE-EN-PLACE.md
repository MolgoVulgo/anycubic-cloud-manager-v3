1. Objectif

Définir un plan d’implémentation concret pour intégrer le canal MQTT décrit dans `Docs/MQTT/CDC-MQTT.md`, en tenant compte des certificats ajoutés dans `accloud/resources/mqtt/tls`.

2. Entrées de référence

Documents:

`Docs/MQTT/CDC-MQTT.md`

Matériaux TLS présents:

`accloud/resources/mqtt/tls/anycubic_mqqt_tls_ca.crt`

`accloud/resources/mqtt/tls/anycubic_mqqt_tls_client.crt`

`accloud/resources/mqtt/tls/anycubic_mqqt_tls_client.key`

3. Décisions de cadrage

MQTT reste un canal temps réel/asynchrone, pas le canal principal de commande.

Les commandes continuent à passer en HTTP (`sendOrder`) et la corrélation se fait via `msgid` ou fallback normatif.

En v1, la session MQTT suit strictement le CDC: MQTT 3.1.1, `clean_session=true`, QoS 0, `retain=false`, resync REST complet après reconnexion.

4. Plan de mise en place

Phase 0 - Sécurisation des matériaux TLS

Objectif:

Empêcher les usages dangereux du fichier `.key` tout en gardant un setup reproductible en dev.

Actions:

Créer un `TlsMaterialProvider` dédié.

Interdire tout accès direct aux certificats depuis l’UI.

Charger les chemins de certificats via configuration (variables d’environnement ou settings applicatifs), avec fallback dev explicite vers `accloud/resources/mqtt/tls`.

Ajouter un mode "compat/debug" explicite pour assouplissements TLS, jamais actif par défaut.

Vérifier à l’initialisation:

fichiers lisibles;

couple client cert/key cohérent;

CA chargée.

Livrables:

API C++ `TlsMaterialProvider` (header + implémentation).

Validation runtime des chemins + logs redacted.

Critère de sortie:

Connexion TLS impossible si matériaux invalides, avec erreur métier claire.

Phase 1 - Socle de session MQTT

Objectif:

Établir une session fiable avec reconnexion contrôlée.

Actions:

Implémenter `MqttSessionManager`:

connexion/déconnexion;

reconnexion avec backoff/jitter défini dans le CDC;

resubscribe systématique après reconnexion.

Configurer les paramètres normatifs:

MQTT 3.1.1;

`clean_session=true`;

subscribe/publish QoS 0;

publish retain false.

Livrables:

Service MQTT démarrable/arrêtable depuis la couche application.

Signaux d’état `connected/disconnected/reconnecting`.

Critère de sortie:

Après coupure réseau, la session revient, resouscrit, puis déclenche un refresh REST.

Phase 2 - Credentials MQTT

Objectif:

Produire des credentials interopérables broker sans fuite de secrets.

Actions:

Implémenter `MqttCredentialProvider`:

client_id Android/Slicer;

username `user|...`;

token Android et Slicer selon CDC.

Implémenter la matrice Android bornée:

mode normal: profil A;

mode compatibilité: A -> B -> C;

échec final: `AUTH_PROFILE_UNVERIFIED`.

Mettre en cache le profil gagnant par broker/mode_auth/compte.

Livrables:

API credentials testable unitairement.

Logs sans token brut ni key material.

Critère de sortie:

Authentification MQTT stable sur comptes Android et Slicer de test.

Phase 3 - Topics, parsing et routing

Objectif:

Transformer les payloads wire en événements métier robustes.

Actions:

Implémenter `MqttTopicBuilder` et `MqttMessageRouter`.

Appliquer les règles envelope:

printer-topic connu sans `state` => `InvalidEnvelope`;

user-topics `slice/report` et `fdmslice/report` non bloquants (opaque v1);

message inconnu => `UnknownMessage`, session conservée.

Isoler les libellés wire fautifs dans une couche d’adaptation vers enums internes.

Livrables:

Routeur par `type/action/state`.

Compteurs d’erreurs de parsing et signatures inconnues.

Critère de sortie:

Aucun crash session sur payload invalide ou inconnu.

Phase 4 - Store temps réel et bus interne

Objectif:

Consolider l’état imprimante hors UI.

Actions:

Implémenter `PrinterRealtimeStore` (état par imprimante).

Publier les mises à jour via bus interne/signals Qt.

Interdire parsing JSON ou accès broker côté QML/UI.

Livrables:

State store central lisible par `CloudBridge`/UI.

Événements internes normalisés (online, busy, print, temp, fan, peripherals, files).

Critère de sortie:

L’UI se met à jour sans appel direct MQTT.

Phase 5 - Corrélation HTTP/MQTT

Objectif:

Fiabiliser le résultat métier des commandes asynchrones.

Actions:

Implémenter `OrderResponseTracker`:

corrélation primaire par `msgid`;

fallback strict par `printer_id + correlation_class`.

Appliquer la contrainte:

une seule commande en vol par `printer_id + correlation_class` si fallback possible.

Gérer:

doublons;

hors ordre;

timeouts par profil de commande;

ambiguïté fallback (`AmbiguousFallback`).

Livrables:

Résultats métier `Success/Failure/Timeout/AmbiguousFallback`.

Métriques de corrélation.

Critère de sortie:

Aucune auto-corrélation ambiguë en fallback.

Phase 6 - Validation et durcissement

Objectif:

Valider l’interop réelle et fermer les risques opérationnels.

Actions:

Exécuter les tests unitaires:

credentials;

routing;

envelope;

redaction logs;

fallback corrélation.

Exécuter les tests d’intégration:

connexion TLS broker;

multi-imprimantes;

reconnexion + resubscribe + refresh REST;

flux sendOrder -> update MQTT.

Exécuter les tests de robustesse:

JSON malformé;

topic inconnu;

auth invalide persistante;

message retained entrant (ignoré métier en v1).

Livrables:

Rapport de validation (pass/fail + anomalies).

Critère de sortie:

Tous les critères d’acceptation du CDC sont satisfaits.

5. Cible d’implémentation (arborescence proposée)

`accloud/domain/cloud/`:

types d’état et résultats métier MQTT.

`accloud/infra/cloud/`:

clients/protocoles MQTT, providers TLS/credentials, routeur, tracker.

`accloud/app/`:

intégration orchestration (`CloudBridge`, démarrage/arrêt, diffusion UI).

`accloud/tests/cloud/`:

unitaires + intégration MQTT.

6. Gestion des certificats dans ce repo

Les fichiers de `accloud/resources/mqtt/tls` servent de référence d’intégration et de test.

Règle de sécurité:

ne pas exposer le contenu du `.key` dans les logs;

ne pas injecter ces fichiers dans les artefacts de distribution sans revue sécurité explicite;

prévoir une substitution par secrets externes en CI/CD et packaging.

7. Ordre d’exécution recommandé

1. Phase 0
2. Phase 1
3. Phase 2
4. Phase 3
5. Phase 4
6. Phase 5
7. Phase 6

8. Définition de terminé

Le lot est terminé quand:

la connexion MQTT tourne en continu avec reconnexion stable;

les updates imprimante alimentent le store interne;

les commandes HTTP asynchrones sont corrélées sans ambiguïté;

les logs sont redacted;

les tests critiques passent.
