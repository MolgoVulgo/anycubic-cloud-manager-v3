# Qt i18n workflow

## Statut
- `IMPLEMENTE` (workflow build + runtime), avec backlog d'harmonisation backend messages.

## Fichiers
- Catalogues: `accloud/i18n/accloud_en.ts`, `accloud/i18n/accloud_fr.ts`
- Bridge runtime: `accloud/app/AppI18nBridge.{h,cpp}`
- UI QML: `qsTr(...)`

## Workflow operateur

1. Modifier les textes UI en utilisant `qsTr(...)` et des placeholders `%1`, `%2`.
2. Mettre a jour les catalogues:
   - `cmake --build accloud/build/default --target update_translations`
3. Traduire/valider les `.ts` (Qt Linguist).
4. Regenerer les `.qm`:
   - `cmake --build accloud/build/default --target release_translations`
5. Rebuild et verifier:
   - `cmake --build --preset default`
   - `ctest --preset default --output-on-failure`

## Conventions
- Garder non traduits les identifiants techniques (`op_id`, `file_id`, codes API).
- Les logs techniques JSONL restent en langue source.
- Preferer `messageKey + params` pour les messages backend affiches a l'utilisateur.
