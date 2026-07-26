# Architecture et runtime actif

## En bref

L'application suit une architecture C++ en couches. QML affiche l'état et transmet l'intention utilisateur ; le C++ conserve les use cases, protocoles, stockages et règles de sécurité.

```text
pages et dialogues QML
        ↓
bridges Qt et modèles UI
        ↓
use cases et store temps réel
        ↓
cloud / MQTT / cache / logs / formats
```

## Frontières des modules

| Chemin | Responsabilité |
| --- | --- |
| `src/accloud/app/` | bootstrap, bridges Qt, modèles UI et coordination des use cases |
| `src/accloud/domain/` | vocabulaire métier et contrats stables |
| `src/accloud/infra/` | cloud HTTP, MQTT, stockage, cache, logs et formats |
| `src/accloud/render3d/` | base OpenGL et intégration Qt Quick |
| `src/accloud/ui/qml/` | shell visuel, pages, dialogues et contrôles |
| `tests/` | tests de régression C++ et QML |

Une correction reste dans le module déjà propriétaire. Un problème cloud ne migre pas vers QML et un correctif cloud-only ne modifie ni MQTT ni render3d.

## Exécutable et points d'entrée

CMake construit l'exécutable partagé `accloud_cli`.

`src/accloud/app/main.cpp` sélectionne :

```text
--smoke ou --import-har
-> exécution CLI via App

aucun flag CLI et Qt disponible
-> QGuiApplication
-> création des bridges
-> qrc:/qml/MainWindow.qml
```

Le bootstrap desktop expose à QML `SessionImportBridge`, `CloudBridge`, `MqttBridge`, `UiSettingsBridge`, `AppI18nBridge` et les modèles UI enregistrés. Les objets debug n'existent que lorsque `ACCLOUD_DEBUG` est activé.

## Ressources QML

Les ressources production sont déclarées dans `src/accloud/app/resources.qrc`. Les pages debug sont séparées dans `resources_debug.qrc` et compilées uniquement dans les builds correspondants.

Une correction QML n'est valide que si le fichier est inclus dans les ressources du preset ciblé.

## Modes de build

| Preset | Build | Outils debug |
| --- | --- | --- |
| `default` | Debug | exclus |
| `dev-debug` | Debug | inclus |
| `prod` | Release | exclus |

La production ne doit jamais dépendre de `LogBridge`, `LogTailModel`, `UiClickTracer` ou de ressources QML debug-only.

## Autorité des états

```text
HTTP/cloud  = autorité de resynchronisation complète
MQTT        = autorité des transitions live
cache       = fallback explicitement étiqueté uniquement
```

Un événement MQTT obsolète ne doit pas écraser un resync HTTP plus récent. Une acceptation HTTP ne doit pas effacer un échec MQTT ultérieur.

## Thread GUI

Les opérations longues doivent être asynchrones du point de vue du thread graphique. QML ne doit pas porter les appels réseau, le parsing massif, les transactions SQLite, la génération de credentials ou les retries.

## Frontière expérimentale

Le parsing Photon/PWMB et render3d restent partiels ou expérimentaux. Voir [l'annexe viewer](annexes/viewer-photon-formats.md). Ils ne doivent pas devenir des dépendances implicites des correctifs cloud ou MQTT.
