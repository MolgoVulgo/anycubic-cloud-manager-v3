# Migration i18n Qt - Synthese et plan

## 1) Branche
- Branche creee: `i18n`

## 2) Etat actuel (constat)
- Aucune utilisation de Qt i18n detectee (`qsTr`, `tr`, `QT_TR_NOOP`, `QT_TRANSLATE_NOOP`).
- Les chaines UI sont majoritairement hardcodees en QML.
- Les messages backend exposes a l'UI sont en partie hardcodes en C++ (`CloudClient`, `SessionImportBridge`, `LogBridge`), avec un melange EN/FR et quelques remplacements ad hoc cote QML.
- CMake ne configure pas encore `Qt6::LinguistTools`, ni generation de `.ts` / `.qm`.

## 3) Inventaire des chaines UI (reference complete)
Extraction automatisee realisee le 2026-03-07.

Fichiers generes:
- `Docs/i18n/qml_ui_props_strings.txt` (243 lignes): proprietes `text/title/subtitle/placeholderText`.
- `Docs/i18n/qml_ui_status_assignments.txt` (51 lignes): affectations runtime des messages de statut UI.
- `Docs/i18n/qml_ui_models_strings.txt` (13 lignes): modeles/choix UI (`ComboBox`, filtres, presets).
- `Docs/i18n/cpp_ui_message_candidates.txt` (340 lignes): candidats messages C++ exposes a l'UI.

Volume QML total recense (props + status + models): 307 occurrences, 273 valeurs litterales uniques.

Hotspots (charge de migration):
- `accloud/ui/qml/MainWindow.qml`: 66 occurrences.
- `accloud/ui/qml/pages/PrinterPage.qml`: 66 occurrences.
- `accloud/ui/qml/pages/CloudFilesPage.qml`: 67 occurrences.
- `accloud/ui/qml/dialogs/SessionSettingsDialog.qml`: 17 occurrences.

## 4) Points sensibles a traiter avant traduction
- Constantes techniques a ne pas traduire:
  - codes/status machine (`READY`, `ERROR`, etc. internes protocolaires),
  - cles JSON (`op_id`, `component`, `event`, etc.),
  - identifiants techniques (`file_id`, `printer_id`) et formats.
- Texte backend renvoye tel quel:
  - actuellement, des erreurs C++ sont deja localisees en FR et d'autres en EN.
  - il faut eviter de traduire des phrases en sortie d'API distante si elles sont deja localisees serveur.
- Composition de chaines par concatenation (QML/C++):
  - a convertir vers placeholders (`%1`, `%2`) pour qualite linguistique.

## 5) Proposition de plan de migration (Qt i18n)

### Phase A - Socle technique (priorite haute)
1. Ajouter un dossier i18n:
   - `accloud/i18n/accloud_en.ts`
   - `accloud/i18n/accloud_fr.ts`
   - (optionnel) `accloud/i18n/accloud_zh_CN.ts`
2. Mettre a jour `accloud/CMakeLists.txt`:
   - `find_package(Qt6 COMPONENTS LinguistTools ...)`
   - generation des `.qm` (ex: `qt_add_translations(...)` ou equivalent Qt6).
3. Charger les traducteurs au demarrage (`main.cpp`):
   - `QTranslator` app + Qt,
   - selection locale systeme + override utilisateur futur,
   - fallback robuste (EN source).
4. Distribuer les `.qm` (qrc ou dossier deploiement) et garantir le chargement runtime.

### Phase B - Migration QML (priorite haute)
1. Remplacer les chaines UI fixes par `qsTr(...)`:
   - menus, boutons, titres, sous-titres, placeholders, empty states.
2. Remplacer les concatenations UI par placeholders:
   - ex: `qsTr("Files %1").arg(count)`.
3. Traduire les modeles visibles utilisateur:
   - labels de filtres, presets, options de dialogues.
4. Conserver non traduits les tokens techniques (codes, noms API, IDs).

### Phase C - Messages backend exposes UI (priorite haute)
Deux options (recommandee: option 2):
1. Traduction directe en C++ via `tr()` (si classes QObject et contexte stable).
2. **Recommandee**: retourner des codes de message stables (`error.network`, `printer.not_selected`, etc.) + arguments, puis mapper en QML via `qsTrId(...)`.

Option 2 evite:
- duplication EN/FR dans le backend,
- regressions sur changements de texte,
- confusion entre logs techniques et messages utilisateur.

### Phase D - Gouvernance des textes (priorite moyenne)
1. Definir une convention:
   - UI: `qsTrId("ui.files.refresh")`
   - Backend->UI: `messageKey + params`
   - Logs: non traduits (techniques).
2. Uniformiser la langue source (EN recommandee) avant traduction FR/autres.
3. Nettoyer les remplacements ad hoc de texte chinois en QML et les remplacer par mapping officiel.

### Phase E - Validation et CI (priorite haute)
1. Ajouter une verification CI:
   - extraction `lupdate`,
   - echec CI si nouvelles chaines non internationalisees.
2. Ajouter tests UI minimaux:
   - smoke FR/EN,
   - verification de bascule de langue a chaud (si implementee),
   - non-regression sur valeurs techniques non traduites.
3. Ajouter pseudo-localisation (optionnel) pour detecter layouts casses.

## 6) Ordonnancement recommande (execution)
1. Socle CMake + chargement traducteur runtime.
2. Migration `MainWindow.qml`, `CloudFilesPage.qml`, `PrinterPage.qml`.
3. Migration dialogs + components communs.
4. Refactor backend vers `messageKey + params`.
5. QA multi-langue + CI.

## 7) Definition of Done (DoD)
- 0 chaine UI visible hardcodee hors `qsTr/qsTrId` dans QML.
- 0 message utilisateur C++ hardcode sans cle de traduction.
- Fichiers `.ts` versionnes et `.qm` charges au runtime.
- Tests smoke EN/FR verts.
- Documentation i18n maintenue.
