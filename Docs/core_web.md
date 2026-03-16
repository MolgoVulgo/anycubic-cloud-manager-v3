# core-web

## Objectif

Centraliser tous les accès web des endpoints Anycubic dans un flux unique, déterministe et réutilisable, afin de séparer clairement :

- le **transport HTTP**
- la **signature / auth / session**
- la **description des endpoints**
- le **mapping des réponses**
- l’**orchestration métier**
- l’**adaptation UI/QML**

Le but n’est **pas** de créer une classe monolithique type `MegaCloudClient`. Le but est de créer une **colonne vertébrale web unique** avec des responsabilités nettes.

---

## Constat sur l’état actuel

### Ce qui est déjà bon

- Une partie du transport Anycubic est déjà regroupée dans `accloud/infra/cloud/CloudClient.*`.
- La signature `XX-*` est isolée dans `accloud/infra/cloud/SignHeaders.*`.
- L’import HAR/session est séparé dans `accloud/infra/cloud/HarImporter.*`.
- Les bridges QML existent déjà et fournissent un point d’entrée UI fonctionnel.

### Ce qui coince

#### 1. `CloudClient` mélange trop de choses

Aujourd’hui `CloudClient` fait à la fois :

- le transport HTTP
- l’ajout des headers signés
- le choix des endpoints
- le parsing JSON
- le mapping vers des structures métier-ish
- une partie de l’agrégation de données

Résultat : ce fichier devient le centre nerveux du cloud **et** le lieu où s’entremêlent infra et logique applicative.

#### 2. `CloudBridge` reste trop épais

Le bridge UI fait encore des choses qui ne devraient pas vivre dans une couche QML/app :

- chargement direct des tokens depuis `session.json`
- retry/backoff
- orchestration de refresh
- enrichissement imprimantes par appels supplémentaires
- gestion cache SQLite
- téléchargement direct de thumbnails / signed URL
- mapping final vers `QVariantMap`

Autrement dit : le bridge n’est pas seulement un adaptateur UI, c’est un quasi orchestrateur métier + réseau.

#### 3. Duplication d’agrégation côté imprimantes

Le flux imprimantes est actuellement dispersé :

- `fetchCloudPrinters()` enrichit déjà avec `getProjects`
- `CloudBridge::fetchPrintersWithRetry()` appelle ensuite encore `fetchPrinterDetails()` et `fetchPrinterProjects()`

Donc la composition métier est éclatée entre `infra/cloud` et `app/CloudBridge`. Ça produit du N+1, de la redondance et un flux difficile à raisonner.

#### 4. Session/auth non centralisées

La session est lue à plusieurs endroits via `loadSessionFile()`. Cela crée un couplage diffus entre :

- bridge UI
- import HAR
- check auth
- appels endpoints

Il manque un **provider de session** unique.

#### 5. Les modèles domain/cloud sont trop faibles ou déconnectés

Le dossier `domain/cloud` existe, mais les vrais contrats utilisés par le flux web sont essentiellement redéfinis dans `CloudClient.h`.

Donc aujourd’hui :

- le domaine n’est pas vraiment la source de vérité
- les DTO de transport et les modèles métier se confondent

#### 6. L’architecture cible du repo n’est pas encore alignée

Le repo annonce une architecture en couches avec `infra/jobs` et `infra/cache`, mais ces modules sont encore en grande partie scaffold. En parallèle, une partie du cache réel vit dans `app/LocalCacheStore.*`.

Donc l’architecture réelle est fonctionnelle, mais encore hybride.

---

## Cible recommandée

Je recommande une architecture en **5 étages**, très simple à lire.

### 1. `domain/cloud`

Responsabilité : contrats stables, sans Qt réseau, sans JSON brut, sans `QVariantMap`.

Contenu cible :

- `CloudSession`
- `CloudRequestContext`
- `CloudError`
- `CloudFile`
- `CloudPrinter`
- `CloudPrinterDetails`
- `CloudProject`
- `CloudQuota`
- `PrinterCompatibility`
- `PrintOrderReceipt`

Le domaine décrit **ce que l’application manipule**, pas comment l’API répond.

### 2. `infra/cloud/core`

Responsabilité : noyau technique web partagé.

Contenu cible :

- `HttpClient`
- `WorkbenchRequestBuilder`
- `WorkbenchSigner`
- `EndpointRegistry`
- `RetryPolicy`
- `ResponseEnvelopeParser`
- `SessionProvider`
- `CloudTransportErrorMapper`

Cette couche doit contenir le pipeline unique de requête.

### 3. `infra/cloud/api`

Responsabilité : implémentation endpoint par endpoint, sans logique UI.

Exemples :

- `AuthApi`
- `FilesApi`
- `QuotaApi`
- `PrintersApi`
- `ProjectsApi`
- `DownloadsApi`
- `ReasonCatalogApi`
- `PrintOrderApi`

Chaque API :

- prépare la requête via `EndpointRegistry`
- passe par `HttpClient`
- parse la réponse brute
- retourne un DTO ou un résultat typé

Aucune API ne doit toucher QML, `QVariantMap`, cache UI ou messages d’interface.

### 4. `app/usecases/cloud`

Responsabilité : composition métier.

Exemples :

- `LoadCloudFilesUseCase`
- `RefreshCloudFilesUseCase`
- `LoadPrintersDashboardUseCase`
- `GetPrinterDetailsUseCase`
- `DownloadCloudFileUseCase`
- `SendPrintOrderUseCase`
- `CheckSessionUseCase`

Ici vivent :

- l’agrégation multi-endpoints
- l’ordre des appels
- les règles de fallback
- la politique cache
- les retries applicatifs si vraiment nécessaires

Le point crucial :

**toute orchestration multi-endpoints doit quitter `CloudBridge` et vivre ici.**

### 5. `app/bridges`

Responsabilité : adaptation QML pure.

`CloudBridge` doit devenir un adaptateur fin qui :

- reçoit des paramètres UI
- appelle un use case
- convertit le résultat en `QVariantMap` / `QVariantList`
- émet des signaux QML

Rien de plus. Dès qu’on voit :

- chargement de session
- retries
- backoff
- agrégation réseau
- stratégie cache

… c’est que le code n’est pas dans la bonne couche.

---

## Flux unique recommandé

### Flux standard pour chaque endpoint

1. Le bridge UI appelle un use case.
2. Le use case demande un `CloudRequestContext` au `SessionProvider`.
3. Le use case appelle une API spécialisée (`FilesApi`, `PrintersApi`, etc.).
4. L’API récupère sa définition depuis `EndpointRegistry`.
5. `WorkbenchRequestBuilder` construit l’URL, la méthode, les headers, le body.
6. `WorkbenchSigner` ajoute les headers `XX-*` si requis.
7. `HttpClient` exécute la requête.
8. `ResponseEnvelopeParser` valide HTTP + `code/msg/data`.
9. L’API retourne un résultat typé.
10. Le use case applique composition métier, fallback, cache.
11. Le bridge transforme en format UI.

### Conséquence

On obtient un seul chemin cohérent :

**UI -> UseCase -> API -> Core Web -> HTTP**

et jamais :

**UI -> session file -> retry -> infra/cloud -> cache -> download -> parse -> UI**

---

## Découpage concret conseillé

### A. Garder `SignHeaders`, mais le déplacer conceptuellement

Conserver la logique actuelle, mais la rattacher au noyau web :

- `infra/cloud/SignHeaders.*`
- devient
- `infra/cloud/core/WorkbenchSigner.*`

Raison : ce n’est pas un helper isolé, c’est une brique centrale du pipeline de requête.

### B. Transformer `CloudClient` en façade mince ou le dissoudre

Deux options valides.

#### Option 1 — recommandée

Découper `CloudClient.cpp` en plusieurs API spécialisées.

Exemple :

- `FilesApi.cpp`
- `PrintersApi.cpp`
- `ProjectsApi.cpp`
- `QuotaApi.cpp`
- `AuthApi.cpp`
- `DownloadsApi.cpp`
- `PrintOrderApi.cpp`

Et garder éventuellement un petit `CloudApiFacade` qui compose ces sous-services.

#### Option 2 — acceptable mais moins propre

Garder `CloudClient`, mais le réduire à une façade qui délègue entièrement à des sous-modules.

Dans tous les cas, le gros fichier actuel doit être décomposé. Il porte trop de responsabilités.

### C. Créer un `SessionProvider`

Responsabilité : fournir le contexte d’auth de manière unique.

Interface cible :

- `loadCurrentSession()`
- `requireAccessToken()`
- `optionalXxToken()`
- `refreshOrInvalidateIfNeeded()` plus tard si nécessaire

Cela supprime la lecture répétée de `session.json` dans les bridges.

### D. Créer un `EndpointRegistry`

Responsabilité : décrire les endpoints dans un seul endroit.

Exemple de structure :

- identifiant logique
- méthode HTTP
- path
- type d’auth
- besoin de signature `XX-*`
- content type
- politique de retry
- règles de log/redaction

Exemple logique :

- `Auth.CheckSession`
- `Auth.LoginWithAccessToken`
- `Files.List`
- `Files.ListFallback`
- `Files.Delete`
- `Files.GetSignedDownloadUrl`
- `Printers.List`
- `Printers.CompatibilityByExt`
- `Printers.CompatibilityByFileId`
- `Printers.Details`
- `Projects.ListByPrinter`
- `Reasons.Catalog`
- `Orders.Send`

Avantage immédiat : plus de chemins codés en dur éparpillés.

### E. Déplacer la composition imprimantes dans un use case dédié

Créer un use case du style :

`LoadPrintersDashboardUseCase`

Il fera :

- `PrintersApi.list()`
- puis enrichissement optionnel avec `PrinterDetailsApi.get(id)`
- puis enrichissement avec `ProjectsApi.listByPrinter(id)`
- puis fusion selon une règle unique

Et surtout :

- plus de logique d’agrégation dans `CloudBridge`
- plus d’agrégation partielle cachée dans `CloudClient`

Il faut **une seule couche responsable de la vue consolidée imprimante**.

### F. Sortir le cache de `app/LocalCacheStore`

Le cache SQLite actuel marche, mais il est au mauvais endroit architecturalement.

Cible :

- `infra/cache/sqlite/LocalCacheStore.*`

Puis un port applicatif au-dessus :

- `CloudCacheRepository`

Le use case écrit/lit le cache via ce port.

Le bridge UI ne doit pas connaître le backend cache.

### G. Isoler les téléchargements

Il y a deux flux distincts :

1. récupérer l’URL signée cloud
2. télécharger le binaire depuis l’URL signée sans headers cloud

Il faut formaliser cela dans deux composants séparés :

- `DownloadsApi::getSignedUrl(fileId)`
- `BinaryDownloader::download(url, savePath)`

Le bridge QML peut piloter la progression, mais la logique de téléchargement ne doit pas être un îlot autonome collé au bridge.

---

## Proposition d’arborescence

```text
accloud/
  domain/
    cloud/
      CloudSession.h
      CloudError.h
      CloudFile.h
      CloudPrinter.h
      CloudPrinterDetails.h
      CloudProject.h
      CloudQuota.h
      CloudRequestContext.h

  infra/
    cloud/
      core/
        HttpClient.h
        HttpClient.cpp
        WorkbenchSigner.h
        WorkbenchSigner.cpp
        WorkbenchRequestBuilder.h
        WorkbenchRequestBuilder.cpp
        EndpointRegistry.h
        EndpointRegistry.cpp
        RetryPolicy.h
        ResponseEnvelopeParser.h
        SessionProvider.h
        SessionProvider.cpp
      api/
        AuthApi.h
        AuthApi.cpp
        FilesApi.h
        FilesApi.cpp
        QuotaApi.h
        QuotaApi.cpp
        PrintersApi.h
        PrintersApi.cpp
        ProjectsApi.h
        ProjectsApi.cpp
        DownloadsApi.h
        DownloadsApi.cpp
        ReasonCatalogApi.h
        ReasonCatalogApi.cpp
        PrintOrderApi.h
        PrintOrderApi.cpp
      session/
        HarImporter.h
        HarImporter.cpp
        SessionStore.h
        SessionStore.cpp

    cache/
      sqlite/
        LocalCacheStore.h
        LocalCacheStore.cpp

  app/
    usecases/
      cloud/
        LoadCloudFilesUseCase.h
        LoadCloudFilesUseCase.cpp
        LoadPrintersDashboardUseCase.h
        LoadPrintersDashboardUseCase.cpp
        GetDownloadUrlUseCase.h
        GetDownloadUrlUseCase.cpp
        DownloadCloudFileUseCase.h
        DownloadCloudFileUseCase.cpp
        SendPrintOrderUseCase.h
        SendPrintOrderUseCase.cpp
        CheckCloudSessionUseCase.h
        CheckCloudSessionUseCase.cpp
    bridges/
      CloudBridge.h
      CloudBridge.cpp
      SessionImportBridge.h
      SessionImportBridge.cpp
```

---

## Plan de migration recommandé

## Étape 1 — Stabiliser le noyau sans casser l’UI

Objectif : ne rien casser visuellement, mais poser l’ossature.

À faire :

- créer `SessionProvider`
- créer `EndpointRegistry`
- créer `WorkbenchRequestBuilder`
- renommer conceptuellement `SignHeaders` en `WorkbenchSigner`
- encapsuler les helpers `workbenchGet/workbenchPost/workbenchPostForm` dans un vrai `HttpClient`

Résultat attendu :

- plus aucun endpoint codé “en vrac” hors registre
- une seule manière de faire un appel Workbench

### Critère de sortie

Les fonctions actuelles continuent de marcher, mais elles passent déjà toutes par le même pipeline interne.

---

## Étape 2 — Extraire les APIs spécialisées depuis `CloudClient`

Objectif : vider `CloudClient.cpp` de sa logique fourre-tout.

À faire :

- extraire `AuthApi`
- extraire `FilesApi`
- extraire `QuotaApi`
- extraire `PrintersApi`
- extraire `ProjectsApi`
- extraire `DownloadsApi`
- extraire `ReasonCatalogApi`
- extraire `PrintOrderApi`

Résultat attendu :

- chaque fichier API est petit
- chaque fichier correspond à un groupe fonctionnel d’endpoints
- les parsers JSON sont localisés

### Critère de sortie

`CloudClient.cpp` devient soit vide et supprimable, soit une façade de quelques lignes.

---

## Étape 3 — Remonter l’orchestration métier dans `app/usecases/cloud`

Objectif : sortir la logique métier du bridge UI.

À faire :

- créer `LoadCloudFilesUseCase`
- créer `LoadPrintersDashboardUseCase`
- créer `CheckCloudSessionUseCase`
- créer `SendPrintOrderUseCase`
- créer `GetDownloadUrlUseCase`

Résultat attendu :

- `CloudBridge` ne fait plus de retries
- `CloudBridge` ne lit plus directement `session.json`
- `CloudBridge` n’agrège plus lui-même les endpoints

### Critère de sortie

Les méthodes `Q_INVOKABLE` se contentent de :

- valider les paramètres UI
- appeler un use case
- convertir le résultat pour QML

---

## Étape 4 — Recentrer la vue imprimantes sur un seul orchestrateur

Objectif : supprimer la duplication de logique imprimante.

À faire :

- choisir **un seul** point responsable du dashboard imprimantes
- y intégrer : liste, détails, projet actif, fusion des statuts
- sortir complètement cette composition de `CloudBridge`
- retirer la composition implicite de `fetchCloudPrinters()` si elle est redondante

### Règle stricte

Un agrégat métier complexe ne doit être construit **qu’à un seul endroit**.

---

## Étape 5 — Déplacer le cache applicatif dans `infra/cache`

Objectif : réaligner l’implémentation sur l’architecture du repo.

À faire :

- déplacer `LocalCacheStore.*` hors `app/`
- exposer un port de cache orienté métier
- laisser les use cases décider quand lire/écrire le cache

Résultat attendu :

- le bridge UI ne manipule plus directement le cache
- le cache devient un détail d’infrastructure

---

## Étape 6 — Introduire des jobs cloud réels

Objectif : utiliser enfin `infra/jobs/Jobs_Cloud.*` pour ce qu’il annonce.

À faire :

- brancher les refresh cloud sur un orchestrateur de jobs
- gérer annulation, priorité, sérialisation éventuelle
- remplacer les `std::thread(...).detach()` par une gestion propre du cycle de vie

Résultat attendu :

- plus de threads détachés pilotés depuis le bridge
- un modèle clair pour progress / cancel / retry

---

## Ce que je conseille de ne pas faire

### 1. Ne pas centraliser dans un seul fichier géant

Créer un `CoreWebManager.cpp` de 2500 lignes serait juste une catastrophe mieux rangée.

### 2. Ne pas laisser `QVariantMap` remonter dans les couches infra/domain

`QVariantMap` doit rester un artefact UI.

### 3. Ne pas garder la lecture de session dans chaque appel

La session doit venir d’un provider unique, pas d’un accès disque diffus.

### 4. Ne pas mélanger fallback technique et fallback métier

Exemple :

- fallback `/files -> /userFiles` = technique endpoint
- enrichissement imprimante via `details + projects` = métier/app

Ces deux logiques ne vivent pas au même étage.

---

## Tests à mettre en place

## 1. Tests unitaires noyau web

### `WorkbenchSigner`

Tester :

- présence de tous les headers `XX-*`
- stabilité du format de signature
- comportement avec/sans `XX-Token`
- surcharge par variables d’environnement

### `EndpointRegistry`

Tester :

- chaque endpoint connu existe
- méthode/path/content-type corrects
- marquage “signed / bearer / unsigned” correct

### `ResponseEnvelopeParser`

Tester :

- `HTTP 2xx + code=1` => succès
- `HTTP 2xx + code!=1` => erreur métier
- `401/403` => auth invalide
- `429/5xx` => erreur retryable
- JSON invalide => erreur parse

---

## 2. Tests unitaires API

Créer des tests par groupe d’endpoints avec réponses mockées.

### `FilesApi`

Tester :

- succès sur `/files`
- fallback `/userFiles`
- parsing champs `slice_param`
- absence de fuite de signed URL dans les logs

### `PrintersApi`

Tester :

- parsing de `getPrinters`
- compatibilité par extension
- compatibilité par file_id
- parsing printer details
- parsing reason catalog

### `ProjectsApi`

Tester :

- parsing `getProjects`
- cas liste vide
- cas projet actif

### `PrintOrderApi`

Tester :

- construction du form body
- `deleteAfterPrint=true/false`
- récupération de `task_id`

---

## 3. Tests de use cases

### `LoadPrintersDashboardUseCase`

Tester :

- agrégation correcte liste + détails + projets
- stratégie de fusion cohérente des champs `progress`, `currentFile`, `layers`
- comportement quand un sous-endpoint échoue
- absence de double appel inutile

### `LoadCloudFilesUseCase`

Tester :

- lecture cache puis cloud
- invalidation
- message final cohérent

### `CheckCloudSessionUseCase`

Tester :

- session absente
- access token absent
- session valide
- fallback auth

---

## 4. Tests d’intégration

Prévoir un faux serveur ou un layer mock pour rejouer :

- listing fichiers
- quota
- imprimantes
- détails imprimante
- projets
- signed URL
- print order

Objectif : vérifier le **flux complet** sans QML.

---

## 5. Tests de non-régression UI bridge

Conserver quelques tests sur `CloudBridge`, mais uniquement pour :

- conversion vers `QVariantMap`
- émission de signaux
- gestion des paramètres QML

Il ne faut plus tester la logique cloud profonde à ce niveau.

---

## Priorités recommandées

### Priorité 1

- `SessionProvider`
- `HttpClient`
- `EndpointRegistry`
- `WorkbenchSigner`

### Priorité 2

- extraction des APIs depuis `CloudClient`
- suppression de la logique de retry dans `CloudBridge`

### Priorité 3

- use cases cloud
- déplacement cache vers `infra/cache`

### Priorité 4

- brancher `infra/jobs/Jobs_Cloud.*`
- supprimer les threads détachés

---

## Verdict

Le repo a déjà une base utile, mais il est dans un état intermédiaire :

- le réseau est **partiellement centralisé**
- la logique métier est **encore trop proche de l’UI**
- les responsabilités sont **mieux rangées qu’avant**, mais pas encore **vraiment séparées**

La bonne direction n’est pas de refaire tout le cloud. La bonne direction est de :

1. **poser un noyau web unique**
2. **extraire les APIs spécialisées**
3. **remonter l’orchestration dans des use cases**
4. **amincir radicalement `CloudBridge`**

C’est le point où l’architecture cesse d’être une promesse PowerPoint et commence à devenir du code qui ne mord pas au prochain refactor.
