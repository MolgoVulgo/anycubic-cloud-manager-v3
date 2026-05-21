# Runtime MQTT

Statut : `IMPLEMENTE` pour le runtime actuel, `PARTIEL` pour la couverture exhaustive des modèles.

## Rôle

MQTT est le canal temps réel. HTTP/cloud initie ou resynchronise ; MQTT observe état imprimante, transitions de job, progression, checks, erreurs et retours asynchrones.

Découpage : HTTP = compte/cloud/commandes/resync ; MQTT = live printer/job ; cache = fallback et accélération UI.

## Connexion et TLS

Variables : `ACCLOUD_MQTT_TLS_CA_PATH`, `ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH`, `ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH`, `ACCLOUD_MQTT_TLS_ALLOW_INSECURE`, `ACCLOUD_MQTT_TLS_DEV_FALLBACK`, `ACCLOUD_MQTT_OPENSSL_CONF_PATH`.

Contraintes : mTLS requis, mode runtime `slicer`, compatibilité OpenSSL 3 possible, vérification stricte si supportée, jamais de clés/certificats loggés.

## États runtime

`Disconnected`, `Connecting`, `Connected`, `Subscribed`, `Degraded`, `Reconnecting`. `Connected` seul ne suffit pas : sans souscriptions utiles, le runtime n’est pas opérationnel.

## Topics importants

| Famille | Sens |
| --- | --- |
| `status/report` | État global machine, disponibilité, busy/free. |
| `print/report` | Transitions print, progression, préparation, échec, fin. |
| `releaseFilm/report` | Télémetrie film/lift/release. |
| `autoOperation/report` | Checks automatiques, opérations hardware/autoload. |
| `wifi/report` | État réseau/Wi-Fi. |

## Pipeline parsing

```text
message MQTT brut -> classification topic -> parsing JSON -> extraction action/state/type -> mapping domaine -> store realtime -> overlay UI -> observation inconnue si non supporté
```

Un message inconnu ne doit pas casser le store. Il doit être capturé avec topic, sample redacted, signature, disposition, fréquence et dernier timestamp.

## Clés payload importantes

`action`, `state`, `type`, `taskid`, progression, layers, check maps, identifiants imprimante, codes erreur et reason messages. Une clé seule ne suffit pas : il faut conserver topic + phase + taskid.

## Arbitration HTTP/MQTT/cache

HTTP est autoritaire pour resync complet. MQTT est autoritaire pour les transitions live. Le cache est autoritaire uniquement comme fallback étiqueté. Un état MQTT obsolète ne doit pas écraser un resync HTTP plus récent.

## Décision

MQTT n’est pas un simple log viewer. C’est une source d’état temps réel. L’UI principale affiche l’état normalisé ; le brut reste en debug/diagnostic.
