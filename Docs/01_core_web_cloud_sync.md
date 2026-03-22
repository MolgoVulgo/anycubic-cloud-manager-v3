# Core Web / Cloud Sync — documentation unifiée

Statut documentaire : `PARTIEL`
Statut global : `IMPLEMENTE + PARTIEL + SPEC`

---

## 1. Objet

Ce document unifie les notes initiales sur :
- `core_web`
- endpoints Anycubic
- import HAR/session
- logging runtime
- cache local
- stratégie de synchronisation
- plan de correction cloud

Il remplace la lecture fragmentée par une vue cohérente du sous-système cloud.

---

## 2. Position réelle du module

Le produit est aujourd’hui principalement un **client cloud Qt/QML**.

Le cœur réellement avancé est :
- accès cloud HTTP Anycubic ;
- import de session/HAR ;
- listing fichiers cloud ;
- quota ;
- dashboard imprimantes ;
- upload, téléchargement, ordre d’impression ;
- cache local SQLite ;
- bridge UI/QML fonctionnel.

Ce qui manque encore n’est pas la base fonctionnelle. Ce qui manque, c’est la fermeture propre des responsabilités.

---

## 3. État consolidé de l’architecture

### 3.1 Ce qui existe déjà et tient
- `CloudClient` et l’API legacy offrent une base fonctionnelle réelle.
- la signature `XX-*` est déjà isolée.
- l’import HAR/session est séparé.
- les use cases cloud existent et couvrent les besoins métier principaux.
- le cache local sert déjà de plan de repli pour l’UI.
- le bridge UI sait exposer les données à QML.

### 3.2 Ce qui reste trop concentré
Trois zones sont encore trop épaisses :

#### a. la couche cloud legacy
Elle mélange encore :
- transport HTTP ;
- headers signés ;
- endpoints ;
- parsing ;
- mapping ;
- parfois agrégation.

#### b. `CloudBridge`
Il reste trop chargé pour une simple couche d’adaptation UI.
Il contient encore ou orchestre encore :
- décisions de refresh ;
- usage direct du cache ;
- enrichissements de données ;
- logique d’assemblage orientée runtime.

#### c. la sync
Le système sait charger, cacher, rafraîchir et retenter, mais son contrat de synchronisation n’est pas totalement fermé.

---

## 4. Lecture cible recommandée

Le sous-système cloud doit être compris en 5 couches.

### 4.1 Domaine
Responsabilité : contrats stables.

À porter ou stabiliser :
- session cloud ;
- erreurs cloud ;
- fichiers cloud ;
- imprimantes ;
- projets/jobs ;
- quota ;
- compatibilité ;
- accusés de réception d’ordre ;
- états de sync.

### 4.2 Infra cloud core
Responsabilité : mécanique commune.

À y centraliser :
- client HTTP ;
- construction des requêtes ;
- signature ;
- registre d’endpoints ;
- parsing d’enveloppe réponse ;
- provider de session ;
- mapping des erreurs transport.

### 4.3 APIs spécialisées
Responsabilité : un endpoint logique par API claire.

Découpage recommandé :
- auth ;
- files ;
- quota ;
- printers ;
- projects ;
- downloads ;
- print order ;
- catalogues secondaires.

### 4.4 Use cases applicatifs
Responsabilité : orchestration métier.

Ils portent :
- l’ordre des appels ;
- les fallback ;
- les décisions de cache ;
- les flux multi-endpoints ;
- les résultats typés pour le bridge.

### 4.5 Bridge UI
Responsabilité : adaptation QML, rien de plus.

Il doit :
- convertir vers `QVariantMap` / `QVariantList` ;
- exposer des signaux ;
- déclencher des use cases ;
- rester agnostique du détail HTTP, session, sync et cache.

---

## 5. Endpoints et politique documentaire

### 5.1 Règle de base
La documentation endpoints doit distinguer :
- contrat supposé ;
- contrat observé ;
- comportement effectivement codé.

### 5.2 Ce qui est retenu
- les endpoints capturés et vérifiés servent de base opérationnelle ;
- les captures HAR et reports servent d’appui de validation ;
- les snapshots d’endpoints ne valent pas spécification finale sans vérification croisée avec le code.

### 5.3 Politique recommandée
Pour chaque endpoint documenté, garder :
- but ;
- auth requise ;
- méthode ;
- paramètres ;
- réponse utile ;
- mapping code actuel ;
- niveau de confiance.

---

## 6. Session / HAR

### 6.1 Rôle réel
L’import HAR/session sert à injecter un contexte d’authentification exploitable sans recréer tout le login Anycubic.

### 6.2 Lecture consolidée
Ce mécanisme est un vrai point fort du projet, mais il doit être lu comme :
- une source de session technique ;
- pas comme une logique métier ;
- pas comme une responsabilité du bridge UI.

### 6.3 Cible
La session doit être fournie par un provider unique, capable de :
- charger la session courante ;
- signaler les états invalides ;
- produire un contexte prêt pour les appels cloud ;
- supprimer toute lecture dispersée de `session.json`.

---

## 7. Logging runtime

### 7.1 Ce qui est acquis
Le runtime possède déjà une base de logging sérieuse :
- logger structuré ;
- redaction ;
- rotation ;
- distinction debug/runtime.

### 7.2 Règle de conservation
Le logging cloud/MQTT doit rester :
- structuré ;
- redigé sur les secrets ;
- exploitable pour diagnostic ;
- découplé des messages utilisateur.

### 7.3 Point d’attention
Ne pas laisser les couches UI fabriquer leur propre sémantique technique de logs. La logique de diagnostic doit rester côté app/infra.

---

## 8. Cache local

### 8.1 Rôle réel
Le cache SQLite joue déjà trois rôles :
- accélération de démarrage ;
- repli local ;
- mémoire de sync.

### 8.2 Ce qu’il fait bien
- conserver fichiers, imprimantes, jobs, quota ;
- fournir un chargement initial rapide ;
- stocker l’état minimal des synchronisations.

### 8.3 Limite actuelle
Le cache est encore utilisé comme support technique pratique, mais pas encore comme composant pleinement gouverné par une politique de sync explicite.

---

## 9. Problème central : la sync est fonctionnelle mais pas fermée

Le vrai sujet n’est pas l’existence d’erreurs de dev. Le vrai sujet est que la mécanique de sync reste **partiellement inachevée**.

### 9.1 Resync plus diagnostic que reconstructif
Aujourd’hui, le resync vérifie plusieurs sous-flux et remonte un état synthétique.

Ce qu’il ne fait pas encore de manière assez forte :
- reconstruire l’état local de bout en bout ;
- invalider proprement les scopes réellement cassés ;
- republier un état cohérent comme résultat d’orchestration.

Le resync se rapproche donc d’un contrôle enrichi, pas encore d’un orchestrateur de reconstruction.

### 9.2 Fraîcheur centrée sur le dernier succès
La décision de refresh s’appuie surtout sur :
- le dernier succès ;
- un TTL ;
- une éventuelle demande forcée.

Ce qui manque :
- le poids des échecs récents ;
- la distinction entre donnée encore affichable et donnée réellement saine ;
- la mémoire des échecs consécutifs.

### 9.3 Dépendances entre scopes
Des rafraîchissements restent couplés alors qu’ils devraient être syncables séparément.

Conséquence :
- une panne locale peut empêcher un autre scope d’être mis à jour ;
- les états partiels sont moins lisibles ;
- l’orchestration devient difficile à raisonner.

---

## 10. Modèle cible de synchronisation

### 10.1 Un scope = une vérité opérationnelle
Chaque scope doit avoir son état propre :
- `files`
- `quota`
- `printers`
- `jobs`
- autres scopes utiles si le code les formalise plus tard.

### 10.2 Métadonnées minimales par scope
À conserver :
- dernier essai ;
- dernier succès ;
- dernier échec ;
- compteur d’échecs ;
- dernier message d’erreur ;
- état logique.

### 10.3 États recommandés
- `never_synced`
- `syncing`
- `fresh`
- `stale`
- `failed`
- `partial`

### 10.4 Règle produit
Une donnée peut être :
- affichable ;
- mais périmée ;
- ou techniquement dégradée.

Ces statuts doivent être visibles dans le système, pas inférés implicitement.

---

## 11. Cible de correction architecturale

### 11.1 Ce qui doit sortir du bridge
- décisions de sync ;
- stratégie de cache ;
- lecture directe de session ;
- agrégation cloud multi-étapes ;
- retry métier.

### 11.2 Ce qui doit entrer dans les use cases
- orchestration multi-sources ;
- gestion des dépendances faibles entre scopes ;
- resync ciblée ;
- politique de repli ;
- production d’un résultat typé stable.

### 11.3 Ce qui doit être extrait côté infra
- pipeline web commun ;
- provider session ;
- APIs spécialisées ;
- parser d’enveloppe ;
- cartographie propre des erreurs réseau et HTTP.

---

## 12. Séquencement de correction retenu

### Phase A — Core web minimal
But : pipeline unique, sans casser la surface fonctionnelle.

### Phase B — Session/auth centralisées
But : supprimer les accès diffus à la session.

### Phase C — APIs spécialisées
But : sortir la logique endpoint de la zone legacy massive.

### Phase D — Use cases cloud renforcés
But : faire porter l’orchestration par l’application, pas par l’UI.

### Phase E — Resync et sync states
But : fermer le contrat de synchronisation.

### Phase F — Amincissement du bridge
But : ne garder qu’un adaptateur QML.

### Phase G — Alignement jobs/cache/téléchargement
But : faire disparaître les chemins ambigus et l’orchestration opportuniste éparse.

---

## 13. Résultat cible

Quand ce document sera pleinement vrai, le sous-système cloud devra se lire ainsi :

- l’infra transporte et signe ;
- les APIs spécialisées parlent le protocole ;
- les use cases composent le métier ;
- le cache supporte la disponibilité ;
- la sync produit des états explicites ;
- le bridge ne fait plus que l’adaptation UI.

C’est la colonne vertébrale recherchée.

