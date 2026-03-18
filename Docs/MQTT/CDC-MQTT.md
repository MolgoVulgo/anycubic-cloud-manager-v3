1. Objet

Définir les exigences fonctionnelles et techniques pour intégrer un canal MQTT temps réel inspiré du projet hass-anycubic_cloud, afin de l’adapter proprement à l’application cible.

L’objectif n’est pas de remplacer les appels HTTP existants, mais de :

centraliser le temps réel imprimante ;

récupérer les réponses asynchrones à certaines commandes ;

unifier la gestion d’état imprimante côté application ;

découpler l’UI de la logique réseau.

2. Constat extrait du projet analysé
2.1 Rôle réel de MQTT dans le projet source

Dans le projet analysé, MQTT sert à deux usages principaux :

Recevoir les mises à jour temps réel de l’imprimante

état online/offline ;

état libre/occupée ;

progression d’impression ;

températures ;

ventilation ;

firmware / OTA ;

périphériques ;

fichiers locaux / UDisk ;

ACE / multi-color box.

Recevoir les réponses asynchrones à des commandes envoyées en HTTP

les ordres sont envoyés via POST /work/operation/sendOrder ;

la confirmation détaillée ou l’état de résultat revient ensuite en MQTT.

2.2 Conclusion d’architecture

Le modèle à reprendre est donc :

HTTP = commande / interrogation classique

MQTT = événements temps réel + retours asynchrones

Il ne faut pas faire l’erreur de transformer MQTT en canal principal de pilotage dès la v1.

3. Références techniques extraites
3.1 Broker

Hôte : mqtt-universe.anycubic.com

Port : 8883

Transport : TLS

Keepalive observé : 1200 s

Reconnexion minimale observée côté source : 5 s

3.2 Topics utilisés
Souscriptions utilisateur

Racine :

anycubic/anycubicCloud/v1/server/app/<user_id>/<user_id_md5>/...

Topics observés :

.../slice/report

.../fdmslice/report

Souscriptions imprimante

Topics observés :

anycubic/anycubicCloud/v1/printer/public/<machine_type>/<printer_key>/#

Note implémentation `accloud` (2026-03-17) :
le topic `anycubic/anycubicCloud/v1/+/public/<machine_type>/<printer_key>/#` est désactivé car redondant (doublon de flux avec `v1/printer/public/.../#` sur les imprimantes concernées).

Publication vers imprimante

Topic observé :

anycubic/anycubicCloud/v1/printer/public/<machine_type>/<printer_key>/<endpoint>

Dans l’implémentation analysée, le pilotage métier passe surtout par HTTP sendOrder. La publication MQTT ne doit donc pas être considérée comme socle obligatoire en v1.

3.3 Paramètres de session MQTT normatifs

Les paramètres suivants sont normatifs pour la v1 :

protocole MQTT : 3.1.1 ;

clean_session : true ;

keepalive : 1200 s ;

username/password reconstruits à chaque nouvelle connexion ;

session locale non persistante.

QoS normatif v1

subscribe QoS : 0 ;

publish QoS : 0 ;

aucun autre QoS n’est garanti par le contrat v1.

Retain normatif v1

publish retain : false ;

tout message entrant avec retained=true est ignoré pour la logique métier ;

sur user-topic, retained peut être journalisé en debug sans effet métier bloquant.

Garantie de récupération après reconnexion

avec clean_session=true, QoS 0 et retained non fiable, MQTT ne garantit pas le rattrapage des événements manqués ;

après chaque reconnexion réussie, l’application doit resouscrire tous les topics puis lancer un refresh REST complet.

4. Authentification MQTT observée
4.1 Préconditions

L’authentification MQTT dépend d’une authentification cloud déjà valide.

Prérequis :

token utilisateur valide ;

email utilisateur connu ;

user_id connu ;

mode d’authentification connu.

4.2 Modes supportés

Le projet source considère que MQTT est supporté pour :

mode ANDROID

mode SLICER

Le mode WEB n’est pas considéré comme support MQTT.

4.3 Construction du client_id

Le client_id est un MD5 basé sur l’email.

Règles observées :

Android : md5(email)

Slicer : md5(email + "pcf")

4.4 Construction du token MQTT
Cas Slicer

Le token MQTT est :

le token auth brut ;

encodé en UTF-8 ;

chiffré RSA PKCS#1 v1.5 avec la clé publique extraite du certificat CA MQTT ;

encodé en Base64 standard.

Cas Android

Le token MQTT est :

md5(auth_token) ;

puis hashé avec bcrypt.

4.5 Construction du username MQTT

Format observé :

user|<mqtt_app_id>|<email>|<signature_md5>

avec :

mqtt_app_id = "pcf" pour Slicer ;

mqtt_app_id = "app" pour Android ;

signature_md5 = md5(client_id + mqtt_token + client_id)

4.6 Exigence d’implémentation

L’application cible devra encapsuler cette logique dans un composant dédié :

MqttCredentialProvider

Ce composant ne devra jamais exposer le token brut à l’UI.

4.7 Profil crypto figé pour l’implémentation

Les règles suivantes sont normatives côté application :

encodage texte : UTF-8 ;

MD5 : hexadécimal lowercase, sans séparateur ;

email utilisé tel quel, sans trim implicite ni normalisation de casse automatique ;

client_id Android : md5(email) ;

client_id Slicer : md5(email + "pcf") ;

token Slicer :

source : auth_token UTF-8 ;

clé : clé publique RSA extraite du certificat CA MQTT Anycubic ;

padding : PKCS#1 v1.5 ;

sortie : Base64 standard ;

token Android :

source : md5(auth_token) ;

hash : bcrypt ;

stratégie locale recommandée : $2b$, cost 12, régénération à chaque nouvelle connexion MQTT ;

cette stratégie doit être validée en test d’interopérabilité broker.

4.8 Points à valider au premier test d’interopérabilité

Les points suivants doivent être considérés comme à confirmer par test, et non comme garanties théoriques :

acceptation exacte du cost bcrypt par le broker ;

tolérance éventuelle à plusieurs prefixes bcrypt ;

dépendance éventuelle à la chaîne exacte du certificat source utilisé pour RSA ;

comportement si email fourni avec casse différente.

4.9 Stratégie de fallback crypto Android (normative)

Profil nominal Android

source : md5(auth_token) ;

bcrypt prefix : $2b$ ;

bcrypt cost : 12.

Déclencheur du fallback

Le fallback crypto ne s’applique que si :

la session HTTP est valide ;

le broker MQTT renvoie une erreur d’authentification ;

le mode d’authentification est ANDROID.

Le fallback ne s’applique jamais sur erreur réseau ou TLS.

Matrice de fallback bornée

Profil A : $2b$, cost 12 ;

Profil B : $2a$, cost 12 ;

Profil C (compatibilité contrôlée) : $2b$, cost 10.

Règle d’application

mode normal : tentative A uniquement ;

mode compatibilité : A puis B puis C ;

aucune autre variante n’est autorisée automatiquement.

Cache du profil gagnant

le premier profil valide est mémorisé par broker + mode_auth + compte ;

tant qu’il reste valide, il est réutilisé directement.

Échec final

si A, B et C échouent alors que la session HTTP est valide :

état = AUTH_PROFILE_UNVERIFIED ;

arrêt des reconnexions agressives ;

remontée d’erreur métier ;

action manuelle ou mode debug requis.

Cas SLICER

aucun fallback bcrypt ;

RSA public key issue du certificat CA MQTT ;

padding PKCS#1 v1.5 ;

sortie Base64 standard.

5. Sécurité TLS
5.1 Ce que fait le projet analysé

Le projet charge :

un certificat client ;

une clé client ;

un certificat CA ;

mais désactive aussi :

la vérification hostname ;

la validation stricte du certificat serveur.

5.2 Exigence pour l’application cible

L’application cible devra prévoir deux modes.

Mode normal

validation TLS activée ;

vérification certificat serveur activée ;

vérification hostname activée si compatible.

Mode compatibilité / debug

assouplissement TLS uniquement si le service l’exige réellement ;

activation via option explicite ;

journalisation claire du niveau de sécurité dégradé.

5.3 Contraintes de sécurité

ne pas embarquer les secrets dans l’UI ;

ne pas afficher les identifiants MQTT en clair dans les logs ;

ne pas distribuer les artefacts privés sans contrôle ;

isoler la lecture des certificats dans un TlsMaterialProvider.

6. Contrat des messages entrants
6.1 Format général observé

Les messages MQTT sont des JSON contenant au minimum :

type

action

et souvent :

state

data

msg

timestamp

msgid

code

6.2 Envelope MQTT v1 interne

Le protocole externe ne publie pas de champ version explicite.
L’application devra donc définir une envelope interne v1.

Champs obligatoires

type: string

action: string

Champs conditionnels

state: string

data: object

msg: string

timestamp: number|string

msgid: string

code: number|string

6.3 Règles de validation

message sans type ou sans action : rejet + log warn ;

message avec type/action inconnus : ignoré + log warn ;

champs inconnus : tolérés, journalisés en debug ;

absence de state :

message invalide sur printer-topic pour tout type wire actuellement supporté ;

Liste normative (printer-topic)

state est obligatoire pour :

lastWill

user

status

ota

tempature

fan

print

multiColorBox

extfilbox

file

peripherie

Exceptions explicites hors printer-topic

.../slice/report

.../fdmslice/report

Pour ces user-topics :

state n’est pas requis ;

payload traité comme opaque tant qu’un contrat dédié n’est pas formalisé ;

aucun mapping métier bloquant en v1.

Règle d’extension

tout nouveau type wire est considéré avec state obligatoire par défaut ;

une exception n’est autorisée que si ajoutée explicitement dans une allowlist normative du parseur.

Comportement parseur

type connu + state absent sur printer-topic : InvalidEnvelope ;

type inconnu : UnknownMessage sans fermeture de session ;

si state présent mais vide (chaîne vide ou null) : traité comme absent.

payload JSON invalide :

rejet ;

compteur d’erreur incrémenté ;

session MQTT non interrompue ;

message vide ou partiel :

ignoré ;

log technique.

6.4 Politique de versioning

le protocole externe est traité comme wire v1 implicite ;

le décodeur interne possède sa propre version de mapping ;

toute extension de type/action/state doit être backward-compatible ;

aucun changement wire ne doit casser la session entière ;

les signatures de messages inconnus doivent être remontées comme dette d’intégration, pas comme crash.

6.5 Types de messages gérés

Types wire observés :

lastWill

user

status

ota

tempature

fan

print

multiColorBox

extfilbox

file

peripherie

6.6 Mapping wire -> enums internes

Les libellés wire doivent rester confinés au parseur.
Le reste du code ne travaille qu’avec des enums internes propres.

Types

lastWill -> MessageType::LastWill

user -> MessageType::User

status -> MessageType::Status

ota -> MessageType::Ota

tempature -> MessageType::Temperature

fan -> MessageType::Fan

print -> MessageType::Print

multiColorBox -> MessageType::MultiColorBox

extfilbox -> MessageType::ExternalFilamentBox

file -> MessageType::File

peripherie -> MessageType::Peripheral

États impression

downloading -> PrintState::Downloading

checking -> PrintState::Checking

preheating -> PrintState::Preheating

printing -> PrintState::Printing

pausing -> PrintState::Pausing

paused -> PrintState::Paused

resuming -> PrintState::Resuming

resumed -> PrintState::Resumed

finished -> PrintState::Finished

stoped -> PrintState::Stopped

stopping -> PrintState::Stopping

updated -> PrintState::Updated

failed -> PrintState::Failed

6.7 Mapping fonctionnel minimal à implémenter
lastWill

onlineReport / online -> imprimante online

onlineReport / offline -> imprimante offline

user

bindQuery / done -> imprimante liée à l’utilisateur

unbind / done -> imprimante non liée

status

workReport / free -> imprimante libre

workReport / busy -> imprimante occupée

ota

version firmware imprimante

progression téléchargement firmware

progression mise à jour firmware

version / progression firmware multi-color box

tempature

température hotbed actuelle

température nozzle actuelle

température hotbed cible

température nozzle cible

fan

pourcentage ventilateur courant

print

États observés à gérer :

start / downloading

start / checking

start / preheating

start / printing

pause / pausing

pause / paused

resume / resuming

resume / resumed

start / finished

start|stop / stoped|stopping

start|update / updated

start|stop / failed

getSliceParam / done

multiColorBox

récupération infos box

rafraîchissement slots

drying status

feed status

auto feed

loaded slot

extfilbox

reporting état étagères / box externes

file

liste fichiers locaux

suppression fichier local

liste fichiers UDisk

suppression fichier UDisk

cloud recommend list

peripherie

présence caméra

présence multi-color box

présence UDisk

6.8 Politique sur messages inconnus

ne jamais fermer la session MQTT à cause d’un message inconnu ;

log warn une seule fois par signature (type, action, state) ;

incrémenter un compteur de fréquence ;

conserver le payload brut en debug si l’option d’analyse est active ;

remonter ces signatures comme dette de couverture protocolaire.

7. Exigences fonctionnelles
7.1 Démarrage du canal MQTT

L’application devra :

disposer d’une session cloud valide ;

disposer des métadonnées utilisateur nécessaires ;

construire les credentials MQTT ;

ouvrir la connexion TLS ;

souscrire aux topics utilisateur ;

souscrire aux topics de chaque imprimante suivie ;

signaler à l’application que le temps réel est opérationnel.

7.2 Gestion multi-imprimantes

L’application devra supporter plusieurs imprimantes simultanément.

Chaque imprimante devra être identifiée au minimum par :

printer_id

machine_type

printer_key

Le routage MQTT devra se faire par printer_key extrait du topic.

7.3 Routage des messages

Tout message MQTT devra être :

décodé JSON ;

classé user topic ou printer topic ;

associé à l’imprimante cible si applicable ;

dispatché vers un handler métier par type/action/state ;

converti en événement interne applicatif.

7.4 Mise à jour de l’état interne

Le canal MQTT devra mettre à jour un store central, sans passage direct par l’UI.

Le store devra contenir au minimum :

disponibilité réseau imprimante ;

état libre / occupée ;

état impression ;

progression téléchargement ;

progression impression ;

températures actuelles / cibles ;

vitesse ventilateur ;

firmware / OTA ;

périphériques présents ;

fichiers locaux / UDisk ;

état multi-color box.

7.5 Corrélation avec les commandes HTTP

Certaines commandes continueront à être envoyées en HTTP.

Le système devra permettre :

d’émettre la commande HTTP ;

de récupérer le msgid retourné par le backend ;

d’attendre un retour MQTT corrélé ou un changement d’état cohérent ;

de notifier succès / échec / timeout à la couche métier.

7.6 Clé de corrélation normative
Clé primaire

msgid

Source HTTP

sendOrder -> data.msgid

Source MQTT

champ top-level msgid du payload

Fallback si msgid absent côté MQTT

Le fallback n’est autorisé que si aucune clé primaire n’est disponible.

Chaque commande HTTP doit déclarer une correlation_class locale stable (ex: PrintStart, PrintPause, PrintResume, PrintStop, ListLocalFiles, ListUdiskFiles, DeleteLocalFile, DeleteUdiskFile, PeripheralQuery).

Clé de fallback

printer_id

correlation_class

Le triplet type/action/state ne sert que de signature attendue, pas de clé primaire.

Règle stricte de disambiguïsation

Le fallback n’est autorisé que si, au moment de réception MQTT, il existe exactement une commande ouverte pour printer_id + correlation_class dans la fenêtre valide.

Sinon :

0 candidat : message non corrélé ;

plus de 1 candidat : message ambigu, non corrélé.

Interdiction normative

pour les commandes susceptibles d’ack sans msgid, une seule commande en vol est autorisée par printer_id + correlation_class ;

une deuxième commande identique doit être rejetée ou mise en file locale, jamais envoyée en parallèle en mode fallback.

Politique d’ambiguïté

aucun auto-matching ;

log warn ;

état de corrélation = AmbiguousFallback ;

les commandes restent ouvertes jusqu’au timeout ou jusqu’à réception d’un msgid.

7.7 Politique de corrélation

un msgid ne peut produire qu’un seul résultat terminal ;

un message reçu après état terminal pour le même msgid est considéré comme doublon ;

les doublons identiques sont ignorés ;

les doublons contradictoires sont journalisés en anomalie ;

les messages hors ordre sont acceptés tant que la commande n’est pas expirée ;

le premier état terminal reçu fait foi ;

si plusieurs événements non terminaux arrivent, l’état interne peut progresser sans clore la commande ;

une commande corrélée par fallback ne peut être clôturée qu’une seule fois et ferme immédiatement sa correlation_class.

7.8 Timeouts par commande

Le timeout ne doit pas être global unique.
Il doit être défini par profil de commande.

Valeurs recommandées

commande d’information simple : 10 s

commande de contrôle standard : 15 s

pause / reprise / stop / start impression : 30 s

opérations lourdes : 60 s

Règles

timeout atteint sans message terminal :

résultat = Timeout

commande retirée de la table des corrélations ;

si un message tardif arrive après timeout :

il peut mettre à jour l’état imprimante ;

il ne rouvre pas la commande expirée.

7.9 Politique de connexion

L’application devra proposer :

mode connecté permanent ;

mode connecté uniquement si écran imprimante ouvert ;

mode manuel ;

mode jamais connecté.

7.10 Politique de déconnexion

L’application devra pouvoir :

se déconnecter proprement à la fermeture ;

se déconnecter après inactivité configurable ;

redémarrer la connexion sur action utilisateur ou besoin métier.

7.11 Reconnexion automatique

En cas de coupure réseau :

tentative de reconnexion automatique ;

réapplication des credentials ;

resouscription à tous les topics ;

restauration de l’état de suivi.

7.12 Stratégie de reconnexion normative
Paramètres

délai initial : 5 s

backoff : exponentiel x2

plafond : 60 s

jitter : ±20 %

reset du backoff après 5 min de stabilité

Cas réseau / TLS transitoire

reconnexion automatique ;

régénération des credentials si nécessaire ;

resouscription complète après reconnexion.

Cas auth MQTT invalide

arrêt de la boucle naïve de reconnexion ;

tentative de refresh session HTTP ;

régénération des credentials MQTT ;

nouvelle tentative contrôlée.

Cas auth invalide répété

passage en état AUTH_INVALID ;

suspension de la reconnexion automatique agressive ;

relogin ou action utilisateur requis.

7.13 Resynchronisation après reconnexion

Après reconnexion réussie, l’application devra :

resouscrire les topics utilisateur ;

resouscrire les topics imprimante ;

relancer un refresh REST complet de l’état imprimante ;

purger les corrélations expirées ;

conserver uniquement les commandes encore valides.

8. Exigences techniques d’architecture
8.1 Découpage cible recommandé
MqttSessionManager

Responsabilités :

cycle de vie connexion / déconnexion ;

gestion TLS ;

reconnexion ;

souscriptions globales.

MqttCredentialProvider

Responsabilités :

génération client_id ;

génération username ;

génération mot de passe/token MQTT.

MqttTopicBuilder

Responsabilités :

construction topics user ;

construction topics printer ;

extraction des segments métier.

MqttMessageRouter

Responsabilités :

parsing JSON ;

validation minimale ;

routage par topic ;

routage par type/action/state ;

conversion wire -> enums internes.

PrinterRealtimeStore

Responsabilités :

état consolidé par imprimante ;

snapshots immuables ou quasi immuables ;

notification de changements.

OrderResponseTracker

Responsabilités :

associer commande HTTP et retour MQTT ;

gérer timeout ;

gérer doublons / hors ordre ;

produire un résultat métier exploitable.

RealtimeEventBus

Responsabilités :

diffuser événements internes vers services / UI ;

éviter que l’UI touche MQTT directement.

8.2 Contraintes de conception

aucun widget ne parle directement au broker ;

aucun parsing JSON dans l’UI ;

aucun accès direct au token depuis les vues ;

tout log sensible doit être redacted ;

toutes les typos wire doivent être isolées dans une couche d’adaptation.

9. Exigences d’intégration dans l’application KDE / Qt
9.1 Principe

L’intégration devra être pensée pour une application Qt/C++.

9.2 Recommandations Qt

utiliser QMqttClient si le support TLS et le comportement broker sont suffisants ;

sinon encapsuler une bibliothèque MQTT plus robuste derrière une interface Qt ;

exposer les événements en signals/slots ou via un bus interne ;

ne pas bloquer le thread UI ;

exécuter le traitement réseau et le parsing dans une couche service.

9.3 API interne recommandée

Exemples d’interfaces :

startRealtime()

stopRealtime()

subscribePrinter(printerKey, machineType)

unsubscribePrinter(printerKey)

currentPrinterState(printerId)

sendHttpOrderAndWaitRealtimeAck(...)

9.4 Événements internes recommandés

mqttConnected()

mqttDisconnected(reason)

printerOnlineChanged(printerId, online)

printerBusyChanged(printerId, busy)

printStateChanged(printerId, state)

printProgressChanged(printerId, progress)

temperaturesChanged(printerId, current, target)

fanSpeedChanged(printerId, value)

firmwareUpdateChanged(printerId, progress)

localFilesChanged(printerId)

peripheralsChanged(printerId)

orderAsyncResult(orderRef, result)

10. Ce qu’il faut reprendre et ce qu’il faut éviter
10.1 À reprendre

séparation HTTP / MQTT ;

souscription user + imprimantes ;

routage par topic + type/action/state ;

callbacks d’update métier ;

reconnexion + resouscription ;

store central par imprimante ;

corrélation msgid quand disponible.

10.2 À éviter

logique MQTT dispersée dans plusieurs vues ;

dépendance directe de l’UI au JSON brut ;

désactivation TLS par défaut ;

absence de corrélation entre commande HTTP et retour MQTT ;

switch/case gigantesque sans abstraction ;

mélange des états REST et temps réel sans règle claire de priorité ;

fuite des libellés wire fautifs dans le domaine métier.

11. Règles de priorité d’état

L’application devra définir une règle simple :

MQTT prévaut pour l’instantané temps réel

HTTP prévaut pour la re-synchronisation complète

En cas d’incohérence persistante, lancer un refresh REST complet

Exemples :

online/offline : MQTT prioritaire ;

progression impression : MQTT prioritaire ;

structure complète imprimante : HTTP prioritaire ;

récupération après erreur : HTTP prioritaire.

12. Journalisation
12.1 Logs nécessaires

L’application devra journaliser :

ouverture / fermeture connexion ;

souscriptions ;

reconnexions ;

erreurs de décodage JSON ;

messages inconnus ;

messages partiellement gérés ;

transitions d’état majeures ;

doublons de corrélation ;

timeouts de corrélation HTTP / MQTT.

12.2 Redaction obligatoire

Les logs devront masquer :

token ;

email si nécessaire ;

printer_key complet ;

credentials MQTT.

13. Gestion d’erreurs

Le système devra distinguer :

erreur TLS ;

erreur auth MQTT ;

broker indisponible ;

timeout de connexion ;

timeout de réponse métier ;

payload JSON invalide ;

message inconnu ;

topic inattendu ;

imprimante non enregistrée côté client.

Chaque erreur devra produire :

un log technique ;

un état métier ;

éventuellement un signal UI non bloquant.

14. Plan de mise en œuvre
Phase 1 — Socle

implémenter MqttCredentialProvider

implémenter TlsMaterialProvider

implémenter MqttSessionManager

connecter / déconnecter / reconnecter

souscrire user + imprimantes

Phase 2 — Routage

implémenter MqttTopicBuilder

implémenter MqttMessageRouter

parser format général

valider l’envelope

router par type/action/state

mapper wire -> enums internes

Phase 3 — Store métier

implémenter PrinterRealtimeStore

propager signaux internes

brancher sur UI en lecture seule

Phase 4 — Corrélation commandes

brancher sendOrder HTTP

stocker msgid

implémenter OrderResponseTracker

gérer timeout / doublons / hors ordre

notifier succès / échec / timeout

Phase 5 — Couverture métier

Support minimal :

online/offline

free/busy

print lifecycle

températures

ventilateur

périphériques

local files / UDisk

Support étendu :

firmware OTA

ACE / multi-color box

light status

réponses slice/report user

Phase 6 — Validation interop

valider crypto Android réelle contre broker

valider crypto Slicer réelle contre broker

valider stratégie TLS compatible

valider la corrélation sur cas réels sendOrder

15. Tests obligatoires
15.1 Tests unitaires

génération client_id

normalisation MD5

génération username MQTT

génération token Android

génération token Slicer

extraction de clé RSA

parsing topic utilisateur

parsing topic imprimante

routage type/action/state

mapping wire -> enums internes

validation state obligatoire sur printer-topic + exceptions user-topic

disambiguïsation fallback stricte (exactement 1 candidat)

stratégie fallback crypto Android (A ou A->B->C selon mode)

contrainte une seule commande en vol par printer_id + correlation_class

politique retained (ignoré métier en v1)

redaction logs

15.2 Tests d’intégration

connexion broker TLS

souscription multi-imprimantes

vérification clean session=true + resubscribe complet

vérification QoS 0 en subscribe/publish

reconnexion automatique

réception online/offline

réception print update

réception local file list

réception périphériques

corrélation HTTP sendOrder -> update MQTT

contrainte une commande en vol par printer_id + correlation_class

comportement retained ignoré pour la logique métier

gestion doublon même msgid

gestion message hors ordre

15.3 Tests de robustesse

JSON malformé

topic inconnu

imprimante non enregistrée

perte connexion en impression

resouscription après reconnect

timeout sans réponse MQTT

auth invalide persistante

message inconnu avec champs supplémentaires

16. Critères d’acceptation

L’intégration sera considérée comme conforme si :

la connexion MQTT démarre après authentification cloud valide ;

chaque imprimante configurée reçoit ses updates temps réel ;

l’UI se met à jour sans appel direct au broker ;

une reconnexion restaure les souscriptions ;

les commandes HTTP asynchrones peuvent être corrélées à un retour MQTT ;

les doublons et hors ordre ne cassent pas l’état métier ;

les erreurs sont journalisées sans fuite de secrets ;

les typos wire sont confinées au parseur ;

aucune commande fallback sans msgid n’est corrélée de manière ambiguë ;

une seule commande en vol est autorisée par printer_id + correlation_class ;

les paramètres de session (MQTT 3.1.1, clean_session=true, QoS 0, retain=false) respectent la politique définie ;

un refresh REST peut re-synchroniser l’état complet en cas de dérive.

17. Décision de conception recommandée
Recommandation finale

Pour l’application cible, la stratégie recommandée est :

conserver HTTP comme canal de commande officiel ;

ajouter MQTT comme couche temps réel centralisée ;

implémenter un store d’état interne unique ;

faire de l’UI un simple consommateur d’état ;

figer une couche d’adaptation wire pour isoler les étrangetés du protocole.

Décision explicitement déconseillée

Il est déconseillé de :

faire piloter l’imprimante directement depuis l’UI via MQTT ;

dupliquer la logique de parsing dans plusieurs modules ;

considérer MQTT comme remplaçant total des endpoints REST ;

laisser le protocole wire fuiter dans les enums domaine.

18. Résumé exécutable

Le projet analysé montre une architecture viable :

REST pour envoyer les ordres

MQTT pour recevoir l’état et les retours asynchrones

La version patchée impose désormais aussi :

un profil crypto explicite ;

un contrat message formalisé ;

une corrélation HTTP/MQTT normative ;

une stratégie de reconnexion définie ;

un mapping wire -> domaine pour neutraliser les fautes et incohérences du protocole.

L’adaptation à l’application devra donc produire un module Realtime/MQTT indépendant, branché sur la session cloud existante, relié à un store métier central, et exposé à l’UI uniquement sous forme d’événements et d’états consolidés.
