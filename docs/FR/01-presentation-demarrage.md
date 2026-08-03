# Présentation et démarrage

## En bref

Anycubic Cloud Manager V3 regroupe dans une interface desktop les fichiers cloud Anycubic, les imprimantes et l'état temps réel des impressions résine. L'usage courant ne demande pas de connaître HTTP, MQTT ou QML, mais la documentation conserve les détails nécessaires à la maintenance.

## État du produit

| Zone | Statut | Signification |
| --- | --- | --- |
| Shell desktop | ACTIF | Application principale Qt/QML. |
| Fichiers cloud et imprimantes | ACTIF / PARTIEL | Fonctions principales utilisables, compatibilité dépendante du cloud et du modèle. |
| MQTT temps réel | ACTIF | État live reçu selon le contrat broker Anycubic observé. |
| Impression distante | ACTIF / PARTIEL | HTTP initie la demande ; MQTT rapporte ensuite le résultat réel. |
| Viewer Photon/PWMB | EXPÉRIMENTAL | Le parcours PWSZ de développement fonctionne de bout en bout ; les autres formats et la préparation production restent partiels. |

## Parcours normal

```text
capture HAR
-> import de session
-> session.json local
-> appels cloud authentifiés
-> état initial HTTP
-> mises à jour MQTT
-> affichage QML
```

La capture HAR est utilisée car l'application ne réimplémente pas le parcours complet de connexion officiel. Le HAR et le fichier de session généré contiennent des credentials réutilisables.

## Prérequis

- environnement desktop Linux ;
- CMake 3.24 ou plus récent ;
- compilateur C++20 ;
- Qt6 Quick, Quick Controls, Network et SQL ;
- Qt6 MQTT pour le temps réel ;
- OpenSSL et le matériel mTLS requis par le broker Anycubic ;
- session Anycubic valide pour les opérations live.

L'absence de credentials ou de matériel broker signifie **environnement incomplet**, jamais validation réussie.

## Premier build

```bash
./start.sh 1
```

Ou manuellement :

```bash
cd accloud
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

`start.sh` peut lancer l'application interactive et ne constitue pas une validation automatisée.

## Importer une session

```bash
cd accloud
./build/default/accloud_cli --import-har /chemin/capture.har
```

L'importeur extrait et normalise les champs exigés par le runtime. Une session partielle échoue explicitement.

## Données runtime

Racine par défaut :

```text
~/.local/share/accloud
```

Données courantes :

```text
session.json       session réutilisable — secret
settings.ini       préférences UI/runtime
accloud_cache.db   cache local
thumbnails/        cache d'images dérivées
logs/              logs structurés redacted
tmp/               données temporaires et profil OpenSSL de compatibilité
```

## Limites courantes

- Anycubic peut modifier endpoints, signatures, payloads ou topics.
- Les champs varient selon les modèles et firmwares.
- Une commande HTTP acceptée n'est pas une confirmation finale d'impression.
- Les tests live exigent credentials, réseau et matériel mTLS contrôlés.
- Le viewer reste expérimental.

## Suite de lecture

Lire [Architecture et runtime actif](02-architecture-runtime.md) avant toute modification, puis le document cloud ou MQTT correspondant.
