# UI/Theme Action Tasks

## Phase 0 - Cadrage
- [x] Définir les priorités sur les écrans actifs (`MainWindow`, `CloudFilesPage`, `PrinterPage`, `LogPage`).
- [x] Définir la règle "pas de hardcode visuel hors exceptions documentées".

## Phase 1 - Fondations Design System
- [x] Ajouter les tokens manquants dans `Theme.js` (`overlayScrim`, `fgOnDanger`, tokens viewport).
- [x] Ajouter les composants partagés manquants (`AppCheckBox`, `AppSpinBox`).
- [x] Ajouter un composant `AppSlider` (optionnel, à faire si besoin d'harmonisation stricte).

## Phase 2 - Migration UI Prioritaire
- [x] Migrer `SettingsPage.qml` vers `AppPageFrame` et composants `App*`.
- [x] Migrer `DebugPage.qml` vers `AppPageFrame` et composants `App*`.
- [x] Migrer `CloudLoginPage.qml` vers `AppPageFrame` et composants `App*`.
- [x] Réduire l'usage de couleurs codées en dur dans overlays/dialogs.

## Phase 3 - Réactivité thème
- [x] Supprimer le reload du shell UI lors du changement de thème.
- [x] Conserver la persistance thème/accent sans re-mount du `Loader`.

## Phase 4 - Validation rapide
- [x] Exécuter les tests UI QML.
- [ ] Vérifier les régressions visuelles majeures (revue manuelle).

## Phase 5 - Uniformisation Onglets
- [x] Créer `AppTabBar` + `AppTabButton`.
- [x] Migrer les onglets de `MainWindow`, `PrinterPage` et `CloudFilesPage`.
- [x] Ajouter la couverture de test QML minimale pour les composants onglets.

## Phase 6 - Nettoyage
- [x] Éliminer l'usage direct de `TabBar`/`TabButton` hors composants `AppTab*`.
- [x] Ajouter les composants `AppTab*` au `resources.qrc`.
