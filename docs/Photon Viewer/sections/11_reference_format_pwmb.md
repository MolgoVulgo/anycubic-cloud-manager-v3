# 11) Reference format PWMB (extrait)

Statut documentaire : `SPEC`

## Role

Reference technique concise du format PWMB, utilisee pour cadrer le parsing, le mapping et la verification des hypotheses de format.

## Cadrage

- document de reference technique, non normatif a lui seul;
- a lire avec `Docs/03_photon_viewer_formats.md`;
- priorite au code reel en cas d'ecart.

## Source

- `Docs/03_photon_viewer_formats.md` (section 2).

## Contenu technique

## 2) PWMB (Photon Workshop Binary) — container *tables-first*

### 2.1 Signature, version, table addresses

**Signature** : `ANYCUBIC` (préfixe sur 12 octets, padding `\0`).

**Header FILEMARK** (schéma logique commun) :

- `mark[12]` : signature
- `version:u32` : versions vues côté tooling : **515/516/517/518** (compat)
- `table_count:u32` (1..64)
- `table_addresses[table_count]:u32` : offsets de tables (ordre **non garanti**)

**Deux patterns de table** (déduits du code) :

1) **Table “framed”** :
   - `TableName[12]` ASCII (ex: `HEADER`, `MACHINE`, `LAYERDEF`, `PREVIEW`, `EXTRA`, `SOFTWARE`, `MODEL`, `SUBLAYER`, `PREVIEW2`, …)
   - `payload_len:u32` puis payload
2) **Bloc brut** : si pas “framed”, le payload est délimité par la prochaine adresse de table (ou EOF), avec bornage strict.

> Implication : parsing robuste = **table map** puis lecture best-effort des sections.

### 2.2 Sections versionnées (PWMB évolutif)

Découpage observé (UVtools + CDF) :

- `HEADER` + `PREVIEW` : base
- `LayerImageColorTable` : **≥ 515**
- `EXTRA` + `MACHINE` : **≥ 516**
- `SOFTWARE` + `MODEL` : **≥ 517**
- `SubLayerDefinition` + `PREVIEW2` : **≥ 518**

### 2.3 Métadonnées utiles (cœur “viewer/scale”)

Champs attendus / nécessaires (CDF) :

- Échelle : `PixelSizeUm`, `LayerHeight`
- Résolution : `ResolutionX`, `ResolutionY`
- Expositions : `ExposureTime`, `BottomExposureTime`, `BottomLayersCount`
- Anti-aliasing : `AntiAliasing` / `MaxAntialiasingLevel`
- Machine : `MachineName`, `LayerImageFormat`

### 2.4 Encodage couche : `pw0Img` (PW0)

Codec “PW0” (spécifié/confirmé) :

- Flux = suite de **mots 16-bit big-endian**
- `word = BE16`
- `color_index = (word >> 12) & 0xF` (0..15)
- `run_len = word & 0x0FFF` (1..4095) ; `0` invalide (strict)
- Décodage : remplir linéairement `W*H` pixels (row-major)

**LUT / intensité** :

- Si `LayerImageColorTable` est présente : `index -> intensity` via LUT.
- Invariant “vide” : **index 0 = vide** ⇒ intensity forcée à 0 même si LUT aberrante.
- Si LUT absente : fallback linéaire typique `index*17` (0..255) avec index0=0.

### 2.5 Encodage couche : `pwsImg` (PWS)

Codec `pwsImg` (RLE u8 + anti-aliasing multipass) :

- Flux = tokens `u8`
- `bit7` : exposé (1) / vide (0)
- `bits0..6` : répétitions (`reps`)
- Ambiguïté sur la longueur du run :
  - **C0** : `run_len = reps`
  - **C1** : `run_len = reps + 1`
- Sélection **déterministe** recommandée : dry-run sur `W*H` + invariants (pas d’overrun, cohérence éventuelle `NonZeroPixelCount`).

**Anti-aliasing** :

- On fait `AA` passes ; pour chaque pixel, on compte combien de passes l’exposent.
- Projection en `uint8` : `round(255 * count / AA)`.

### 2.6 Raster & axes (conventions de mapping)

Conventions de représentation (CDF) :

- Raster : flat `W*H`, **row-major** (`x=i%W`, `y=i//W`)
- Origine image : **haut-gauche**
- Monde :
  - `pitch_xy_mm = PixelSizeUm / 1000`
  - `pitch_z_mm = LayerHeight`
  - inversion `Y` si le monde attend Y “vers le haut”

---
