# Variables d'environnement runtime

> Statut : annexe technique ACTIVE. Les overrides servent au diagnostic et au déploiement ; ils ne remplacent pas les contrats Anycubic observés.

Les booléens acceptent les formes gérées par leur composant propriétaire (`1/0`, `true/false`, `yes/no`, `on/off` selon le cas).

## Chemins locaux

| Variable | Rôle |
| --- | --- |
| `ACCLOUD_PATHS_INI` | Remplace le fichier INI de résolution des chemins. |
| `ACCLOUD_SESSION_PATH` | Remplace le chemin de `session.json`. Ce fichier reste secret. |
| `ACCLOUD_DB_PATH` | Remplace le chemin du cache SQLite. |
| `ACCLOUD_THUMBNAIL_DIR` | Remplace le répertoire du cache de miniatures. |
| `ACCLOUD_LOG_DIR` | Remplace le répertoire des logs structurés. |
| `ACCLOUD_MQTT_OPENSSL_CONF_PATH` | Remplace le chemin du fichier de compatibilité OpenSSL généré. |

## Identité et credentials MQTT

| Variable | Rôle et contrainte |
| --- | --- |
| `ACCLOUD_MQTT_AUTH_MODE` | Profil de credentials préféré (`slicer`, `android` ou valeur supportée). Le runtime de production reste `slicer`. |
| `ACCLOUD_MQTT_ANDROID_COMPAT` | Active le candidat interne de compatibilité Android. Diagnostic uniquement ; il ne remplace pas le profil slicer validé. |
| `ACCLOUD_MQTT_AUTH_TOKEN` | Remplace le token MQTT chargé depuis la session. Secret. |
| `ACCLOUD_MQTT_EMAIL` | Remplace l'adresse du compte utilisée par le générateur de credentials. Donnée personnelle. |
| `ACCLOUD_MQTT_USER_ID` | Remplace l'identifiant du compte. Donnée personnelle/pseudonyme. |
| `ACCLOUD_MQTT_MODE_AUTH` | Remplace la métadonnée de session `mode_auth`, conservée pour compatibilité avec les sessions importées. |

## TLS MQTT

| Variable | Rôle et contrainte |
| --- | --- |
| `ACCLOUD_MQTT_TLS_CA_PATH` | Matériel CA utilisé par le chemin slicer/TLS. |
| `ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH` | Certificat client mTLS. |
| `ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH` | Clé privée client mTLS. Secrète ; ne jamais la packager ni la logger. |
| `ACCLOUD_MQTT_TLS_ALLOW_INSECURE` | Vaut `true` par défaut dans le chemin de compatibilité validé et autorise `VerifyNone`. C'est une compatibilité MQTT figée, pas une politique SSL globale. |
| `ACCLOUD_MQTT_TLS_DEV_FALLBACK` | Recherche explicite sous `<racine-du-depot>/resources/mqtt/tls` pour le matériel de développement. Le runtime générique la désactive par défaut ; le lanceur local `start.sh` la fixe explicitement à `1` par défaut, préserve toute valeur fournie par l’appelant et ne remplace jamais les chemins TLS explicites. |

## Observation MQTT et diagnostic UI

| Variable | Rôle |
| --- | --- |
| `ACCLOUD_MQTT_EXTENDED_TOPICS` | Ajoute des variantes de topics observées de manière contrôlée. |
| `ACCLOUD_MQTT_CAPTURE_PATH` | Écrit une capture MQTT masquée vers un chemin explicite. Ne pas utiliser avec des identifiants publics. |
| `ACCLOUD_UI_PERF_TRACE` | Active les traces de performance UI. |

## Overrides cloud de headers/signature

Ces valeurs reproduisent des headers et profils de signature observés. Les modifier peut invalider les appels cloud. Les valeurs secrètes ne doivent jamais être affichées.

| Variable courante | Alias de compatibilité | Rôle |
| --- | --- | --- |
| `ACCLOUD_PUBLIC_APP_ID` | `ACCLOUD_WORKBENCH_APP_ID` | Identifiant d'application public/Workbench. |
| `ACCLOUD_PUBLIC_APP_SECRET` | `ACCLOUD_WORKBENCH_APP_SECRET` | Secret de signature. Secret. |
| `ACCLOUD_PUBLIC_VERSION` | `ACCLOUD_WORKBENCH_VERSION` | Version client observée. |
| `ACCLOUD_PUBLIC_DEVICE_TYPE` | — | Type de device observé. |
| `ACCLOUD_PUBLIC_IS_CN` | — | Indicateur de routage régional du profil public. |
| `ACCLOUD_REGION` | `ACCLOUD_CLIENT_REGION` | Header de région client. |
| `ACCLOUD_DEVICE_ID` | `ACCLOUD_CLIENT_DEVICE_ID` | Identifiant device client. |
| `ACCLOUD_USER_AGENT` | `ACCLOUD_CLIENT_USER_AGENT` | User-Agent HTTP. |
| `ACCLOUD_CLIENT_VERSION` | — | Champ de version client complémentaire utilisé par les headers signés. |

## Les options de build ne sont pas des variables runtime

`ACCLOUD_ENABLE_QT`, `ACCLOUD_ENABLE_QML_TESTS` et `ACCLOUD_DEBUG` sont des options CMake. `ACCLOUD_WITH_QT`, `ACCLOUD_WITH_MQTT` et `ACCLOUD_WITH_OPENSSL` sont des définitions produites par le build. Elles ne doivent pas être présentées comme des overrides runtime.

## Source de vérité

- chemins : `src/accloud/infra/config/AppPaths.h` ;
- headers cloud : `src/accloud/infra/cloud/SignHeaders.cpp` ;
- contexte session : `src/accloud/infra/cloud/core/SessionProvider.cpp` ;
- credentials/TLS MQTT : `src/accloud/infra/mqtt/core/` ;
- diagnostic runtime : `src/accloud/app/MqttBridge.cpp`, `src/accloud/app/UiPerfTrace.h`.
