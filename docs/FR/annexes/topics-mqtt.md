# Topics MQTT utilisés par le runtime actif

> Statut : annexe technique ACTIVE. Les motifs ci-dessous viennent de `MqttTopicBuilder.cpp` ; ils ne constituent pas une convention MQTT générique.

## Lecture des paramètres

- `<userId>` : identifiant du compte Anycubic ;
- `<md5(userId)>` : MD5 minuscule utilisé par le topic de rapport observé ;
- `<machineType>` : famille d'imprimante fournie par le dashboard cloud ;
- `<deviceId>` : identifiant de l'imprimante ;
- `<endpoint>` : famille de commande ou de rapport observée.

Ces identifiants doivent être masqués dans les logs, rapports et données de référence publiques.

## Abonnements de rapport utilisateur

```text
anycubic/anycubicCloud/v1/server/app/<userId>/<md5(userId)>/slice/report
```

Il transporte le rapport de slicing résine associé au compte. La famille `fdmslice/report`, propre au FDM, n'est volontairement pas souscrite par cette application.

## Abonnements imprimante par défaut

Pour chaque imprimante retournée par le dashboard cloud :

```text
anycubic/anycubicCloud/v1/printer/public/<machineType>/<deviceId>/#
```

Ce wildcard public constitue le chemin runtime résine normal. Le broker observé refuse `v1/server/printer/<machineType>/<deviceId>/#` avec un SUBACK `0x80` ; ce wildcard est donc exclu du profil nominal.

## Découverte étendue

`ACCLOUD_MQTT_EXTENDED_TOPICS=1` active des variantes observées supplémentaires pour un diagnostic contrôlé. Elles ne remplacent pas les abonnements par défaut. Ce mode réactive aussi le wildcard de compatibilité refusé ci-dessous, qui peut légitimement recevoir un SUBACK `0x80` sur les comptes résine :

```text
anycubic/anycubicCloud/v1/server/printer/<machineType>/<deviceId>/#
```

Familles commande/rapport :

```text
anycubic/anycubicCloud/v1/+/printer/<machineType>/<deviceId>/<endpoint>
```

Valeurs `<endpoint>` actives :

```text
airpure
autoOperation
axis
exposure
file
network
ota
print
releaseFilm
residual
resin
response
smartResinVat
wifi
```

Familles serveur :

```text
anycubic/anycubicCloud/v1/server/printer/<machineType>/<deviceId>/status
anycubic/anycubicCloud/v1/server/printer/<machineType>/<deviceId>/user
anycubic/anycubicCloud/v1/server/printer/<machineType>/<deviceId>/video
```

Variantes legacy encore observées sur certaines branches firmware :

```text
anycubic/anycubicCloud/+/printer/<machineType>/<deviceId>/print
anycubic/anycubicCloud/printer/public/<machineType>/<deviceId>/online/status
```

## Topic de publication

Les commandes imprimante sont publiées via :

```text
anycubic/anycubicCloud/v1/printer/public/<machineType>/<deviceId>/<endpoint>
```

L'endpoint est choisi par le use case propriétaire. QML ne doit pas construire ce topic.

## Règle de routage

Une clé ou un champ de payload ne suffit pas à identifier un événement. Le routage conserve le contexte combiné :

```text
topic + phase + action + state + type + taskid
```

Un message inconnu reste non bloquant. Il peut être conservé comme observation masquée avec signature de topic, disposition, fréquence et échantillon borné.

## Source de vérité

- construction : `src/accloud/infra/mqtt/routing/MqttTopicBuilder.cpp` ;
- routage : `src/accloud/infra/mqtt/routing/MqttMessageRouter.cpp` ;
- propriétaire des abonnements runtime : `src/accloud/app/mqtt/MqttBridgeSession.cpp` (`MqttBridge.cpp` reste la façade exposée à QML).
