# MQTT et état temps réel

## En bref

HTTP fournit un état complet. MQTT transmet les transitions live sans nouvelle synchronisation générale. Le cache local reste uniquement un fallback explicitement étiqueté.

Le broker Anycubic n'est pas un service MQTT standard interchangeable. Les paramètres ci-dessous constituent un **contrat de compatibilité figé** : ils décrivent le runtime validé contre le broker observé, pas une configuration MQTT théorique.

## Flux runtime

```text
message MQTT brut
-> classification du topic
-> parsing JSON
-> extraction action/state/type/taskid
-> événement domaine
-> PrinterRealtimeStore
-> overlay UI
```

Un message inconnu ne doit pas casser le store. Il peut être conservé comme observation redacted avec topic, signature, disposition, fréquence et dernier timestamp.

## Contrat broker figé

| Paramètre | Valeur active |
| --- | --- |
| Host | `mqtt-universe.anycubic.com` |
| Port | `8883` |
| Protocole MQTT | `3.1.1` |
| Transport | TLS chiffré |
| Version TLS | `1.2` |
| Authentification client | mTLS : CA + certificat client + clé privée |
| Mode de credentials | `slicer` |
| Keepalive | `1200` secondes |
| Clean session | `true` |

Compatibilité actuellement requise par l'environnement observé :

- `ACCLOUD_MQTT_TLS_ALLOW_INSECURE` vaut `true` par défaut ;
- Qt utilise `VerifyNone` lorsque ce mode est actif ;
- la CipherString Qt peut utiliser `ALL:@SECLEVEL=0` ;
- un profil `OPENSSL_CONF` avec `DEFAULT:@SECLEVEL=0` peut être généré ;
- `ACCLOUD_MQTT_TLS_DEV_FALLBACK` contrôle seulement la recherche locale explicite sous `<racine-du-depot>/resources/mqtt/tls` ;
- aucun matériel TLS du dépôt n’est sélectionné silencieusement lorsque ce flag est absent ;
- le lanceur du dépôt `start.sh` fixe explicitement ce flag à `1` par défaut pour les exécutions locales, tout en préservant les valeurs fournies par l’appelant et les chemins TLS explicites ;
- CA, certificat client et clé privée restent obligatoires pour une connexion normale.

`VerifyNone` et `SECLEVEL=0` sont des mesures de compatibilité propres au broker MQTT. Ils ne définissent pas la politique HTTPS de l'application et n'autorisent aucune désactivation globale de vérification.

Sans instruction explicite et revalidation broker live, ne pas modifier host, port, version MQTT, keepalive, clean session, mTLS, auth slicer, politique de cipher de compatibilité ou familles de topics observées.

## Connexion

Le runtime charge l'identité de session et le token MQTT, dérive les credentials slicer, valide le matériel TLS externe, applique les paramètres figés, se connecte puis souscrit aux familles de topics utilisateur et imprimante.

Le constructeur peut préparer l’auto-connexion de manière asynchrone. Une demande UI ultérieure ne lance pas une seconde préparation tant que celle d’arrière-plan est active ; une nouvelle tentative reste possible après sa fin si le profil n’était pas prêt.

Un générateur Android interne existe pour l'analyse de compatibilité. Le profil production reste `slicer` ; une opération de nettoyage ou de standardisation ne doit pas le remplacer par `android` ou un mode automatique.

## Topics et contexte

Le runtime résine nominal souscrit uniquement au topic de compte `slice/report` et au wildcard `v1/printer/public/<machineType>/<deviceId>/#` de chaque imprimante. Le topic `fdmslice/report`, propre au FDM, est exclu. Le wildcard `v1/server/printer/.../#`, refusé par le broker observé, reste disponible uniquement en mode de découverte étendue explicite. Les familles résine importantes comprennent `status`, `print`, `releaseFilm`, `autoOperation` et `wifi`.

Une clé isolée ne définit jamais un événement. L'interprétation conserve :

```text
topic + phase + action + state + type + taskid
```

Voir [Topics MQTT](annexes/topics-mqtt.md) pour les builders actifs et variantes legacy, et [Structures JSON MQTT](annexes/structures-json-mqtt.md) pour le vocabulaire des payloads.

## Arbitrage des états

1. HTTP est autoritaire pour un resync complet.
2. MQTT est autoritaire pour les transitions live.
3. Le cache est autoritaire seulement comme fallback étiqueté.
4. Un état MQTT ancien ne peut pas écraser un état HTTP plus récent.
5. Un échec MQTT devient visible immédiatement.
6. Une acceptation HTTP ne peut pas effacer un échec MQTT ultérieur.

## Interprétation résine

Le même vocabulaire peut représenter un autoload avant impression ou un refill pendant l'impression. La phase et la corrélation de tâche doivent être conservées. Les exemples détaillés restent en annexe.

## Validation live

Un test broker exige session valide, réseau, Qt6 MQTT, compatibilité OpenSSL et matériel mTLS externe. `acm.zip` ne contient volontairement aucune clé privée. Tout prérequis absent est qualifié **environnement incomplet**, jamais validation broker réussie.
