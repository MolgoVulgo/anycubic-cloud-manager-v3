# I18N – Référence annexe

## 1. Objet

Ce document regroupe les **annexes d’audit i18n**.

Il complète la référence unifiée, sans en polluer la lecture.

Son rôle est opérationnel :

- inventorier les zones concernées,
- qualifier les types de chaînes,
- identifier les hotspots,
- préparer les lots de migration,
- servir de support de revue et de suivi.

Ce document n’est pas la doctrine. Il est l’outil de travail associé.

---

## 2. Nature des annexes

Les annexes d’audit couvrent quatre familles.

## 2.1 QML – propriétés UI

Chaînes visibles portées par des propriétés de composants, par exemple :

- texte de boutons,
- labels,
- titres,
- placeholders,
- infobulles,
- entêtes,
- captions visibles.

## 2.2 QML – affectations de statuts

Chaînes affectées à des variables ou zones de statut runtime, par exemple :

- `statusText`,
- `statusMessage`,
- message de progression,
- message d’erreur locale,
- résultat d’action.

## 2.3 QML – modèles et listes

Chaînes définies dans des tableaux ou structures de données visibles, par exemple :

- filtres,
- qualités,
- statuts de log,
- modes,
- options cloud,
- priorités,
- catégories.

## 2.4 C++ – candidats messages UI

Chaînes détectées côté C++ susceptibles d’être visibles pour l’utilisateur.

Attention : cet inventaire est mixte. Il contient potentiellement :

- de vrais messages utilisateur,
- des constantes techniques,
- des diagnostics,
- des traces,
- des identifiants non traduisibles.

Il doit donc être qualifié avant toute migration.

---

## 3. Volumétrie consolidée

L’audit consolidé représente **647 occurrences** candidates.

Répartition :

- **243** chaînes QML dans les propriétés UI,
- **51** affectations de messages de statut en QML,
- **13** chaînes ou ensembles dans les modèles QML,
- **340** chaînes candidates côté C++.

Lecture correcte de ces chiffres :

- ce ne sont pas 647 chaînes finales à traduire telles quelles,
- c’est un volume de matière d’audit,
- une partie est dupliquée,
- une partie n’est pas traduisible,
- une partie relève du nettoyage et non de la traduction.

---

## 4. Hotspots identifiés

## 4.1 Vues QML les plus exposées

Les vues ou composants les plus denses en chaînes visibles sont :

- `MainWindow.qml`
- `PrinterPage.qml`
- `CloudFilesPage.qml`
- `FileCard.qml`
- `SessionSettingsDialog.qml`
- `UploadDraftDialog.qml`
- `ViewerDraftDialog.qml`
- `PrintDraftDialog.qml`

Ces fichiers doivent être considérés comme les cibles prioritaires du lot UI statique et du lot statuts visibles.

## 4.2 Hotspots statuts runtime

Les messages de statut dynamiques sont particulièrement concentrés dans :

- `CloudFilesPage.qml`
- `PrinterPage.qml`
- `MainWindow.qml`
- `SessionSettingsDialog.qml`

Ce sont des zones à fort risque de :

- concaténation libre,
- duplication,
- divergence de formulation,
- mélange de langues,
- gestion partielle du changement de langue.

## 4.3 Hotspots C++

Le corpus C++ contient les plus fortes ambiguïtés.

On y trouve probablement :

- vrais messages fonctionnels,
- messages d’erreur utilisateur,
- traces de validation,
- messages de cache,
- messages de session,
- littéraux techniques non destinés à l’UI.

Le travail ici n’est pas d’extraire en masse. Le travail est de classer.

---

## 5. Taxonomie de qualification

Chaque élément d’audit doit être classé avant modification.

## 5.1 Type A — UI statique

Critères :

- visible à l’écran en permanence ou dans une action simple,
- dépend d’un composant visuel,
- ne porte pas de logique métier complexe.

Action :

- intégrer au flux i18n standard QML.

## 5.2 Type B — Message fonctionnel dynamique

Critères :

- affiché suite à une action,
- peut contenir des paramètres,
- informe sur un succès, un échec, un état ou une progression.

Action :

- normaliser,
- supprimer les concaténations,
- rattacher à une clé stable.

## 5.3 Type C — Libellé de modèle

Critères :

- appartient à une liste d’options,
- a une valeur métier associée,
- peut être utilisé dans un filtre ou une sélection.

Action :

- séparer `value` et `label`.

## 5.4 Type D — Technique non traduisible

Critères :

- utile au debug,
- structure interne,
- champ de stockage,
- identifiant,
- libellé de protocole,
- code ou texte machine.

Action :

- exclure de l’i18n,
- documenter éventuellement comme non traduisible.

## 5.5 Type E — Cas ambigu

Critères :

- pourrait être visible dans certains cas,
- usage non confirmé,
- message mixte entre diagnostic et UX.

Action :

- revue manuelle obligatoire,
- décision de reclassement avant modification.

---

## 6. Problèmes observés par famille

## 6.1 QML statique

Problèmes attendus ou probables :

- texte visible codé en dur,
- mélange anglais/français,
- absence de centralisation des formulations,
- micro-variantes inutiles d’un même libellé,
- labels non alignés entre pages et dialogues.

## 6.2 QML statuts

Problèmes attendus ou probables :

- affectation directe à `statusText` ou équivalent,
- messages ad hoc selon la vue,
- concaténation libre,
- intégration brute d’un détail technique,
- absence d’alignement avec les messages backend.

## 6.3 QML modèles

Problèmes attendus ou probables :

- liste visible utilisée comme valeur logique,
- libellé métier servant de clé,
- impossibilité de changer de langue sans effet de bord,
- duplication de tables entre composants.

## 6.4 C++

Problèmes attendus ou probables :

- mélange de messages utilisateur et de technique,
- erreurs remontées telles quelles,
- fallback textuels dispersés,
- chaînes visibles non cartographiées côté UI,
- variabilité forte des formulations d’erreur.

---

## 7. Grille de qualification recommandée

Chaque ligne d’annexe devrait idéalement être restructurée selon la grille suivante.

| Champ | Description |
|---|---|
| Source | fichier / zone logique |
| Ligne ou repère | localisation utile |
| Chaîne détectée | texte brut |
| Famille | QML statique / QML statut / modèle / C++ |
| Type | A / B / C / D / E |
| Visible utilisateur | oui / non / incertain |
| Traduisible | oui / non / à confirmer |
| Cible | UI / backend / exclu |
| Action | migrer / refactorer / exclure / fusionner |
| Priorité | haute / moyenne / basse |
| Statut | à traiter / validé / rejeté |

Cette grille évite de transformer l’audit en simple dump sans décision.

---

## 8. Priorisation de l’annexe

## 8.1 Priorité haute

À traiter en premier dans l’annexe :

- toutes les chaînes visibles des vues principales,
- tous les statuts runtime des pages critiques,
- tous les dialogues d’action utilisateur,
- toutes les erreurs de session, réseau, transfert et cache visibles.

## 8.2 Priorité moyenne

- options de filtres,
- modèles secondaires,
- messages rares mais visibles,
- textes affichés en debug léger mais accessibles à l’utilisateur standard.

## 8.3 Priorité basse

- constantes ambiguës non confirmées,
- textes techniques jamais exposés dans le parcours nominal,
- restes historiques non reliés à une vue active.

---

## 9. Règles de traitement par annexe

## 9.1 Annexe QML propriétés UI

Objectif :

- extraire les chaînes visibles statiques,
- fusionner les doublons évidents,
- aligner les formulations communes,
- garantir la traduisibilité sans impact métier.

## 9.2 Annexe QML statuts

Objectif :

- recenser chaque message dynamique visible,
- identifier sa source logique,
- décider s’il doit devenir une clé i18n locale ou un messageKey backend,
- supprimer les concaténations non conformes.

## 9.3 Annexe QML modèles

Objectif :

- isoler toutes les listes visibles,
- séparer valeur et libellé,
- empêcher qu’un label localisé pilote la logique fonctionnelle.

## 9.4 Annexe C++ candidats UI

Objectif :

- trier strictement visible vs non visible,
- extraire les familles de messages utilisateur réelles,
- identifier les points de refactorisation backend,
- exclure tout ce qui relève de l’interne.

---

## 10. Décisions de fusion recommandées

## 10.1 Ce qui ne doit pas être fusionné dans la référence unifiée

Ne pas fusionner directement dans le document maître :

- listes brutes de chaînes,
- dumps d’inventaire,
- résultats d’audit ligne à ligne,
- relevés ambigus non qualifiés.

## 10.2 Ce qui doit rester ici

Doivent vivre dans cette annexe :

- volumétrie d’audit,
- hotspots,
- taxonomie de qualification,
- tableau de suivi des chaînes,
- backlog d’assainissement des vues,
- liste des cas ambigus à arbitrer.

---

## 11. Format cible de stockage des annexes

Organisation recommandée :

```text
docs/
  i18n/
    I18N-Reference-Unifiee.md
    I18N-Reference-Annexe.md
    audit/
      qml-props-inventory.md
      qml-status-inventory.md
      qml-models-inventory.md
      cpp-message-candidates.md
```

Les annexes détaillées peuvent être produites sous forme de tableaux Markdown ou d’exports outillés, à condition de garder la grille de qualification commune.

---

## 12. Critères de qualité des annexes

Une annexe est exploitable si :

- les lignes sont qualifiées,
- les faux positifs techniques sont identifiés,
- les hotspots sont visibles,
- les priorités sont explicites,
- les décisions de traitement sont traçables.

Une annexe est mauvaise si :

- elle n’est qu’une liste brute,
- elle ne distingue pas visible et technique,
- elle ne donne aucun ordre de traitement,
- elle ne permet pas de construire un backlog cohérent.

---

## 13. Utilisation attendue

Ce document doit servir à :

- préparer les lots de modification,
- piloter les revues i18n,
- suivre la couverture réelle,
- arbitrer les zones ambiguës,
- vérifier qu’aucune zone visible n’a été oubliée.

Il est donc complémentaire de la référence unifiée, mais ne la remplace pas.

---

## 14. Résumé exécutif

La matière d’audit existe déjà en quantité suffisante.

Le besoin n’est pas d’ajouter encore des listes.
Le besoin est de :

- qualifier,
- trier,
- prioriser,
- relier l’inventaire aux décisions d’architecture.

Cette annexe est le support prévu pour cette étape.

