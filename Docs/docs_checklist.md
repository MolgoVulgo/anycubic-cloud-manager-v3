# Checklist maintenance documentation

## Statut
- `IMPLEMENTE`.

## A faire pour toute PR qui touche le code

1. Mettre a jour les docs directement impactees (`README*`, `Docs/*`).
2. Verifier l'alignement `etat reel` vs `SPEC` dans `Docs/etat_reel_vs_cible.md`.
3. Si un ecran/flux est draft, marquer explicitement `PARTIEL` ou `DRAFT`.
4. Si une capture runtime est ajoutee, marquer `SNAPSHOT` + date de reference.
5. Verifier les liens de `Docs/README.md` (pas de lien mort).

## A faire avant commit doc

1. Relire le diff pour supprimer les affirmations non prouvees.
2. Verifier que toute commande/tests cites ont ete executes ou marquer clairement "non execute".
3. Ajouter les references code minimales pour toute page runtime.
4. Garder les statuts (`IMPLEMENTE`, `PARTIEL`, `SPEC`, `SNAPSHOT`) coherents entre pages.
5. Si la PR touche l'UI QML, executer `python3 accloud/tools/check_ui_migration.py` et reporter le statut.

## Revue periodique conseillee

- Frequence: a chaque lot fonctionnel majeur ou au moins mensuelle.
- Points critiques:
  - endpoints et auth,
  - flux Files/Printers,
  - i18n,
  - zones scaffold.
