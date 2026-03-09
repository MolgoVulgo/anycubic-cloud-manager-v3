# Plan de correction

## 1. Objectif

Ce plan découpe la correction UI en étapes successives, avec un objectif simple :

- corriger les défauts visibles ;
- réduire la dette structurelle ;
- éviter les régressions ;
- valider chaque correction par des tests ciblés.

Le chantier doit être conduit du plus structurant au plus local. Corriger les symptômes avant la structure serait une perte de temps élégante, donc une perte de temps quand même.

---

## 2. Priorités globales

Ordre de traitement recommandé :

1. système d’onglets ;
2. shell des dialogs ;
3. système de formulaires ;
4. normalisation thème / tokens ;
5. nettoyage des pages lourdes.

---

## 3. Étape 1 — corriger le système d’onglets

## 3.1 But

Obtenir un vrai système d’onglets de type languettes :

- groupe unique ;
- contour continu ;
- onglet actif fusionné avec le panneau ;
- coins supérieurs arrondis ;
- aucune cassure de ligne.

## 3.2 Fichiers visés

- `ui/qml/components/AppTabBar.qml`
- `ui/qml/components/AppTabButton.qml`
- `ui/qml/pages/PrintersTabsBar.qml`
- `ui/qml/MainWindow.qml`
- `ui/qml/pages/CloudFileDetailsDialog.qml`

## 3.3 Corrections à appliquer

### A. Reprendre la structure du composant

Mettre en place une structure unique :

```text
TabsShell
 ├─ AppTabBar
 └─ ContentPanel
```

Règles :
- une seule bordure globale ;
- la barre d’onglets constitue le bord haut du panneau ;
- le contenu ne redessine pas une bordure haute concurrente.

### B. Corriger `AppTabButton`

Le tab doit avoir :
- radius haut gauche = `Theme.radiusControl`
- radius haut droit = `Theme.radiusControl`
- radius bas = `0`
- bordure basse supprimée si actif

### C. Corriger les séparateurs

Les séparateurs verticaux doivent :
- rester internes à la barre d’onglets ;
- ne jamais casser la ligne de base ;
- ne jamais dépasser ou couper les bordures structurelles.

### D. Corriger la baseline

Sous les tabs :
- une ligne continue unique ;
- masquée uniquement sous l’onglet actif.

### E. Supprimer les doubles conteneurs visuels

Dans `PrintersTabsBar.qml` et dans la zone tabs de `MainWindow.qml` :
- supprimer les bordures concurrentes ;
- ne garder qu’une seule autorité de contour.

## 3.4 Tests de validation

### Test visuel T1.1
Vérifier que les tabs `Files / Printers / Logs` forment un seul groupe visuel.

Critères :
- pas d’effet “trois boutons” ;
- contour continu ;
- aucun angle carré parasite.

### Test visuel T1.2
Vérifier que `Printer > Device Details` utilise la même grammaire visuelle.

Critères :
- tabs locales cohérentes avec tabs globales ;
- aucun décalage de bordure.

### Test fonctionnel T1.3
Cliquer successivement sur chaque onglet.

Critères :
- changement d’état actif correct ;
- changement de contenu correct ;
- aucun clignotement ou saut de layout.

### Test géométrique T1.4
Vérifier aux extrémités gauche/droite et aux jonctions entre tabs.

Critères :
- coins arrondis bien rendus ;
- ligne basse continue ;
- fusion propre de l’onglet actif avec le contenu.

### Test clavier T1.5
Si navigation clavier supportée :
- Tab focus
- flèches gauche/droite si prévu
- Entrée/Espace si prévu

Critères :
- état actif cohérent ;
- focus visible.

---

## 4. Étape 2 — durcir le shell des dialogs

## 4.1 But

Garantir que toutes les fenêtres/modals reposent sur le même shell, sans double header ni structure recodée localement.

## 4.2 Fichiers visés

- `ui/qml/components/AppDialogFrame.qml`
- `ui/qml/dialogs/PrintDraftDialog.qml`
- `ui/qml/dialogs/UploadDraftDialog.qml`
- `ui/qml/dialogs/ViewerDraftDialog.qml`
- `ui/qml/dialogs/SessionSettingsDialog.qml`
- dialogs inline éventuels de `MainWindow.qml`

## 4.3 Corrections à appliquer

### A. Verrouiller le shell

Dans `AppDialogFrame.qml` :
- `header: null`
- `padding: 0`
- `framePadding` basé sur token
- style header/footer basé sur tokens thème

### B. Interdire les headers locaux

Dans les dialogs métiers :
- supprimer tout header principal reconstruit à la main ;
- utiliser uniquement le header du shell.

### C. Uniformiser footer et fermeture

Règles :
- `X` géré par le shell ;
- ordre des actions stable ;
- actions destructives isolées du flux principal.

### D. Prévoir variante scrollable si nécessaire

Créer ou stabiliser :
- `AppScrollableDialogFrame.qml`

## 4.4 Tests de validation

### Test visuel T2.1
Ouvrir chaque dialog principal.

Critères :
- même header ;
- même position du bouton `X` ;
- pas de surplus haut ;
- footer homogène.

### Test structurel T2.2
Vérifier que le body du dialog ne contient plus de faux header.

Critères :
- un seul titre visible ;
- pas de double bandeau.

### Test fonctionnel T2.3
Tester fermeture via :
- bouton `X`
- bouton `Close/Cancel`
- touche `Esc`
- clic hors dialog si autorisé

Critères :
- comportement conforme à la policy.

### Test scroll T2.4
Pour les dialogs longs :
- vérifier overflow ;
- vérifier scroll ;
- vérifier que header/footer restent propres.

---

## 5. Étape 3 — créer le système de formulaires

## 5.1 But

Éviter les formulaires assemblés ligne par ligne avec des labels manuels.

## 5.2 Composants à créer

- `ui/qml/components/FormLabel.qml`
- `ui/qml/components/FormRow.qml`
- éventuellement `ui/qml/components/FormSection.qml`

## 5.3 Corrections à appliquer

### A. Créer `FormLabel`

Style imposé :
- `font.pixelSize: Theme.fontBodyPx`
- `color: Theme.fgPrimary`
- alignement vertical stable

### B. Créer `FormRow`

Rôle :
- aligner label + champ ;
- standardiser spacing ;
- permettre largeur de colonne stable.

### C. Refactorer les dialogs de formulaire

Priorité :
- `PrintDraftDialog.qml`
- `UploadDraftDialog.qml`
- `SessionSettingsDialog.qml`
- `Default3DRenderingSettings` / équivalent
- `Language Settings` / `Theme Settings` si besoin d’homogénéisation supplémentaire

## 5.4 Tests de validation

### Test visuel T3.1
Comparer plusieurs dialogs de formulaire.

Critères :
- labels alignés ;
- même taille de texte ;
- même spacing vertical.

### Test fonctionnel T3.2
Tester :
- tabulation clavier ;
- focus successif ;
- lecture des libellés ;
- cohérence champs/labels.

### Test responsive T3.3
Redimensionner les dialogs.

Critères :
- pas de collision label/champ ;
- pas de rupture de layout inutile.

---

## 6. Étape 4 — normaliser thème, typo et couleurs

## 6.1 But

Faire des tokens canoniques la seule référence de style pour les composants refactorés.

## 6.2 Fichiers visés

- `ui/qml/components/Theme.js`
- `ui/qml/components/FileCard.qml`
- `ui/qml/components/BusyOverlay.qml`
- `ui/qml/components/ErrorBanner.qml`
- `ui/qml/components/ProgressCard.qml`
- pages/dialogs contenant encore des couleurs ou styles littéraux

## 6.3 Corrections à appliquer

### A. Basculer vers tokens canoniques

À privilégier :
- `fgPrimary`
- `fgSecondary`
- `bgSurface`
- `bgDialog`
- `borderDefault`
- `borderSubtle`
- `accent`
- `accentFg`

### B. Réduire les alias legacy

Ne plus introduire :
- `textPrimary`
- `textSecondary`
- `panel`
- `card`
- `panelStroke`

### C. Supprimer les couleurs hardcodées

Exemple :
- couleur hex locale dans `CloudFilesPage.qml`

### D. Encadrer la monospace

La monospace doit rester limitée aux zones techniques :
- logs
- JSON
- payloads
- debug

## 6.4 Tests de validation

### Test visuel T4.1
Comparer :
- dialogs
- cards
- overlays
- banners
- pages

Critères :
- cohérence de couleurs ;
- cohérence de contraste ;
- même hiérarchie typographique.

### Test de thème T4.2
Tester les presets de thème disponibles.

Critères :
- aucune couleur locale aberrante ;
- lisibilité correcte ;
- composants cohérents entre thèmes.

### Test typographique T4.3
Vérifier titres, labels, textes secondaires, états.

Critères :
- tailles conformes aux tokens ;
- monospace absente hors zones techniques.

---

## 7. Étape 5 — alléger les pages lourdes

## 7.1 But

Réduire la densité des gros fichiers QML pour faciliter maintenance et corrections futures.

## 7.2 Fichiers visés

- `ui/qml/pages/CloudFilesPage.qml`
- `ui/qml/pages/PrinterPage.qml`
- éventuellement `ui/qml/pages/LogPage.qml`

## 7.3 Corrections à appliquer

### A. Extraire les blocs réutilisables

Pour `CloudFilesPage.qml`, candidats typiques :
- toolbar
- quota card
- table header
- table row
- contenu d’onglets de détail

Pour `PrinterPage.qml`, candidats typiques :
- toolbar
- tabs bar
- panneau détail imprimante
- panneau historique
- sections remote print

### B. Garder la page comme orchestrateur

La page doit piloter :
- composition ;
- états ;
- navigation ;

Elle ne doit pas porter toute la mécanique visuelle détaillée.

## 7.4 Tests de validation

### Test structurel T5.1
Vérifier que les pages continuent à rendre les mêmes contenus.

Critères :
- pas de perte de fonctionnalité ;
- pas de régression d’affichage.

### Test métier T5.2
Tester les parcours principaux :
- liste cloud
- upload
- détail fichier
- print
- liste imprimantes
- détail imprimante
- logs

Critères :
- parcours complets sans régression.

---

## 8. Étape 6 — campagne de validation finale

## 8.1 Vérifications globales

### V1 — cohérence UI

Contrôler :
- tabs
- dialogs
- formulaires
- titres
- footers
- cards
- overlays

### V2 — comportement

Contrôler :
- clic
- clavier
- scroll
- fermeture dialogs
- changement onglets
- resize

### V3 — cohérence thème

Contrôler :
- palette
- contraste
- états actifs / inactifs / disabled
- composants d’action primaire / secondaire / danger

## 8.2 Liste de tests finaux

### Test final F1 — tabs
- page générale
- page printer
- dialog détail fichier

### Test final F2 — dialogs
- print draft
- upload draft
- viewer draft
- session settings
- language/theme settings
- render defaults

### Test final F3 — formulaires
- alignement labels
- focus
- ordre de tabulation
- cohérence visuelle

### Test final F4 — thème
- preset clair
- autres presets disponibles
- accent colors

### Test final F5 — parcours métier
- import HAR
- navigation pages
- upload
- print order
- ouverture détail fichier
- session details
- logs

---

## 9. Critères de sortie

Le plan est considéré terminé quand :

1. les tabs forment un vrai panneau à onglets ;
2. tous les dialogs reposent sur le même shell ;
3. les formulaires utilisent une structure commune ;
4. les composants refactorés n’utilisent plus de styles locaux critiques ;
5. les couleurs et typos sont cohérentes ;
6. les parcours métier passent sans régression visible.

---

## 10. Ordre d’exécution recommandé

Ordre final conseillé :

1. `AppTabBar` / `AppTabButton` / shell tabs
2. `AppDialogFrame` + dialogs métiers
3. `FormLabel` / `FormRow` + formulaires
4. normalisation thème/tokens
5. extraction des gros blocs de page
6. campagne finale de validation

---

## 11. Résumé opérationnel

Le chantier doit être mené comme suit :

- d’abord corriger la **géométrie commune** ;
- ensuite corriger la **structure commune** ;
- ensuite corriger la **grammaire formulaire** ;
- ensuite nettoyer les **styles et tokens** ;
- enfin stabiliser les **pages métier**.

Autrement dit : on arrête de peigner les pixels pendant que l’ossature boîte encore.

