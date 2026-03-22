# Photon / Viewer / Formats — documentation unifiée

Statut global : `PARTIEL + SPEC`

---

## 1. Objet

Ce document regroupe :
- la structure application photons ;
- les notes sur les formats Anycubic ;
- l’état viewer réel vs cible ;
- les éléments épars présents dans la doc UI.

Il fixe une lecture nette :
- ce qui existe déjà ;
- ce qui n’est qu’architecture cible ;
- ce qui doit attendre sans polluer le client cloud.

---

## 2. Position réelle du viewer

Le viewer n’est pas encore un sous-produit engagé.

### 2.1 Existant réel
On trouve surtout :
- placeholders QML ;
- squelette `render3d` ;
- premières structures de domaine photons ;
- drivers/formats amorcés ;
- documentation de cible assez détaillée.

### 2.2 Ce qui n’existe pas encore comme produit
- pipeline viewer fermé ;
- décodage complet validé ;
- rendu 3D exploitable ;
- navigation couches/preview aboutie ;
- UX viewer stabilisée.

### 2.3 Conclusion
Le viewer doit être considéré comme un **chantier structuré mais non commencé au sens produit**.

---

## 3. Lecture consolidée des formats

### 3.1 Rôle des docs formats
Les docs formats servent à :
- cartographier les familles Anycubic ;
- poser les hypothèses de lecture ;
- croiser avec des analyses externes ;
- guider la conception des drivers.

### 3.2 Règle d’interprétation
La documentation format est de type `SPEC` tant qu’elle n’est pas démontrée par :
- un driver réel ;
- un parsing testé ;
- un résultat exploitable par le viewer ou d’autres flux.

### 3.3 Familles concernées
Le projet vise plusieurs extensions/formats :
- PWMB
- PWS
- PHZ
- PHOTONS
- PWSZ
- variantes voisines selon les captures et contraintes Anycubic

### 3.4 Position retenue
La documentation format ne doit pas sur-vendre la maturité du moteur. Elle doit rester une base d’architecture et de reverse/documentation, pas une promesse runtime non tenue.

---

## 4. Architecture cible photons

### 4.1 Domaine
Le domaine photons doit porter :
- document ;
- méta ;
- capacités ;
- couches ;
- index ;
- contrat de driver.

### 4.2 Infra photons
L’infra doit porter :
- lecteurs ;
- drivers ;
- décodeurs preview ;
- décodeurs data ;
- adaptation propre par famille de format.

### 4.3 Render3D
Le rendu doit rester un sous-système spécialisé, clairement séparé :
- upload ;
- structures GPU/GL ;
- item Qt Quick ;
- file d’upload ;
- meshless/volumique si cette piste est confirmée.

### 4.4 UI viewer
L’UI viewer ne doit arriver qu’après :
- contrats format assez solides ;
- lecture document fermée ;
- minimum de preview/rendu stable ;
- modèle de navigation clair.

---

## 5. Écart réel vs cible

### 5.1 Ce qui est avancé dans la doc
- structure des couches ;
- nomenclature des formats ;
- vision d’architecture ;
- séparation domaine / infra / rendu / UI.

### 5.2 Ce qui est faible dans le code réel
- implémentations nombreuses encore en stub ;
- rendu 3D non substantiel ;
- panes viewer placeholder ;
- pas de boucle produit viewer réellement fermée.

### 5.3 Décision documentaire
Le corpus unifié considère donc que :
- la partie photons est une **trajectoire d’architecture** ;
- la partie viewer est un **périmètre futur** ;
- la base active du projet reste le cloud client.

---

## 6. Règle de priorité projet

Tant que les couches suivantes ne sont pas stabilisées :
- core web ;
- sync ;
- UI Cloud ;
- MQTT ;
- i18n ;

le viewer ne doit pas reprendre la priorité au détriment du cœur déjà vivant.

---

## 7. Plan d’entrée recommandé pour le viewer

### 7.1 Étape 1 — lecture document unifiée
Définir un contrat minimal commun :
- ouverture de fichier ;
- détection format ;
- méta ;
- preview 2D ;
- navigation couches minimale.

### 7.2 Étape 2 — un format de référence
Choisir un format pilote, idéalement celui déjà le plus documenté et le plus proche d’un usage réel, puis fermer le flux complet dessus.

### 7.3 Étape 3 — contrat UI minimal
Créer un viewer minimal utile :
- ouverture ;
- infos de base ;
- preview ;
- navigation simple.

### 7.4 Étape 4 — extension multi-formats
Étendre seulement après validation du pipeline de référence.

---

## 8. Ce que ce document verrouille

- le viewer n’est pas à traiter comme déjà implémenté ;
- les docs format gardent une valeur forte mais de type `SPEC` ;
- le code placeholder ne vaut pas avancée produit ;
- la structure photons est pertinente, mais elle ne doit pas diluer la priorité cloud active.

---

## 9. Résultat cible

Le jour où ce sous-ensemble sera mature, il devra se lire ainsi :
- détection format fiable ;
- driver clair par famille ;
- preview réellement utile ;
- viewer minimal complet ;
- séparation nette entre lecture, rendu et UI.

Pour l’instant, ce document sert surtout à empêcher une mauvaise lecture de maturité.

