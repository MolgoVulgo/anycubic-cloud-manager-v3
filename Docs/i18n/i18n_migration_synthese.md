# i18n Qt - Synthese etat reel et plan d'achevement

Date de reference: 2026-03-08

## 1) Statut global

- Migration i18n: `PARTIEL`.
- Socle technique i18n Qt: `IMPLEMENTE`.
- Couverture backend->UI par cles de message: `PARTIEL`.

## 2) Ce qui est deja implemente

### 2.1 Socle runtime
- Bridge langue: `accloud/app/AppI18nBridge.{h,cpp}`.
- Chargement traducteurs app + Qt (`QTranslator`) avec fallback EN.
- Persistence preference langue via `QSettings` (`ui.language`).
- Retraduction a chaud via `engine.retranslate()`.

### 2.2 Build/CMake
- `Qt6::LinguistTools` detecte dans `accloud/CMakeLists.txt`.
- Catalogues versionnes:
  - `accloud/i18n/accloud_en.ts`
  - `accloud/i18n/accloud_fr.ts`
- Generation `.qm` via `qt_add_translations(...)`.
- Cibles build disponibles dans `accloud/build/default`:
  - `update_translations`
  - `release_translations`

### 2.3 UI QML
- Utilisation large de `qsTr(...)` dans `MainWindow.qml`, `CloudFilesPage.qml`, `PrinterPage.qml`, `LogPage.qml`, dialogs et composants.
- Dialog langue expose en UI (`Settings > Language`).

### 2.4 Backend->UI
- `messageKey` renseigne dans certains retours bridge:
  - `CloudBridge::finalizeUiMessage`
  - `SessionImportBridge::finalizeUiMessage`

## 3) Ecarts restants

1. Le mapping `messageKey` n'est pas encore la strategie unique dans tout le frontend.
2. Des remplacements ad hoc de messages localises restent presents en QML (replacements chinois/anglais a la volee).
3. Certaines chaines dynamiques construites en JS peuvent etre moins bien extraites par `lupdate`.
4. Gouvernance incomplete sur la frontiere:
   - message utilisateur traduisible
   - log technique non traduisible.

## 4) Plan d'achevement

1. Standardiser backend->UI sur `messageKey + params` pour les messages fonctionnels.
2. Ajouter un mapping QML centralise `messageKey -> qsTr(...)` et supprimer les remplacements ad hoc.
3. Passer les textes dynamiques a placeholders `%1/%2` avec contextes stables.
4. Ajouter une verification CI i18n:
   - echec si nouvelles chaines UI non extraites/non gerees.
5. Verifier la non-regression EN/FR (smoke UI).

## 5) Definition of Done cible

- Tous les textes UI visibles passent par `qsTr(...)` ou `qsTrId(...)`.
- Les messages backend utilisateur passent par des cles stables (`messageKey`).
- Les logs techniques restent hors couche i18n.
- Les catalogues `.ts` sont a jour et `.qm` generes au build.

