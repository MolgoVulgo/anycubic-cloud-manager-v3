## 3) View: Tab "Files" - Cloud Files

### Statut
- `IMPLEMENTE` (workflow principal de production cote fichiers cloud).

### But
Lister les fichiers cloud et exposer les actions:
- refresh
- details
- download
- delete
- entree vers print (via Printers)

### Data affichees
1) Toolbar
- `Refresh`
- `Upload` (placeholder de lot suivant, pas pipeline upload complet dans cette vue)
- filtre `Type`

2) Quota
- `Used / Total`
- `Free`
- `Files <count>`
- progress bar quota

3) Status
- statut operationnel inline (cache/cloud/download/delete/sync)

4) Listing dense (table)
Colonnes:
- Thumb
- File name
- Type
- Size
- Date
- Actions

Actions par ligne:
- `Details`
- `Download`
- `Print`
- menu `Rename` / `Delete`

5) Dialog details fichier
- onglets `Basic Information` et `Slice Settings`
- actions footer: delete/download/print

### Positionnement
- Layout vertical: toolbar -> quota -> table -> pagination locale
- Pagination locale (rows/page 10/20/50/100)

### Comportement reel
- Strategie cache-first:
  - charge cache local (`loadCachedFiles`, `loadCachedQuota`)
  - puis sync cloud asynchrone (`refreshFilesAsync`)
- Download:
  - obtention URL signee
  - ecriture locale avec progression et annulation

### Analyse
- Flux stable et teste (QML tests + sync signals).
- Ecarts connus:
  - `Upload` reste un placeholder dans cette page.
  - `Print` redirige vers le flux printer-centric (pas de sendOrder direct ici).

---
