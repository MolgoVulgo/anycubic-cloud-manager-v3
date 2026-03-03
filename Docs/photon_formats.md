# Formats Anycubic Photon — extraction des specs (PWMB / PWS / PHZ / PHOTONS / PWSZ)

## 0) Cadre / sources internes utilisées

- Code **manager_anycubic_cloud** : implémente surtout le parsing/décode **PWMB** (container + codecs `pw0Img`/`pwsImg`).
- Rapport UVtools (analyse statique) : confirme la logique *tables + versions + codec PW0 pour PWMB* et le découpage des sections. fileciteturn0file0
- CDF (exigences) : fixe les invariants de lecture/décodage (tables-first, sections versionnées, contrats PW0/PWS, vérité matière, etc.). fileciteturn0file1
- Templates 010 Editor (`*.bt`) fournis : décrivent les headers/champs des formats **PHOTONS**, **PHZ**, **PWSZ/PWSZScene** et le container « PhotonWorkshop ».

> Objectif ici : **décrire les formats** (structures, sections, champs, encodages couche) et les **différences** entre familles Anycubic Photon.

---

## 1) Carte rapide des formats (familles)

| Extension | Famille | Nature | Encodage des couches | Notes clés |
|---|---|---|---|---|
| `.pwmb` | Photon Workshop “tables-first” (Anycubic) | **bitmap slices + previews + tables** | `pw0Img` (RLE word16 BE, index 4-bit + run 12-bit) ou `pwsImg` (RLE u8 + AA multipass) | Versions observées 515/516/517/518, sections conditionnelles |
| `.pws` | Photon Workshop (ancienne/variante) | bitmap slices | `pwsImg` (RLE u8 + AA) | Convention run_len C0/C1 à auto-déterminer |
| `.phz` | Photon Mono / génération « phz » | bitmap slices + previews | bloc RLE brut par layer (template décrit l’adressage + tailles) | `PageNumber` + `DataAddress` (support >4 Go) |
| `.photons` | Photon S (ancienne) | bitmap slices + preview RGB565 | `LayerRLE` (taille dérivée de `DataSize`) | Container **BigEndian** + header en `double` |
| `.pwsz` | PWSZ « image/lines » | **vector/contours** (pas bitmap) | segments (Start/End) | Marqueurs `{==`, `[--`, `--]`, `==}` |
| `.pwsz` (scene) | PWSZ « scene meta » | métadonnées layers/contours | pas de pixels | Magic `ANYCUBIC-PWSZ`, LayerDef (area/bbox/contours) |

---

## 2) PWMB (Photon Workshop Binary) — container *tables-first*

### 2.1 Signature, version, table addresses

**Signature** : `ANYCUBIC` (préfixe sur 12 octets, padding `\0`). fileciteturn0file0

**Header FILEMARK** (schéma logique commun) :

- `mark[12]` : signature
- `version:u32` : versions vues côté tooling : **515/516/517/518** (compat) fileciteturn0file0
- `table_count:u32` (1..64)
- `table_addresses[table_count]:u32` : offsets de tables (ordre **non garanti**)

**Deux patterns de table** (déduits du code) :

1) **Table “framed”** :
   - `TableName[12]` ASCII (ex: `HEADER`, `MACHINE`, `LAYERDEF`, `PREVIEW`, `EXTRA`, `SOFTWARE`, `MODEL`, `SUBLAYER`, `PREVIEW2`, …)
   - `payload_len:u32` puis payload
2) **Bloc brut** : si pas “framed”, le payload est délimité par la prochaine adresse de table (ou EOF), avec bornage strict.

> Implication : parsing robuste = **table map** puis lecture best-effort des sections.

### 2.2 Sections versionnées (PWMB évolutif)

Découpage observé (UVtools + CDF) : fileciteturn0file0 fileciteturn0file1

- `HEADER` + `PREVIEW` : base
- `LayerImageColorTable` : **≥ 515**
- `EXTRA` + `MACHINE` : **≥ 516**
- `SOFTWARE` + `MODEL` : **≥ 517**
- `SubLayerDefinition` + `PREVIEW2` : **≥ 518**

### 2.3 Métadonnées utiles (cœur “viewer/scale”)

Champs attendus / nécessaires (CDF) : fileciteturn0file1

- Échelle : `PixelSizeUm`, `LayerHeight`
- Résolution : `ResolutionX`, `ResolutionY`
- Expositions : `ExposureTime`, `BottomExposureTime`, `BottomLayersCount`
- Anti-aliasing : `AntiAliasing` / `MaxAntialiasingLevel`
- Machine : `MachineName`, `LayerImageFormat`

### 2.4 Encodage couche : `pw0Img` (PW0)

Codec “PW0” (spécifié/confirmé) : fileciteturn0file0 fileciteturn0file1

- Flux = suite de **mots 16-bit big-endian**
- `word = BE16`
- `color_index = (word >> 12) & 0xF` (0..15)
- `run_len = word & 0x0FFF` (1..4095) ; `0` invalide (strict)
- Décodage : remplir linéairement `W*H` pixels (row-major)

**LUT / intensité** :

- Si `LayerImageColorTable` est présente : `index -> intensity` via LUT.
- Invariant “vide” : **index 0 = vide** ⇒ intensity forcée à 0 même si LUT aberrante. fileciteturn0file1
- Si LUT absente : fallback linéaire typique `index*17` (0..255) avec index0=0.

### 2.5 Encodage couche : `pwsImg` (PWS)

Codec `pwsImg` (RLE u8 + anti-aliasing multipass) : fileciteturn0file1

- Flux = tokens `u8`
- `bit7` : exposé (1) / vide (0)
- `bits0..6` : répétitions (`reps`)
- Ambiguïté sur la longueur du run :
  - **C0** : `run_len = reps`
  - **C1** : `run_len = reps + 1`
- Sélection **déterministe** recommandée : dry-run sur `W*H` + invariants (pas d’overrun, cohérence éventuelle `NonZeroPixelCount`). fileciteturn0file1

**Anti-aliasing** :

- On fait `AA` passes ; pour chaque pixel, on compte combien de passes l’exposent.
- Projection en `uint8` : `round(255 * count / AA)`.

### 2.6 Raster & axes (conventions de mapping)

Conventions de représentation (CDF) : fileciteturn0file1

- Raster : flat `W*H`, **row-major** (`x=i%W`, `y=i//W`)
- Origine image : **haut-gauche**
- Monde :
  - `pitch_xy_mm = PixelSizeUm / 1000`
  - `pitch_z_mm = LayerHeight`
  - inversion `Y` si le monde attend Y “vers le haut”

---

## 3) PWS (extension `.pws`) — même codec image, container potentiellement différent

Dans l’écosystème Anycubic, `.pws` est souvent traité comme « même logique d’image en `pwsImg` ».

- Cœur technique : **RLE u8 + AA** (voir §2.5)
- Point dur : **convention C0/C1**

Dans le code manager_anycubic_cloud, la brique “decode_pws” est explicitement conçue pour résoudre **C0 vs C1** automatiquement via dry-run + heuristiques.

---

## 4) PHZ (`.phz`) — container “offsets + layer descriptors”

### 4.1 Endianness / header

Template décrit : **LittleEndian**.

**HEADER** (champs principaux) :

- `Magic:u32`, `Version:u32`
- `LayerHeightMilimeter:f32`
- `LayerExposureSeconds:f32`, `BottomExposureSeconds:f32`
- `BottomLayersCount:u32`
- `ResolutionX:u32`, `ResolutionY:u32`
- Offsets :
  - `PreviewLargeOffsetAddress:u32`
  - `PreviewSmallOffsetAddress:u32`
  - `LayersDefinitionOffsetAddress:u32`
- `LayerCount:u32`
- Paramètres machine/process : `PrintTime`, `ProjectorType`, `AntiAliasLevel`, `LightPWM`, `BottomLightPWM`, `TotalHeight`, `BedSizeX/Y/Z`, `EncryptionKey`, `EncryptionMode`, `TimestampMinutes`, `SoftwareVersion`, etc.
- `MachineNameAddress:u32` + `MachineNameSize:u32`

### 4.2 Previews

Deux previews possibles, chacun avec un petit header :

- `ResolutionX`, `ResolutionY`
- `ImageOffset`, `ImageLength`
- puis `Data[ImageLength]`

### 4.3 Layers : table de descripteurs + adressage paginé

`LAYER_DATA` par couche :

- `PositionZ:f32`
- `Exposure:f32`
- `LightOffSeconds:f32`
- `DataAddress:u32`
- `DataSize:u32`
- `PageNumber:u32`
- (plusieurs `Unknown:u32`)

Lecture du blob couche (template) :

- on se place sur `LayersDefinitionOffsetAddress`, on lit les `LAYER_DATA` séquentiellement
- pour chaque layer : seek vers `PageNumber * 2^32 + DataAddress` puis lire `DataSize` octets de RLE

> Point clé : PHZ supporte nativement des fichiers **>4 Go** via `PageNumber`.

**Remarque** : le template fourni décrit la *structure*, mais ne fixe pas ici le codec exact du bloc `layerDataBlock` (il est traité comme binaire brut). Donc : structure OK, encodage couche à confirmer par reverse sur échantillons.

---

## 5) PHOTONS (`.photons`) — Anycubic Photon S (ancienne génération)

### 5.1 Endianness / header

Template décrit : **BigEndian**.

**HEADER** :

- `Tag1:u32` (ex: 2)
- `Tag2:u16` (ex: 49)
- `XYPixelSize:double`
- `LayerHeight:double`
- `ExposureSeconds:double`
- `LightOffDelay:double`
- `BottomExposureSeconds:double`
- `BottomLayerCount:u32`
- Cinématique : `LiftHeight/LiftSpeed/RetractSpeed:double`
- `VolumeMl:double`
- Preview : `PreviewResolutionX:u32`, `PreviewResolutionY:u32` + bloc raw (taille `W*H*2` ⇒ typiquement RGB565)
- `LayerCount:u32`

### 5.2 Layers

Pour chaque layer, un bloc `layerData` :

- `Unknown1:u32` (ex: 44944)
- `Unknown2:u32` (ex: 0)
- `Unknown3:u32` (ex: 0)
- `ResolutionX:u32`, `ResolutionY:u32`
- `DataSize:u32`
- `Unknown4:u32` (ex: 2684702720)
- `LayerRLE[DataSize/8 - 4]` (bloc binaire)

> Point clé : la couche est stockée sous forme RLE/bit-packed (la formule `DataSize/8 - 4` suggère une logique “bits” + méta), mais l’algorithme exact n’est pas décrit par le template.

---

## 6) PWSZ (`.pwsz`) — formats “vector/scene” (pas un fichier d’impression bitmap)

### 6.1 PWSZ « lines » (marqueurs `{== ... ==}`)

Structure (LittleEndian) :

- `FileStartMarker[4]` : `{==`
- `BOUNDING_BOX` : area + offsets XY + `ObjectCount`
- `ImageStartMarker[4]` : `[--`
- `LineCount:u32` + `Unknown:u32` (souvent 1)
- `LINE[LineCount]` :
  - `StartX/YOffsetFromCenter:f32`
  - `EndX/YOffsetFromCenter:f32`
  - `Unknown:u8` (0/1)
- `ImageEndMarker[4]` : `--]`
- `FileEndMarker[4]` : `==}`

Interprétation : représentation **vectorielle** (segments) probablement destinée à des contours / debug / preview technique.

### 6.2 PWSZ « scene meta » (Magic `ANYCUBIC-PWSZ`)

Structure (LittleEndian) :

- `Magic[16]` : `ANYCUBIC-PWSZ`
- `Software[64]` : identifiant générateur
- `BinaryType:u32` (1/2/3)
- `Version:u32` (1)
- `LayerCount:u32`
- Bounding rectangle global (start/end XY) + `MinZ/MaxZ`
- Un gros `Padding[64]`
- `Separator[4]` : `<---`
- `LayerCount` répété

Puis `LAYER_DEF[LayerCount]` (métadonnées par layer) :

- `Height:f32`, `Area:f32`
- Bounding rectangle offsets
- `ContourCount:u32`, `MaxContourArea:f32`
- `Padding[8]`

Puis `EndMarker[4]` : `--->`

Interprétation : **métadonnées de scène / contours** ; pas de pixels, pas un “slice bitmap” exploitable directement pour un rendu voxel.

---

## 7) Ce que le code manager_anycubic_cloud couvre réellement

### 7.1 Couverture actuelle

- Parsing/décodage solide pour la famille **PWMB** :
  - FILEMARK + table addresses + tables framed/best-effort
  - codecs couche : `pw0Img` + `pwsImg`
  - support heuristique (variants) pour réduire les cas réels “bizarres”

### 7.2 Trous fonctionnels (formats Anycubic hors PWMB)

- **PHZ** : la structure est claire (offsets + layer descriptors + page addressing), mais il manque le codec exact du blob layer (à dériver par reverse sur échantillons).
- **PHOTONS** : structure claire (BE, doubles), mais l’algorithme `LayerRLE` n’est pas spécifié ici.
- **PWSZ** : pas une cible “viewer voxel” (c’est du vector/meta), donc à traiter comme format auxiliaire.

---

## 8) Invariants transverses utiles (quel que soit le container)

- Toujours séparer :
  - **lecture container** (header/offsets/tables)
  - **lecture layer index** (addr/len/z/flags)
  - **codec layer** (PW0, PWS, autre)
- Toujours borner :
  - offsets dans `[0, file_size)`
  - tailles raisonnables
  - remplissage exact `W*H` (sinon erreur/dégradé contrôlé)
- Pour le rendu/mesures : **vérité matière** = pixel non-noir (seuil 0) ; les seuils “127” sont réservés aux heuristiques d’analyse. fileciteturn0file1

