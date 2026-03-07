# Qt i18n workflow

## Fichiers
- Sources de traduction: `accloud/i18n/accloud_en.ts`, `accloud/i18n/accloud_fr.ts`
- QML/UI: `qsTr(...)`
- Backend->UI: `message` + `messageKey` (bridges)

## Commandes
- Mettre a jour les catalogues:
  - `cmake --build accloud/build/default --target update_translations`
- Regenerer l'app et les `.qm`:
  - `cmake --build --preset default`
- Tests:
  - `ctest --preset default`

## Conventions
- Utiliser des placeholders `%1`, `%2`, ... pour les textes dynamiques.
- Garder non traduits les identifiants techniques (`op_id`, `file_id`, etc.).
- Les logs techniques restent en langue source et ne sont pas des textes UI.
