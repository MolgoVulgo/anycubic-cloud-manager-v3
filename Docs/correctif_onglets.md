# Correctif Onglets — Spécification consolidée (v1 + v2 + v3)

## 1. Objet

Ce document fusionne les versions successives du correctif onglets :

- `correctif_onglets.md`
- `correctif_onglets_v_2.md`
- `correctif_onglets_v_3.md`

Objectif : définir une seule référence de correction pour obtenir un système d'onglets visuellement continu, cohérent et réutilisable.

---

## 2. Problèmes cibles

Les écarts à corriger sont :

- effet "suite de boutons" au lieu d'un groupe d'onglets unique ;
- rupture de ligne entre barre d'onglets et panneau de contenu ;
- coins incohérents entre onglets et conteneur ;
- bordures doublées ou décalées entre tabs ;
- séparations verticales mal raccordées ;
- micro-écarts résiduels (espacement perçu, z-order, baseline).

---

## 3. Architecture cible

Structure unique attendue :

```text
TabsShell
 ├─ AppTabBar
 └─ ContentPanel
```

Règles structurelles :

- une seule bordure globale par shell tabs ;
- `AppTabBar` constitue le bord supérieur du panneau ;
- `ContentPanel` ne redessine pas de bordure supérieure concurrente ;
- la baseline est unique, continue, interrompue uniquement sous l'onglet actif.

---

## 4. Grammaire visuelle cible

Le rendu attendu est celui d'un panneau à languettes type navigateur/préférences :

- onglets jointifs (pas d'effet d'items indépendants) ;
- un seul onglet actif ;
- onglet actif visuellement fusionné avec le contenu ;
- onglets inactifs en retrait mais dans le même bloc ;
- contour continu, sans cassure ni double ligne ;
- lecture immédiate du groupe comme composant unique.

---

## 5. Règles géométriques

### 5.1 Espacement

- `spacing` horizontal entre tabs : `0` en mode classique ;
- pas de `anchors.margins` horizontaux sur les tabs ;
- pas de trous visuels entre tabs.

### 5.2 Coins

- rayon supérieur gauche/droit des tabs : `Theme.radiusControl` ;
- aucun rayon en bas des tabs ;
- coins internes tab/tab carrés (arrondis uniquement aux extrémités du groupe).

### 5.3 Bordures

- éviter les doubles bordures entre deux tabs ;
- une seule autorité de trait, standardisée via tokens thème ;
- séparateurs verticaux limités à la zone tab, sans coupe de baseline.

### 5.4 Baseline

- ligne unique sous la barre d'onglets ;
- interruption nette uniquement sous l'onglet actif ;
- alignement exact avec le panneau (pas de décrochage 1px).

### 5.5 Z-order

- onglet actif rendu au-dessus du panneau (`z` supérieur) pour éviter la micro-ligne parasite.

### 5.6 Padding horizontal tabs

- padding horizontal modéré (`18px` cible), sans dépasser `20px`.

---

## 6. Règles d'implémentation QML

### 6.1 Composants concernés

- `accloud/ui/qml/components/AppTabBar.qml`
- `accloud/ui/qml/components/AppTabButton.qml`
- `accloud/ui/qml/pages/PrintersTabsBar.qml`
- `accloud/ui/qml/MainWindow.qml`
- `accloud/ui/qml/pages/CloudFileDetailsDialog.qml`

### 6.2 API minimale

Le système tabs doit exposer :

- liste des tabs ;
- `currentIndex` ;
- signal de changement ;
- mode `equal`/`content` selon contexte.

### 6.3 Variantes

- navigation principale (`Files/Printers/Logs`) : largeur homogène, shell large ;
- tabs locales (`Device Details`, dialogs de détail) : largeur contenu, shell compact ;
- même grammaire visuelle dans les deux cas.

---

## 7. Standardisation thème (obligatoire)

Le style tabs ne doit pas reposer sur des littéraux dispersés.

Tokens dédiés dans `Theme.js` :

- `tabStrokeWidth`
- `tabStrokeColor`
- `tabBaselineColor`

Règles :

- couleur/épaisseur du trait d'onglet pilotées par ces tokens ;
- baseline pilotée par ces tokens ;
- aucun hardcode local pour contour tabs.

---

## 8. Critères de validation

Le correctif est validé si :

1. la barre tabs + contenu est perçue comme un seul composant ;
2. aucune double bordure n'est visible entre tabs ;
3. aucune rupture de trait n'est visible aux extrémités/jonctions ;
4. les coins supérieurs sont cohérents, sans angle carré parasite ;
5. la baseline est continue et interrompue proprement sous l'actif ;
6. `Files/Printers/Logs` et `Device Details` partagent la même grammaire ;
7. le changement d'onglet ne provoque pas de saut de layout.

---

## 9. Tests recommandés

### 9.1 Test visuel

Vérifier :

- aucun espace entre tabs ;
- aucune double ligne ;
- coins cohérents ;
- continuité du contour global.

### 9.2 Test interaction

Cliquer successivement :

- `Files`
- `Printers`
- `Logs`

Vérifier :

- état actif correct ;
- contenu associé correct ;
- pas de clignotement/saut.

### 9.3 Test extrémités

Inspecter les coins gauche/droite du groupe tabs :

- rayon correct ;
- raccord du trait propre ;
- pas d'encoche interne résiduelle.

---

## 10. Décision finale

Source de vérité unique : ce document.

Toute correction tabs future doit modifier ce fichier et ne pas réintroduire de documents `correctif_onglets_v_X.md` concurrents.
