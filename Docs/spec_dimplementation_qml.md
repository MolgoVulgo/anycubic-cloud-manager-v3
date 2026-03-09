# Spec d’implémentation QML

## 1. Objet

Cette spec définit la mise en œuvre QML cible pour la couche UI du projet à partir de l’état réel de la dernière version.

Le but n’est pas de refaire l’UI from scratch. Le but est de **verrouiller l’architecture de présentation**, d’éliminer les divergences de structure déjà visibles, et de transformer les composants existants en un système QML cohérent, maintenable et testable.

La base existe déjà :

- `Theme.js`
- `AppButton`, `AppTextField`, `AppComboBox`, `AppSpinBox`, `AppSlider`, `AppCheckBox`
- `AppPageFrame`
- `AppDialogFrame`
- `SectionHeader`
- pages métier déjà structurées

Le problème n’est donc pas l’absence de fondation. Le problème est que **la fondation cohabite encore avec des implémentations locales concurrentes**.

---

## 2. Diagnostic synthétique

## 2.1 Points déjà solides

La version actuelle a déjà plusieurs briques correctes :

- tokens de thème centralisés dans `Theme.js` ;
- composants transverses de base déjà en place ;
- `AppDialogFrame` déjà exploité sur plusieurs dialogs ;
- `AppPageFrame` utilisé comme conteneur de page ;
- `SectionHeader` déjà aligné avec la hiérarchie visuelle du projet ;
- design system suffisamment avancé pour éviter une refonte complète.

## 2.2 Problèmes confirmés

### A. Deux modèles de dialogs coexistent

1. **Dialogs structurés via `AppDialogFrame`**
2. **Dialogs bruts `Dialog { ... }` avec header/body/footer reconstruits localement**

Effets :
- duplication ;
- styles proches mais pas identiques ;
- double header potentiel ;
- spacing incohérent ;
- coût de maintenance inutile.

### B. Typographie et couleurs pas complètement verrouillées

Le thème expose des tokens canoniques, mais le code continue à utiliser :

- des alias legacy (`textPrimary`, `card`, `panel`, etc.) ;
- des `Text` stylés localement ;
- des tailles implicites ou non normalisées ;
- quelques cas de polices mono hors zone strictement technique.

### C. Mix tokens + littéraux

Présence récurrente de :
- `15` pour marges de dialogs ;
- `8`, `10`, `12`, `14` pour spacing ;
- hauteurs/rayons/couleurs répétées localement.

### D. Pages lourdes encore trop denses

`CloudFilesPage.qml` et `PrinterPage.qml` restent très gros et concentrent à la fois :

- layout ;
- logique d’affichage ;
- gestion d’états ;
- dialogs métier.

Ce n’est pas cassé, mais c’est une usine à régression si on continue ainsi.

---

## 3. Décision d’architecture

## 3.1 Principe directeur

L’UI doit reposer sur trois niveaux fixes :

1. **tokens**
2. **composants de design system**
3. **pages/dialogs métier composés à partir de ces composants**

Aucune fenêtre métier ne doit reconstruire librement sa structure de base.

## 3.2 Composants structurants officiels

Le socle officiel devient :

- `Theme.js` : palette, typo, spacing, radius, dimensions
- `AppPageFrame.qml` : shell de page
- `AppDialogFrame.qml` : shell unique des dialogs/modals
- `SectionHeader.qml` : hiérarchie locale de section
- `AppButton.qml` : action standard
- `AppTextField.qml`, `AppComboBox.qml`, `AppSpinBox.qml`, `AppSlider.qml`, `AppCheckBox.qml` : champs standards
- `InlineStatusBar.qml`, `ErrorBanner.qml`, `ProgressCard.qml`, `BusyOverlay.qml`, `StatusChip.qml` : feedback/état

Tous les dialogs et pages métier doivent converger sur ce socle.

---

## 4. Spécification du shell de dialog

## 4.1 Décision

`AppDialogFrame.qml` devient le **shell officiel et unique** des dialogs.

Il remplace de fait les `Dialog` bruts reconstruits localement.

## 4.2 Structure cible

```text
AppDialogFrame
 ├─ Header
 │   ├─ Title
 │   ├─ Subtitle optionnel
 │   ├─ Header actions optionnelles
 │   └─ Close button
 ├─ Body
 │   └─ contenu métier injecté
 └─ Footer
     ├─ leading actions
     └─ trailing actions
```

## 4.3 Contraintes obligatoires

Le shell doit imposer :

- `header: null`
- `padding: 0`
- `modal: true`
- `background` unique
- `Overlay.modal` unique
- `closePolicy` piloté par propriété
- bouton `X` géré par le shell uniquement
- plus aucun header/body/footer principal reconstruit à la main dans les dialogs métier

## 4.4 API cible de `AppDialogFrame`

API minimale à conserver ou stabiliser :

- `title`
- `subtitle`
- `allowScrimClose`
- `showCloseButton`
- `showHeaderDivider`
- `showFooterDivider`
- `minimumWidth`
- `maximumWidth`
- `minimumHeight`
- `maximumHeight`
- `bodyData`
- `headerActionsData`
- `footerLeadingData`
- `footerTrailingData`

## 4.5 Évolutions obligatoires

### A. Neutraliser explicitement le header implicite Qt

À ajouter dans `AppDialogFrame.qml` :

```qml
header: null
```

Objectif : supprimer toute ambiguïté avec le header natif de `Dialog`.

### B. Remplacer `framePadding: 15`

Le padding interne ne doit plus être piloté par un littéral par défaut.

Cible :

```qml
property int framePadding: Theme.paddingDialog
```

### C. Introduire une notion de taille normalisée

Ajouter une variante simple pilotable :

- `dialogSize: "small" | "medium" | "large" | "workspace"`

Ou, si on garde l’approche actuelle, fournir des helpers documentés pour éviter les couples `minimumWidth/maximumWidth` inventés au cas par cas.

### D. Ajouter variante scrollable

Certaines fenêtres doivent pouvoir accueillir du contenu long sans bricoler un `ScrollView` de structure à chaque fois.

Deux options valides :

1. `AppDialogFrame` + `bodyScrollable: true`
2. `AppScrollableDialogFrame.qml`

Décision recommandée : garder simple et créer :
- `AppDialogFrame.qml`
- `AppScrollableDialogFrame.qml`

---

## 5. Spécification du shell de page

## 5.1 Décision

`AppPageFrame.qml` reste le shell unique des pages.

## 5.2 Rôle

Il doit centraliser :

- fond de page ;
- padding principal ;
- espacement vertical des sections ;
- éventuel support header local de page ;
- structure générique lisible pour les pages métier.

## 5.3 Règle

Toute page métier (`CloudFilesPage`, `PrinterPage`, `LogPage`, `SettingsPage`, `DebugPage`, `CloudLoginPage`, `ViewerPage`) doit être structurée autour de `AppPageFrame` et non d’un assemblage libre d’items racines concurrents.

---

## 6. Spécification des tokens UI

## 6.1 Politique générale

Le système de tokens déjà en place devient la source unique de vérité.

Tous les nouveaux développements et tous les refactors doivent utiliser les tokens canoniques.

## 6.2 Tokens canoniques à utiliser

### Couleurs

- `bgWindow`
- `bgSurface`
- `bgDialog`
- `fgPrimary`
- `fgSecondary`
- `fgDisabled`
- `borderDefault`
- `borderSubtle`
- `accent`
- `accentFg`
- `danger`
- `warning`
- `success`
- `selectionBg`
- `selectionFg`
- `overlayScrim`
- `fgOnDanger`
- `viewportBg`
- `viewportBorder`
- `viewportFg`

### Typographie

- `fontTitlePx`
- `fontSectionPx`
- `fontBodyPx`
- `fontCaptionPx`

### Layout

- `paddingPage`
- `paddingDialog`
- `gapRow`
- `gapSection`
- `controlHeight`
- `radiusControl`
- `radiusDialog`
- `borderWidth`

## 6.3 Interdiction progressive des alias legacy

Les alias legacy restent compatibles à court terme mais ne doivent plus être utilisés dans les composants refactorés :

- `textPrimary`
- `textSecondary`
- `panel`
- `card`
- `panelStroke`
- autres alias historiques du même type

Politique :
- **pas de nouveau code** basé sur les alias legacy ;
- les composants modifiés doivent migrer vers les tokens canoniques.

---

## 7. Spécification typographique

## 7.1 Hiérarchie officielle

### Titre principal de dialog/page
- `Theme.fontTitlePx`
- `font.bold: true`
- `Theme.fgPrimary`

### Titre de section
- `Theme.fontSectionPx`
- `font.bold: true`
- `Theme.fgPrimary`

### Texte métier standard
- `Theme.fontBodyPx`
- `Theme.fgPrimary`

### Texte secondaire / aide / meta / statut passif
- `Theme.fontCaptionPx` ou `Theme.fontBodyPx` selon importance
- `Theme.fgSecondary`

## 7.2 Usage de la monospace

La monospace est autorisée uniquement dans :

- logs ;
- JSON ;
- payloads ;
- diagnostics techniques ;
- traces ;
- zones de debug.

Elle ne doit pas apparaître dans les headers, formulaires, settings, détails métier standards.

---

## 8. Spécification des boutons et actions

## 8.1 Variants officiels

`AppButton` conserve :

- `primary`
- `secondary`
- `danger`

## 8.2 Hiérarchie d’action

### Footer trailing
Ordre recommandé :

1. action secondaire
2. fermeture
3. action primaire

Exemples :
- `Close`, puis `Apply`
- `Cancel`, puis `Send order`
- `Download`, `Close`, `Print`

### Footer leading
Réservé à :
- actions destructives ;
- reset ;
- actions techniques latérales.

Exemples :
- `Delete`
- `Reset`
- `Retry`
- `Export`

## 8.3 Règles

- le bouton primaire doit être unique par footer, sauf cas exceptionnel ;
- les actions destructives ne doivent pas être noyées au milieu des actions neutres ;
- le `X` ne remplace pas un bouton `Close` quand le footer doit exprimer explicitement la sortie.

---

## 9. Variantes officielles de dialogs

## 9.1 Standard dialog

Pour :
- settings ;
- formulaires ;
- édition simple ;
- détail court.

Base : `AppDialogFrame`

## 9.2 Scrollable dialog

Pour :
- détails session ;
- informations longues ;
- about ;
- git ;
- diagnostics.

Base : `AppScrollableDialogFrame`

## 9.3 Workspace dialog

Pour :
- viewer 3D ;
- outils avec panneau latéral + viewport.

Base : `AppDialogFrame` avec taille `workspace` ou bornes larges.

## 9.4 Confirm dialog

Pour :
- suppression ;
- confirmation critique ;
- yes/no ;
- cancel/confirm.

Base : `AppDialogFrame` taille `small`, `allowScrimClose: false` si action critique.

## 9.5 Info dialog

Pour :
- erreur ;
- succès ;
- warning ;
- information simple.

Base : `AppDialogFrame` taille `small`.

---

## 10. Plan de refactor fichier par fichier

## 10.1 Priorité P1 — shell et composants système

### `accloud/ui/qml/components/AppDialogFrame.qml`

Actions :
- ajouter `header: null` ;
- remplacer padding littéral par token ;
- stabiliser API ;
- documenter les tailles cibles ;
- vérifier calcul hauteur/largeur ;
- prévoir variante scrollable.

### `accloud/ui/qml/components/Theme.js`

Actions :
- maintenir compatibilité ;
- clarifier que les tokens canoniques sont la référence ;
- éviter tout nouveau composant basé sur alias legacy.

### `accloud/ui/qml/components/SectionHeader.qml`

Actions :
- conserver comme composant standard de titre local ;
- interdire les titres de section recodés librement quand `SectionHeader` couvre le besoin.

## 10.2 Priorité P2 — dialogs métier dédiés

### `dialogs/PrintDraftDialog.qml`

Refactor cible :
- remplacer le `Dialog` brut par `AppDialogFrame` ;
- déplacer description et champs dans le body ;
- footer sur `footerTrailingData` ;
- plus aucun header local.

### `dialogs/UploadDraftDialog.qml`

Même stratégie que `PrintDraftDialog.qml`.

### `dialogs/ViewerDraftDialog.qml`

Refactor cible :
- passer sur `AppDialogFrame` ;
- conserver body split panel ;
- garder les actions `Retry`, `Reset camera`, `Export screenshot` en leading ou trailing selon hiérarchie ;
- normaliser le footer.

### `dialogs/SessionSettingsDialog.qml`

Refactor cible :
- passer sur `AppDialogFrame` ou variante scrollable ;
- intégrer proprement la bannière de mode obligatoire ;
- structurer la zone résultat comme bloc de contenu, pas comme layout libre de dialog brut ;
- homogénéiser footer et close policy.

## 10.3 Priorité P3 — dialogs inline de `MainWindow.qml`

À migrer vers `AppDialogFrame` :

- `sessionPathDialog`
- `sessionDetailsDialog`
- `render3dDefaultsDialog`
- `aboutDialog`
- `gitDialog`

### Règle

Aucun `Dialog { ... }` inline ne doit continuer à reconstruire son header/footer à la main si le besoin est couvert par le shell.

## 10.4 Priorité P4 — pages métier

### `pages/CloudFilesPage.qml`

Objectifs :
- garder les dialogs déjà sur `AppDialogFrame` ;
- nettoyer les zones encore trop denses ;
- extraire des blocs réutilisables si nécessaire.

Sous-composants potentiels à extraire :
- `CloudFilesToolbar`
- `CloudQuotaCard`
- `CloudFilesTableHeader`
- `CloudFilesTableRow`
- `FileDetailsBasicTab`
- `FileDetailsSliceTab`

### `pages/PrinterPage.qml`

Objectifs :
- conserver les dialogs déjà sur `AppDialogFrame` ;
- extraire les blocs fonctionnels majeurs ;
- réduire la densité du fichier principal.

Sous-composants potentiels à extraire :
- `PrinterToolbar`
- `PrinterTabs`
- `PrinterDetailPanel`
- `PrinterHistoryPanel`
- `RemotePrintTaskSummary`
- `RemotePrintOptionsSection`
- `RemotePrintGuardMessage`

### `pages/LogPage.qml`

Objectifs :
- conserver l’usage mono là où il est justifié ;
- vérifier la hiérarchie de texte autour des filtres ;
- éviter les tailles locales hors tokens sauf titre de page explicitement validé.

---

## 11. Conventions d’écriture QML

## 11.1 Layout

Règles :
- utiliser `Layout.*` dans les `RowLayout`, `ColumnLayout`, `GridLayout`, `StackLayout` ;
- éviter de mélanger `anchors` et `Layout.*` sur le même item sauf cas précis justifié ;
- préférer les layouts à la géométrie absolue ;
- garder le positionnement absolu uniquement pour les composants qui en ont réellement besoin (`AppSlider`, colonnes calculées, overlays ciblés).

## 11.2 Styling

Règles :
- pas de couleurs hex locales dans les shells et composants standards ;
- pas de taille de police littérale dans les zones structurantes ;
- pas de marges/paddings littéraux si un token existe ;
- pas de rayon/bordure littéraux si un token existe.

## 11.3 Dialogs

Règles :
- un seul shell ;
- aucun faux header ;
- aucun faux footer ;
- aucun `title:` utilisé comme rendu natif si le shell gère le titre.

## 11.4 Pages volumineuses

Règles :
- extraire les blocs denses en composants ;
- garder la page comme orchestrateur, pas comme fourre-tout de 1500 lignes ;
- séparer logique visuelle et logique métier au maximum possible côté QML.

---

## 12. Stratégie d’implémentation

## Phase 1 — durcissement du design system

Livrables :
- `AppDialogFrame.qml` stabilisé ;
- éventuel `AppScrollableDialogFrame.qml` ;
- conventions tokens validées ;
- charte de footer validée.

## Phase 2 — migration dialogs critiques

Ordre recommandé :
1. `PrintDraftDialog.qml`
2. `UploadDraftDialog.qml`
3. `ViewerDraftDialog.qml`
4. `SessionSettingsDialog.qml`
5. dialogs inline de `MainWindow.qml`

## Phase 3 — réduction des gros fichiers page

Ordre recommandé :
1. `CloudFilesPage.qml`
2. `PrinterPage.qml`
3. `LogPage.qml` si nécessaire

## Phase 4 — verrouillage projet

À partir de cette phase :
- aucun nouveau dialog brut ;
- aucun nouveau style structurel hors tokens ;
- aucun nouveau header/footer local ;
- tout nouveau composant structurel doit être documenté dans le design system.

---

## 13. Critères d’acceptation

Le chantier sera considéré conforme quand :

1. tous les dialogs fonctionnels utilisent `AppDialogFrame` ou sa variante scrollable ;
2. plus aucun dialog métier ne reconstruit localement son header principal ;
3. aucun surplus haut de dialog n’apparaît à cause d’un double header ;
4. les footers suivent une hiérarchie stable ;
5. les composants structurels utilisent les tokens canoniques ;
6. les alias legacy ne sont plus utilisés dans les composants refactorés ;
7. les gros fichiers page commencent à être re-segmentés ;
8. les tests QML restent verts.

---

## 14. Tests à maintenir ou ajouter

## 14.1 Régression visuelle/structurelle

Vérifier :
- ouverture/fermeture dialog ;
- bouton `X` ;
- `Esc` ;
- clic scrim selon policy ;
- ordre des boutons footer ;
- taille minimale/maximale ;
- contenu scrollable ;
- focus et navigation clavier.

## 14.2 Régression métier

Vérifier :
- import HAR ;
- paramètres session ;
- language/theme ;
- upload ;
- print order ;
- viewer ;
- delete confirm ;
- remote print config.

## 14.3 KPI techniques de suivi

Indicateurs proposés :
- % dialogs basés sur `AppDialogFrame` ;
- % composants structurels utilisant les tokens canoniques ;
- nombre de `Dialog {` bruts restants ;
- taille maximale d’une page métier ;
- nombre de sous-composants extraits de `CloudFilesPage` et `PrinterPage`.

---

## 15. Décision finale

La dernière version du projet confirme que l’UI est déjà suffisamment structurée pour passer à une phase de normalisation sérieuse.

La bonne stratégie n’est pas une refonte cosmétique.
La bonne stratégie est :

1. **durcir le design system existant** ;
2. **faire de `AppDialogFrame` le shell unique des dialogs** ;
3. **migrer les dialogs bruts** ;
4. **normaliser tokens, typo et couleurs** ;
5. **segmenter progressivement les pages trop denses**.

En clair : on arrête les petites républiques indépendantes de QML. Il faut un État central, avec constitution, cadastre et police des paddings.

---

## 16. Système d’onglets (fusion du correctif)

Le système d’onglets est normalisé sur un style **languettes connectées au contenu** (type préférences classiques), et non sur des boutons.

## 16.1 Objectif UX

Le composant doit rendre immédiatement lisible :

- qu’il s’agit d’un groupe unique ;
- qu’un seul onglet est actif ;
- que l’onglet actif est connecté visuellement au panneau affiché ;
- que les onglets inactifs sont en retrait mais appartenant au même groupe.

## 16.2 Règles visuelles

### Conteneur

- ligne de base commune ;
- tabs jointifs ou quasi jointifs ;
- continuité visuelle avec le panneau.

### Onglet actif

- fond proche du panneau ;
- bordure continue ;
- bord inférieur neutralisé sous l’actif (effet connecté) ;
- texte plus affirmé.

### Onglet inactif

- fond neutre ;
- bordure visible mais moins dominante ;
- texte secondaire.

## 16.3 Règles structurelles

Le composant officiel est :

- `AppTabBar.qml`
- `AppTabButton.qml`

API attendue :

- `currentIndex`
- signal de changement
- sizing `equal` ou `content`
- variantes `navigation` et `local`
- rendu `classic` connecté au panneau.

Aucun onglet ne doit être reconstruit localement avec des `AppButton`.

## 16.4 Variantes officielles

### Navigation principale

Cas :
- `Files`, `Printers`, `Logs`

Règles :
- largeur répartie (`equal`) ;
- groupe large ;
- style stable de navigation.

### Onglets locaux

Cas :
- liste imprimantes (`Printer > Device Details`)
- tabs détail fichier cloud

Règles :
- largeur au contenu (`content`) ;
- groupe compact ;
- même grammaire visuelle que la navigation.

## 16.5 Critères d’acceptation onglets

Le correctif onglets est conforme si :

1. les tabs de navigation sont perçus comme un seul groupe ;
2. les tabs locaux ne ressemblent pas à des boutons indépendants ;
3. l’état actif est immédiatement identifiable ;
4. le style est homogène entre pages ;
5. tous les usages passent par `AppTabBar`/`AppTabButton`.

## 16.6 Validation automatisée

Contrôle local/CI :

- script : `accloud/tools/check_ui_migration.py`
- test CTest : `accloud_ui_migration_check`

Commandes :

```bash
python3 accloud/tools/check_ui_migration.py
ctest --preset default -R accloud_ui_migration_check --output-on-failure
```

---

## 17. État d’implémentation (mise à jour)

### 17.1 Dialogs

- shell unique `AppDialogFrame` appliqué aux dialogs métier ;
- `AppScrollableDialogFrame` ajouté ;
- header/footer du shell uniformisés avec une teinte légèrement plus foncée (`Qt.darker(Theme.bgDialog, 1.05)`).

### 17.2 Onglets

- `AppTabBar`/`AppTabButton` durcis ;
- variante `navigation` appliquée aux tabs principales ;
- variante `local` appliquée aux tabs locales ;
- rendu `classic` connecté au contenu actif.

### 17.3 Segmentation pages (Lot D)

- `CloudFilesPage.qml` réduit par extraction des sous-composants (toolbar, quota, table, pagination, dialog détail) ;
- `PrinterPage.qml` réduit par extraction des sous-composants (toolbar, tabs, panel principal, dialogs remote print, config avancée).

### 17.4 Validation

- `qmllint` vert sur les composants/pages refactorés ;
- `accloud_ui_qml` vert ;
- `accloud_ui_migration_check` vert ;
- aucun `Dialog { ... }` métier hors `AppDialogFrame`.
