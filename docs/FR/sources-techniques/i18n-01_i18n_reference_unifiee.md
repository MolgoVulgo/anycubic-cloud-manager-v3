# I18N – Référence unifiée

Statut documentaire : `PARTIEL`

## 1. Objet

Ce document est la **référence centrale** pour l’internationalisation du projet.

Il fusionne :

- la doctrine i18n,
- l’état réel de l’implémentation,
- les règles d’architecture,
- le workflow opérateur,
- les critères de conformité.

Il ne contient pas les inventaires bruts détaillés de chaînes. Ceux-ci doivent vivre dans les annexes d’audit.

---

## 2. Objectifs

L’objectif de l’i18n n’est pas seulement de traduire des textes visibles.

L’objectif est de rendre le projet :

- cohérent sur le plan UI,
- stable sur le plan runtime,
- maintenable sur le plan architecture,
- testable sur le plan fonctionnel.

Le projet doit pouvoir :

- afficher correctement l’interface dans plusieurs langues,
- changer de langue sans comportement incohérent,
- distinguer clairement texte utilisateur, texte technique et logs,
- empêcher le retour de chaînes visibles codées en dur hors cadre défini.

---

## 3. Périmètre

## 3.1 Inclus

- chaînes visibles en QML,
- titres, boutons, labels, placeholders,
- messages de statut visibles,
- messages fonctionnels issus du backend mais destinés à l’utilisateur,
- listes et modèles QML avec libellés visibles,
- mécanisme de sélection et persistance de langue,
- extraction `.ts`, compilation `.qm`, chargement runtime.

## 3.2 Exclus

- logs techniques,
- noms de clés JSON,
- identifiants internes,
- noms de champs SQL,
- codes d’erreur internes non affichés,
- traces debug,
- payloads bruts de transport.

---

## 4. État réel du projet

## 4.1 Socle déjà présent

Le socle i18n Qt est déjà en place.

Les éléments déjà présents ou supposés établis dans le projet sont :

- chargement des traducteurs,
- coexistence traductions Qt + application,
- persistance du choix utilisateur,
- retranslation runtime,
- support build pour mise à jour et génération des catalogues,
- catalogues de base en anglais et français.

Conclusion : le socle technique de traduction existe déjà.

## 4.2 État de migration

La migration est **partielle**.

Cela implique :

- une partie de l’UI suit déjà la mécanique Qt,
- une partie du texte visible reste dispersée,
- certains messages utilisateur sont encore générés de manière opportuniste,
- la frontière entre UI, backend et technique n’est pas complètement stabilisée.

## 4.3 Problème principal

Le problème principal n’est pas l’absence de support i18n.

Le problème principal est la **gouvernance des messages** :

- dispersion documentaire,
- absence de frontière rigoureuse entre texte fonctionnel et texte technique,
- messages dynamiques construits localement,
- modèles QML mélangeant logique métier et libellés visibles,
- retour backend encore trop textuel.

---

## 5. Modèle cible

## 5.1 Trois catégories strictes

Tout texte du projet doit appartenir à l’une des trois catégories suivantes.

### A. Texte UI statique
Exemples :

- titres,
- boutons,
- labels,
- placeholders,
- noms d’onglets,
- libellés de dialogue.

Traitement :

- traduit via Qt i18n,
- défini dans les vues ou composants selon la structure UI.

### B. Message fonctionnel utilisateur
Exemples :

- confirmation,
- erreur métier,
- statut d’action,
- résultat de synchronisation,
- message de session invalide.

Traitement :

- doit être normalisé,
- ne doit pas dépendre de concaténations libres,
- doit idéalement provenir d’une clé stable + paramètres.

### C. Texte technique non traduit
Exemples :

- logs,
- payloads,
- identifiants,
- noms de colonnes,
- diagnostics machine,
- clés internes.

Traitement :

- hors i18n,
- conservé tel quel,
- jamais injecté directement comme message utilisateur sans médiation.

---

## 6. Règles d’architecture

## 6.1 Règle générale

Le texte final visible par l’utilisateur doit être déterminé par la couche UI ou par une couche de présentation explicitement dédiée.

Le backend ne doit pas être la source libre du texte final à afficher.

## 6.2 QML

Pour les chaînes visibles statiques, utiliser une stratégie compatible Qt i18n.

Exemples de formes acceptables selon le contexte du projet :

- `qsTr(...)`
- `qsTrId(...)`

Le choix exact doit être homogène à l’échelle du projet.

## 6.3 Backend vers UI

Le modèle cible pour un message fonctionnel backend est :

- `messageKey`
- `params`
- éventuellement `fallbackMessage` durant la phase de transition

Exemple logique :

```json
{
  "ok": false,
  "messageKey": "cloud.session.invalid",
  "params": {},
  "fallbackMessage": "Session invalide."
}
```

Etat implemente (partiel, 2026-03-22) :
- `CloudBridge` et `SessionImportBridge` publient desormais une enveloppe stable
  `messageKey` + `params` + `fallbackMessage` sur les reponses UI principales.
- Le champ `message` est conserve pour compatibilite transitoire avec les vues existantes.
- La migration complete des signaux runtime vers cette enveloppe reste a finaliser.

## 6.4 Concaténations interdites pour le visible

Sont à éviter pour toute chaîne affichée à l’utilisateur :

- concaténations directes,
- fabrication locale opportuniste de phrase,
- interpolation non standardisée,
- fragments de phrase issus de plusieurs couches.

Exemples non conformes :

- `"Delete failed: " + message`
- `"Printer " + name + " unavailable"`
- `"Upload completed: " + count`

## 6.5 Paramètres

Quand un message a des variables, il doit être exprimé comme :

- une clé stable,
- des paramètres nommés ou ordonnés,
- une formulation traduisible sans dépendance à l’ordre de concaténation.

## 6.6 Modèles et listes

Tout modèle visible doit dissocier :

- la valeur métier stable,
- le libellé traduisible.

Modèle cible :

- `value` : stable, non traduite,
- `label` : visible et traduisible.

Exemple logique :

```text
value = high_reliability
label = Haute fiabilité / High reliability
```

---

## 7. Règles de contenu

## 7.1 Cohérence de ton

Les messages utilisateur doivent être :

- courts,
- explicites,
- homogènes,
- sans jargon technique inutile,
- sans copier brut des erreurs internes.

## 7.2 Uniformisation des erreurs

Les erreurs visibles doivent suivre une logique commune.

Exemples de familles :

- erreur de session,
- erreur réseau,
- erreur de validation,
- erreur d’opération cloud,
- erreur de cache local,
- erreur d’accès imprimante.

Chaque famille doit avoir :

- une clé stable,
- une formulation cohérente,
- éventuellement une variante contextualisée.

## 7.3 Uniformisation des succès et statuts

Même exigence pour :

- succès d’opération,
- avertissements,
- progression,
- attente,
- refresh,
- synchronisation,
- téléchargement,
- upload.

## 7.4 Séparation UI / diagnostic

Un message utilisateur ne doit pas être un dump technique maquillé.

Exemple :

- conforme : `Téléchargement impossible.`
- complément diagnostic éventuel : détail technique visible uniquement dans zone debug ou log.

---

## 8. Workflow opérateur

## 8.1 Ajout ou modification d’une chaîne visible

Quand une chaîne visible est ajoutée ou modifiée :

1. placer la chaîne dans le cadre i18n défini,
2. lancer la mise à jour des catalogues,
3. traduire les entrées nouvelles,
4. régénérer les `.qm`,
5. rebuild,
6. vérifier le rendu runtime.

## 8.2 Catalogues

Le projet doit conserver des catalogues structurés et cohérents, au minimum pour :

- anglais,
- français.

## 8.3 Relecture fonctionnelle

Une traduction n’est validée que si :

- elle tient dans l’UI,
- elle garde le bon sens fonctionnel,
- elle ne casse pas les libellés métier,
- elle ne modifie pas une valeur logique.

---

## 9. Organisation documentaire cible

## 9.1 Document maître

Le présent document est la doctrine centrale.

Il doit contenir :

- la cible,
- les règles,
- l’état réel,
- le workflow,
- les critères d’acceptation,
- les liens vers les annexes.

## 9.2 Annexes séparées

Les inventaires détaillés doivent être déplacés dans un document dédié d’annexes d’audit.

Le document maître ne doit pas devenir une liste brute de chaînes.

## 9.3 Plan d’exécution séparé

Le plan de correction du code et des tests doit vivre dans un troisième document distinct, orienté exécution.

---

## 10. Lots de migration

## 10.1 Lot 1 — UI statique QML

Traiter en premier :

- titres,
- boutons,
- labels,
- placeholders,
- entêtes de pages,
- dialogues.

Raison : gain rapide, risque faible, visibilité immédiate.

## 10.2 Lot 2 — Statuts visibles QML

Traiter ensuite :

- `statusText`,
- messages d’erreur locale,
- feedback de chargement,
- messages de refresh,
- statuts de transfert.

Raison : forte valeur UX et forte source d’incohérence.

## 10.3 Lot 3 — Modèles QML

Refactorer les listes visibles pour séparer `value` et `label`.

Raison : éviter de lier logique métier et texte localisé.

## 10.4 Lot 4 — Backend utilisateur

Faire converger les messages backend affichés vers `messageKey + params`.

Raison : verrouiller la frontière architecture.

## 10.5 Lot 5 — Nettoyage final

Supprimer :

- les concaténations résiduelles,
- les anciennes formulations redondantes,
- les fallbacks devenus inutiles,
- les messages techniques affichés hors debug.

---

## 11. Critères de conformité

## 11.1 Conforme

Une zone est conforme si :

- tout texte visible suit la stratégie i18n définie,
- aucun libellé visible n’est injecté en dur hors cadre,
- les messages dynamiques utilisent une forme normalisée,
- les logs ne sont pas mélangés aux messages utilisateur,
- le changement de langue conserve un rendu cohérent.

## 11.2 Non conforme

Une zone est non conforme si l’on observe :

- texte visible en dur non extrait,
- message runtime concaténé,
- libellé métier utilisé comme valeur logique,
- message backend affiché tel quel sans médiation,
- mélange de langues dans une même vue,
- retour technique exposé comme message utilisateur.

---

## 12. Tests minimum attendus

## 12.1 Changement de langue

Vérifier :

- bascule anglais/français,
- prise en compte immédiate ou conforme au comportement défini,
- cohérence visuelle de l’interface principale,
- absence de textes qui restent figés dans l’ancienne langue.

## 12.2 Parcours critiques

Vérifier au minimum :

- ouverture session,
- vues principales,
- dialogues principaux,
- statuts de chargement,
- erreurs fréquentes,
- feedback de transfert cloud.

## 12.3 Frontière technique

Vérifier que :

- les logs restent non traduits,
- les identifiants techniques restent stables,
- les messages utilisateur ne sont pas des traces internes.

---

## 13. Définition of Done

La migration i18n d’une zone est considérée terminée quand :

- les chaînes visibles sont extraites et traduites,
- les messages runtime ont été normalisés,
- les modèles visibles séparent valeur et libellé,
- les catalogues sont à jour,
- les tests de langue passent,
- aucune régression UX notable n’est observée,
- aucun message technique brut n’est exposé à l’utilisateur dans le parcours nominal.

---

## 14. Décision documentaire

La structure documentaire cible doit être la suivante :

1. **I18N – Référence unifiée** : doctrine + règles + workflow + critères.
2. **I18N – Référence annexe** : inventaires et annexes d’audit.
3. **I18N – Plan de modification** : exécution des corrections code, tests et validation.

Cette séparation est obligatoire pour éviter :

- un document maître illisible,
- des inventaires perdus hors structure,
- un plan d’action confondu avec la doctrine.

---

## 15. Résumé exécutif

Le projet dispose déjà du support i18n nécessaire.

Le chantier à terminer n’est pas un chantier de “traduction simple”.
C’est un chantier de :

- normalisation des chaînes visibles,
- séparation UI / backend / technique,
- convergence documentaire,
- verrouillage par tests.

La présente référence est la source de vérité de ce cadre.
