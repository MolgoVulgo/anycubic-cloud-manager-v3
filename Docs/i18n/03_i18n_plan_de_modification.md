# I18N – Plan de modification

Statut documentaire : `SPEC`

## 1. Objet

Ce document décrit le plan d’exécution pour rendre le code conforme à la référence i18n.

Il couvre :

- les modifications à appliquer dans le code,
- l’ordre de traitement,
- les points de contrôle,
- les risques,
- les tests de non-régression,
- les critères de validation.

Ce document est orienté exécution. Il ne redéfinit pas la doctrine.

---

## 2. But cible

À l’issue des modifications, le projet doit garantir que :

- toute chaîne visible suit la stratégie i18n définie,
- les messages dynamiques sont normalisés,
- les modèles visibles dissocient logique métier et libellé,
- les messages backend affichés à l’utilisateur sont maîtrisés,
- le changement de langue ne provoque ni texte figé, ni mélange de couches, ni régression fonctionnelle.

---

## 3. Principe d’exécution

Le chantier doit être traité par lots, du moins risqué au plus structurant.

Ordre retenu :

1. UI statique QML,
2. statuts visibles QML,
3. modèles QML,
4. messages utilisateur côté C++ / backend,
5. harmonisation catalogue,
6. verrouillage par tests,
7. nettoyage final.

Ce séquencement permet :

- d’obtenir vite un gain visible,
- de limiter les régressions,
- d’éviter de refactorer le backend avant d’avoir stabilisé le front visible.

---

## 4. Préparation obligatoire

Avant toute modification :

- figer la référence unifiée,
- figer l’annexe d’audit,
- définir la convention retenue pour les clés et les messages,
- identifier les vues critiques à ne pas casser,
- établir un jeu minimal de tests manuels et automatiques.

Checklist :

- conventions i18n validées,
- nomenclature des clés validée,
- parcours critiques listés,
- backlog de chaînes priorisé,
- point de sortie clair par lot.

---

## 5. Lot 1 — Mise en conformité QML statique

## 5.1 Objectif

Supprimer le texte visible codé en dur des zones statiques les plus exposées.

## 5.2 Cibles

Priorité aux vues les plus denses :

- `MainWindow.qml`
- `PrinterPage.qml`
- `CloudFilesPage.qml`
- `FileCard.qml`
- `SessionSettingsDialog.qml`
- `UploadDraftDialog.qml`
- `ViewerDraftDialog.qml`
- `PrintDraftDialog.qml`

## 5.3 Modifications attendues

- remplacer les chaînes visibles codées en dur par des chaînes conformes au mécanisme i18n du projet,
- harmoniser les libellés équivalents,
- supprimer les doublons de formulation évidents,
- revoir les titres, boutons, sous-titres, placeholders, libellés de champs et de dialogues.

## 5.4 Risques

- oubli de chaîne visible dans une sous-vue,
- texte tronqué après traduction,
- incohérence entre composants proches,
- régression de layout.

## 5.5 Validation du lot

Le lot est terminé quand :

- les vues cibles ne contiennent plus de chaînes statiques hors stratégie autorisée,
- les catalogues sont à jour,
- l’interface principale est cohérente en FR et EN,
- aucun composant critique n’est visuellement cassé.

---

## 6. Lot 2 — Mise en conformité des statuts visibles QML

## 6.1 Objectif

Normaliser les messages dynamiques visibles dans les vues.

## 6.2 Cibles

Priorité aux zones runtime :

- `CloudFilesPage.qml`
- `PrinterPage.qml`
- `MainWindow.qml`
- `SessionSettingsDialog.qml`

## 6.3 Modifications attendues

- recenser les `statusText`, `statusMessage` et équivalents,
- remplacer les concaténations libres,
- aligner les formulations pour succès, erreur, avertissement et progression,
- décider pour chaque message s’il doit être :
  - une clé locale UI,
  - ou un message backend normalisé.

## 6.4 Règles strictes

- pas de phrase construite par juxtaposition libre,
- pas d’erreur technique brute affichée telle quelle,
- pas de mélange de langue dans un même statut,
- pas de logique métier encodée dans le texte.

## 6.5 Risques

- rupture de cas runtime rares,
- disparition d’un détail utile à l’utilisateur,
- duplication de messages proches mais non harmonisés,
- changement de sens entre ancien et nouveau message.

## 6.6 Validation du lot

Le lot est terminé quand :

- tous les statuts visibles des vues prioritaires sont cartographiés,
- les messages dynamiques sont normalisés,
- les écrans critiques restent compréhensibles en FR et EN,
- les erreurs nominales et fréquentes sont couvertes par test.

---

## 7. Lot 3 — Refactorisation des modèles QML

## 7.1 Objectif

Séparer les valeurs métier des libellés visibles.

## 7.2 Modifications attendues

Pour chaque liste, tableau ou modèle visible :

- introduire une `value` stable,
- isoler le `label` traduisible,
- supprimer toute dépendance logique à la chaîne affichée,
- centraliser les options communes quand cela réduit la duplication.

## 7.3 Cibles probables

- filtres,
- qualités,
- catégories,
- priorités,
- statuts de log,
- modes d’action,
- options d’affichage ou cloud.

## 7.4 Risques

- casse de filtres existants,
- mauvais mapping valeur/libellé,
- dépendances implicites dans le code QML ou côté backend,
- régression de tri ou de sélection.

## 7.5 Validation du lot

Le lot est terminé quand :

- aucun modèle visible ne repose sur le texte localisé comme clé logique,
- les sélections, filtres et comparaisons fonctionnent dans les deux langues,
- les libellés changent sans effet de bord métier.

---

## 8. Lot 4 — Normalisation des messages utilisateur côté C++ / backend

## 8.1 Objectif

Réduire la production de texte final utilisateur directement côté backend.

## 8.2 Approche

Pour chaque message C++ potentiellement visible :

1. confirmer qu’il est réellement exposé à l’utilisateur,
2. le classer :
   - message utilisateur,
   - message technique,
   - cas ambigu,
3. migrer les vrais messages utilisateur vers un format stable.

## 8.3 Modèle cible

Le backend ne doit plus renvoyer librement le texte final quand cela peut être évité.

Cible recommandée :

- `messageKey`
- `params`
- `fallbackMessage` transitoire si nécessaire

## 8.4 Sous-chantiers

### A. Erreurs de session et d’auth
Normaliser :

- session invalide,
- token manquant,
- token rejeté,
- session expirée,
- authentification requise.

### B. Réseau et cloud
Normaliser :

- échec de téléchargement,
- échec d’upload,
- imprimante indisponible,
- ressource introuvable,
- erreur de synchronisation.

### C. Cache et stockage local
Normaliser :

- cache indisponible,
- fichier local introuvable,
- erreur de persistance,
- échec de lecture/écriture.

## 8.5 Risques

- rupture de compatibilité entre backend et vues,
- oubli de fallback pendant la transition,
- double affichage ancien/nouveau système,
- régression de messages rares.

## 8.6 Validation du lot

Le lot est terminé quand :

- les familles de messages utilisateur backend sont identifiées,
- les cas visibles passent par une structure stable,
- les messages techniques ne sont plus affichés tels quels dans le parcours nominal,
- les fallbacks sont limités et tracés.

---

## 9. Lot 5 — Catalogues et harmonisation terminologique

## 9.1 Objectif

Nettoyer les catalogues et unifier le vocabulaire fonctionnel.

## 9.2 Modifications attendues

- fusionner les doublons de sens,
- standardiser les formulations récurrentes,
- supprimer les variantes inutiles,
- stabiliser les familles de messages,
- vérifier la cohérence FR/EN sur les termes métier.

## 9.3 Validation du lot

Le lot est terminé quand :

- les traductions d’une même notion sont homogènes,
- les familles de messages utilisent les mêmes conventions,
- les termes métier critiques sont alignés sur tout le produit.

---

## 10. Lot 6 — Tests de non-régression

## 10.1 Objectif

Empêcher le retour des chaînes visibles hors cadre et détecter les régressions de comportement.

## 10.2 Tests automatiques recommandés

### A. Tests d’extraction

Vérifier que :

- les nouvelles chaînes visibles sont bien extraites,
- aucune chaîne visible critique n’a disparu du catalogue par erreur,
- le pipeline de génération des traductions fonctionne.

### B. Tests de composants UI

Vérifier sur les composants critiques :

- rendu des titres,
- rendu des boutons,
- rendu des dialogues,
- changement de langue,
- mise à jour correcte des propriétés traduites.

### C. Tests des messages runtime

Vérifier :

- succès d’action,
- échec d’action,
- session invalide,
- erreurs réseau fréquentes,
- transitions de statut importantes.

### D. Tests backend/UI

Vérifier :

- présence de `messageKey` attendu,
- présence des paramètres attendus,
- fallback transitoire si la clé manque,
- absence d’exposition brute d’un message technique dans le parcours nominal.

## 10.3 Tests manuels minimum

Parcours à valider en FR puis EN :

- ouverture application,
- navigation principale,
- page imprimante,
- page fichiers cloud,
- ouverture et fermeture de dialogues,
- changement de langue,
- session invalide ou expirée,
- upload / download simulé ou réel selon environnement,
- affichage d’une erreur de cache ou de réseau.

## 10.4 Tests visuels

Contrôler :

- troncatures,
- débordements,
- alignements cassés,
- boutons trop courts,
- dialogues trop serrés,
- libellés incohérents entre pages.

---

## 11. Lot 7 — Nettoyage final

## 11.1 Objectif

Supprimer les vestiges de l’ancien comportement.

## 11.2 À supprimer

- concaténations visibles résiduelles,
- anciens fallbacks devenus inutiles,
- messages dupliqués,
- formulations historiques non alignées,
- mappings transitoires non utilisés,
- code mort lié à l’ancien système de message.

## 11.3 Validation du lot

Le lot est terminé quand :

- le projet n’a plus de dette visible majeure liée à l’ancien modèle,
- les messages sont gouvernés par le cadre retenu,
- les tests critiques passent,
- les parcours UX principaux sont stabilisés.

---

## 12. Stratégie de test détaillée

## 12.1 Non-régression fonctionnelle

Objectif : s’assurer qu’une traduction ou un refactor i18n ne change pas le comportement métier.

À vérifier :

- sélection d’imprimante,
- navigation cloud,
- filtres et tris,
- ouverture de dialogue,
- état de session,
- opérations de transfert,
- réactions de l’UI après réponse backend.

## 12.2 Non-régression de contenu

Objectif : s’assurer qu’un texte visible reste présent, correct et dans la bonne langue.

À vérifier :

- pas de texte vide à la place d’un libellé,
- pas de clé brute affichée à l’écran,
- pas de mélange FR/EN non voulu,
- pas de texte ancien resté figé.

## 12.3 Non-régression d’architecture

Objectif : empêcher le retour des mauvaises pratiques.

À vérifier :

- pas de nouvelles concaténations visibles non autorisées,
- pas d’utilisation d’un label localisé comme valeur logique,
- pas d’introduction de messages backend bruts dans l’UI,
- pas de logs techniques remontés comme messages utilisateur.

---

## 13. Critères d’acceptation finaux

Le chantier global est validé quand :

- les trois documents de référence sont cohérents entre eux,
- les vues prioritaires sont conformes,
- les statuts visibles sont normalisés,
- les modèles visibles ont été assainis,
- les messages utilisateur backend sont maîtrisés,
- les catalogues sont à jour,
- les tests critiques passent,
- aucun point de régression majeur n’est ouvert sur les parcours principaux.

---

## 14. Risques transverses

## 14.1 Risque documentaire

Le plan peut échouer si les règles changent en cours de migration.

Mesure :

- figer la doctrine avant travaux lourds.

## 14.2 Risque de faux positif côté C++

Beaucoup de chaînes détectées ne sont peut-être pas visibles.

Mesure :

- revue manuelle avant migration backend.

## 14.3 Risque de régression UI

Les traductions peuvent casser l’ergonomie visuelle.

Mesure :

- tests visuels systématiques sur vues prioritaires.

## 14.4 Risque de double système de message

Pendant la transition, ancien et nouveau modèle peuvent coexister de manière sale.

Mesure :

- définir explicitement les fallbacks transitoires,
- retirer rapidement les anciens chemins une fois validés.

---

## 15. Résumé exécutif

Le plan de modification doit être exécuté comme un chantier de conformité, pas comme une simple passe de traduction.

Le bon ordre est :

1. assainir le visible statique,
2. normaliser le dynamique,
3. refactorer les modèles,
4. corriger la frontière backend/UI,
5. verrouiller le tout par tests,
6. nettoyer les restes.

Le critère central n’est pas “tout est traduit”.
Le critère central est :

- le texte visible est gouverné,
- le comportement est stable,
- les régressions sont détectables,
- l’architecture ne permet plus le retour du désordre initial.

