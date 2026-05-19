## 4) View: Dialog “File Details” (depuis Files)

Statut documentaire : `IMPLEMENTE`

### But
Afficher un dump lisible des métadonnées (général/slicing/cloud) d’un fichier cloud.

### Data affichées
- Section [General] : name, file_id, extension, size, gcode_id, status, status_code
- Section [Slicing] : print time, layers, thickness, machine, material, resin usage, dimensions, bottom layers, exposure/off time, printers, md5
- Section [Cloud] : upload/created/updated, thumbnail URL, download URL, region/bucket/path

### Positionnement
- QDialog, taille ~760×460.
- Titre (label) + body (QPlainTextEdit monoBlock) + Close.

### Thème
- Body = `monoBlock` (monospace).

### Analyse
- Très “debug-friendly” (bon pour reverse/diagnostic).
- Aucun lien cliquable (URL affichées en texte). OK si contrainte “pas de fuite”, mais peut être frustrant.

---

