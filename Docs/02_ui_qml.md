# UI / QML — documentation unifiée

Statut documentaire : `PARTIEL`

---

## 1. Objet

Ce document fusionne la documentation initiale relative à :
- la shell UI ;
- les pages Cloud ;
- les dialogs ;
- le design system ;
- les lots UI ;
- le correctif onglets ;
- le workflow remote print ;
- les snapshots de logs UI.

Il devient la référence principale pour la partie interface Qt/QML.

---

## 2. Position réelle de l’UI

L’UI actuellement réelle n’est pas un viewer photons. C’est d’abord un **control room Cloud**.

### 2.1 Vues actives
- fenêtre principale ;
- onglet `Files` ;
- onglet `Printers` ;
- onglet `Logs` en build debug ;
- dialogs de session et d’actions cloud.

### 2.2 Vues non centrales ou draft
- dialogs draft upload/print/viewer ;
- panes viewer placeholder ;
- structure viewer encore largement vide.

### 2.3 Conclusion
L’UI avancée est le client cloud. Le viewer existe encore surtout comme cible et comme amorce de structure.

---

## 3. Architecture UI consolidée

### 3.1 Shell principal
Structure retenue :
- header ;
- navigation par onglets ;
- contenu principal ;
- dialogs secondaires.

### 3.2 Rôle du header
Le header doit rester un espace de :
- contexte de session ;
- accès réglages ;
- raccourcis debug autorisés ;
- aucune logique métier lourde.

### 3.3 Navigation principale
Les onglets applicatifs réellement utiles sont :
- `Files`
- `Printers`
- `Logs` (debug)

La navigation doit exprimer un produit centré cloud et non une promesse viewer encore absente.

---

## 4. Design system consolidé

### 4.1 Objectif visuel
Le design system vise :
- lisibilité ;
- densité contrôlée ;
- surfaces simples ;
- accent unique ;
- hiérarchie texte nette ;
- cohérence des composants transverses.

### 4.2 Tokens à considérer comme obligatoires
- couleurs de fond ;
- couleurs de texte primary/secondary/disabled ;
- bordures ;
- accent et dérivés ;
- couleurs d’état ;
- tailles typo ;
- padding, gaps, rayons, hauteur de contrôle.

### 4.3 Règle ferme
Aucun hardcode visuel nouveau ne doit être introduit hors exception explicitement documentée.

### 4.4 Composants partagés stabilisés
La bibliothèque interne doit rester le point d’entrée visuel :
- `AppButton`
- `AppTextField`
- `AppComboBox`
- `AppCheckBox`
- `AppSpinBox`
- `AppPageFrame`
- `AppDialogFrame`
- `SectionHeader`
- `StatusChip`
- `InlineStatusBar`
- `AppTabBar`
- `AppTabButton`

---

## 5. Correctif onglets — décision consolidée

Le correctif onglets n’est pas cosmétique. Il fixe une grammaire de navigation.

### 5.1 Problèmes ciblés
- effet suite de boutons ;
- rupture entre barre d’onglets et panneau ;
- doubles bordures ;
- coins incohérents ;
- micro-décalages visuels.

### 5.2 Structure retenue
Le groupe attendu est :
- une shell d’onglets ;
- une barre d’onglets ;
- un panneau de contenu ;
- une seule logique de bordure continue.

### 5.3 Règles fermes
- pas d’espacement horizontal entre tabs ;
- baseline unique ;
- onglet actif fusionné visuellement avec le contenu ;
- coins internes carrés ;
- rendu groupe, pas rendu boutons isolés.

### 5.4 Conséquence
Toute nouvelle navigation par tabs doit réutiliser `AppTabBar` et `AppTabButton` plutôt que réintroduire des variantes locales.

---

## 6. État des pages

### 6.1 Files
Page la plus dense et l’une des plus matures.

Fonctions visibles :
- toolbar ;
- listing dense ;
- filtre ;
- quota ;
- détails fichier ;
- upload ;
- téléchargement ;
- suppression ;
- amorce de remote print.

### 6.2 Printers
Page métier clé pour la logique cloud temps réel/état machine.

Fonctions visibles :
- listing imprimantes ;
- état ;
- détails ;
- jobs/projets ;
- déclenchement d’ordre à distance.

### 6.3 Logs
Vue debug liée à la nature outillage du projet.

### 6.4 Dialogs
Les dialogs jouent un rôle central et doivent rester :
- homogènes ;
- pilotés par le design system ;
- découplés du métier profond.

---

## 7. Workflow remote print consolidé

Le remote print doit être lu comme une chaîne UI simple :
1. sélection du fichier cloud ;
2. sélection/validation imprimante ;
3. options de tâche ;
4. envoi ;
5. suivi et retour utilisateur.

### 7.1 Règle produit
L’UI ne doit pas porter la logique profonde d’orchestration. Elle doit présenter :
- contexte ;
- validation ;
- état ;
- erreurs lisibles.

### 7.2 Point d’attention
Le flux remote print reste l’une des zones où l’UI peut facilement se remettre à porter du métier. C’est à éviter.

---

## 8. Dette et limites UI réelles

### 8.1 Fichiers QML lourds
Certaines pages concentrent encore :
- layout ;
- état ;
- actions ;
- mapping d’affichage ;
- réactions asynchrones.

### 8.2 Ce qui doit être surveillé
- taille des pages ;
- logique métier dérivant dans QML ;
- style encore partiellement littéral ;
- duplication de patterns de table/dialog.

### 8.3 Indicateurs de pilotage utiles
- part du style via tokens ;
- taille maximale d’une page ;
- nombre de composants mutualisés ;
- couverture QML sur navigation et dialogs critiques.

---

## 9. Cible de modularisation

### 9.1 MainWindow
Doit rester une coque.

### 9.2 Pages métier
Doivent être découpées en blocs :
- toolbar ;
- liste/table ;
- panneau détail ;
- overlays ;
- dialogs d’action.

### 9.3 Composants transverses
Doivent concentrer :
- style ;
- micro-comportements UI ;
- conventions de rendu.

### 9.4 Règle
La logique d’assemblage métier ne remonte ni dans les composants visuels, ni dans les pages plus que nécessaire.

---

## 10. Ce qui est considéré comme acquis

- la base du design system existe ;
- la shell UI est fonctionnelle ;
- les lots 1 à 5 ont déjà structuré une part importante de l’interface ;
- les onglets ont maintenant une doctrine visuelle claire ;
- le mode thème/persistance est réellement intégré ;
- l’UI Cloud est une vraie base produit.

---

## 11. Ce qui reste à faire côté UI

### 11.1 Fermer la modularisation des pages lourdes
But : réduire la densité logique dans les gros fichiers QML.

### 11.2 Finir l’uniformisation design system
But : supprimer les derniers écarts de style historique.

### 11.3 Clarifier la frontière UI / app
But : éviter que l’UI absorbe de la logique réseau, sync ou métier.

### 11.4 Préparer le viewer sans polluer le shell actuel
But : maintenir une UI Cloud propre tout en laissant une porte nette au futur viewer.

---

## 12. Position finale retenue

La documentation UI unifiée considère que :
- le produit visible aujourd’hui est le **client cloud** ;
- le design system est déjà une base sérieuse ;
- le chantier principal n’est plus l’habillage, mais la **maîtrise de la complexité** ;
- le viewer ne doit pas dégrader le shell cloud avant d’avoir un vrai périmètre implémenté.
