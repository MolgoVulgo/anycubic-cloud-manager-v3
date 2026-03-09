# Structure de l’application (KDE / Linux)

## Statut document

- Type: `SPEC` (cible architecture).
- Etat implementation reel: voir `Docs/etat_reel_vs_cible.md`.
- Cette page decrit la cible technique; elle ne signifie pas que tous les modules sont implementes.
- Les details de formats (PWMB/PWS/PHZ/PHOTONS/PWSZ) sont centralises dans `Docs/photon_formats.md` (reference spec).

## Etat reel (rappel court)

- Implemente aujourd'hui: Cloud manager, import HAR/session, UI Files/Printers/Logs, cache SQLite local, logging.
- Partiellement ou non implemente: `infra/photons`, `render3d`, `infra/jobs`, `infra/cache` (hors SQLite app), tests photons et outils photons.

## 0) Choix techno (KDE-friendly)
- **Langage** : C++20
- **UI** : **Qt 6 + QML (Qt Quick Controls 2) + Kirigami** (patterns KDE)
- **Rendu 3D** : OpenGL via **Qt Quick** (QQuickFramebufferObject ou QSGRenderNode) + pipeline CPU asynchrone
- **I/O & réseau** : QNetworkAccessManager (Cloud)
- **Build** : CMake + presets (Ninja)
- **Tests** : Catch2 / GoogleTest + golden tests **formats Photon** (incl. PWMB)

## 1) Architecture logique (couches)

### 1.1 Couches
1. **UI (QML/Kirigami)**
   - Pages, panneaux, composants réutilisables
   - Bindings sur ViewModels C++ (properties/signals)

2. **Application (ViewModels + Orchestrateurs)**
   - Coordination des use-cases (Open fichier Photon, Download & Open, Build 3D, Export)
   - Gestion des jobs, progress, annulation

3. **Domain (modèles & règles)**
   - PhotonsDocument, LayerIndex, MaskTruth, mesures
   - Invariants (vérité matière, stride/LOD, invalidation cache)

4. **Infrastructure (implémentations concrètes)**
   - Lecture **multi-formats Photon** via **drivers** (probe + dispatch)
   - Codecs (PW0/PWS, + autres selon format)
   - Cache RAM (fenêtre glissante) + cache disque (LRU)
   - Client Cloud + persistance session
   - Logs JSONL (sinks séparés)

### 1.2 Patterns
- **MVVM** : QML = View, C++ = ViewModel, Domain = règles stables
- **Jobs non bloquants** : QThreadPool/QRunnable ou std::jthread + queue
- **Annulation** : token atomique partagé (checkpoints par étape)
- **Progress** : (percent:int, stage:string) normalisés

## 2) Modules (responsabilités)

### 2.1 Module `photons`

#### 2.1.1 PhotonsReader (détection + dispatch)
- Entrée : chemin fichier / `QIODevice`.
- Sortie : `PhotonsDocument` normalisé + `capabilities`.
- Stratégie : **probe multi-drivers** (magic/header) puis fallback extension.

**Capacités (par format)**
- `BitmapSlices` : index de couches + (option) decode → `mask_truth`.
- `VectorOrMeta` : pas de pixels, seulement segments/contours/métadonnées.
- `HasPreviews` : previews exploitable (thumbnail/large).

**Détection (règle)**
- Ne jamais décider sur l’extension seule si un magic fiable existe.
- Probe = tests rapides, non allouants, bornés (offsets/tailles plausibles).

**Détection (ordre “fort”)**
- **PWMB** : signature ASCII `ANYCUBIC` en tête + logique *tables-first*.
- **PWSZ scene** : magic ASCII `ANYCUBIC-PWSZ`.
- **PWSZ lines** : marqueurs `{==` puis `[--` / `--]` / `==}`.
- **PHZ** : header LittleEndian cohérent (offsets previews/layers, LayerCount, ResolutionX/Y plausibles) + adressage `PageNumber + DataAddress`.
- **PHOTONS** : header BigEndian (Tag1/Tag2) + séries de `double` + preview RGB565 (W*H*2) cohérente.
- **PWS** : fallback (souvent pas de magic stable) : extension + heuristique `pwsImg`.

#### 2.1.2 Interface driver (contrat)
Chaque driver expose :
- `probe(stream) -> confidence (0..100)`
- `readMeta()` (résolution, pitch XY/Z, AA, expositions)
- `readPreviews()` (si présents)
- `buildLayerIndex()` (addr/len/z/flags)
- `decodeLayer(i)` **uniquement** si `BitmapSlices` et codec connu

#### 2.1.3 Couverture formats (selon photon_formats.md)
- **PWMB** (bitmap, tables-first) :
  - index + previews
  - decode layers : **PW0** et **PWS**
- **PWS** (bitmap) :
  - decode layers : **PWS** (RLE u8 + AA, C0/C1 auto)
- **PHZ** (bitmap) :
  - meta/index : offsets + descriptors + page addressing
  - **codec layer à confirmer** → support minimal : extraction brute blob + previews
- **PHOTONS** (bitmap, BigEndian, doubles) :
  - meta + preview RGB565
  - **codec layer à confirmer** → support minimal : extraction brute blob
- **PWSZ** (vector/meta) :
  - `pwsz-lines` : segments (start/end) + bounding box
  - `pwsz-scene` : métadonnées layers/contours + bbox/area
  - pas de `mask_truth` (pas de pixels)

#### 2.1.4 Contrats transverses
- `mask_truth` = seuil 0 (canonique) quand une couche bitmap est décodable.
- Pour codecs inconnus : `mask_truth` indisponible, mais meta/previews/index + extraction brute restent possibles.

### 2.2 Module `render3d`
- Consommer `mask_truth` + (pitch XY/Z)
- Builder GPU : chunks/tiles (LOD/stride) + upload incremental
- Renderer : orbit/pan/zoom + clipping Z
- Objectifs : interaction fluide, qualité commutable

### 2.3 Module `cloud`
- Session (login/logout, refresh si applicable)
- Listing fichiers / imprimantes / jobs (selon endpoints)
- Download workflow (URL signée → GET direct)
- Upload workflow (lock/presign/PUT/register/unlock)
- Import HAR → extraction tokens

### 2.4 Module `cache`
- **RAM** : fenêtre glissante (layers/masks) + micro-LRU
- **Disque** : LRU par octets avec budgets par famille (downloads/previews/derived)
- Invalidation : file_fingerprint + algo_version + params_hash

### 2.5 Module `logging`
- JSONL, horodatage ISO-8601 TZ
- Sinks séparés : app / http / render3d
- Rotation taille + rétention 5 + gzip async
- Redaction (tokens, signatures, URLs signées)

### 2.6 Module `jobs`
- JobManager : file d’attente, priorités (UI > background)
- Opérations typiques :
  - OpenPhotonsJob (Meta/Preview/Index/Full)
  - BuildMasksJob (truth/analysis)
  - Build3DJob (LOD, upload chunks)
  - CloudDownloadJob / CloudUploadJob
  - ExportReportJob

## 3) UI moderne (Kirigami) — structure des vues

### 3.1 Fenêtre principale
- `MainWindow.qml`
  - `GlobalDrawer` (navigation)
  - `PageStack` (pages)
  - `StatusBar` (jobs/progress/op_id)

### 3.2 Pages QML
- `CloudLoginPage.qml`
- `CloudFilesPage.qml` (listing + filtres/tri + actions)
- `FileDetailsSheet.qml` (métadonnées + preview + actions)
- `ViewerPage.qml` (split view)
  - `PreviewPane.qml`
  - `LayerInspectorPane.qml` (slider layer + stats + export)
  - `Viewer3DPane.qml` (OpenGL)
- `SettingsPage.qml` (cache, perf, debug)
- `DebugPage.qml` (logs, dumps, métriques)

### 3.3 Composants réutilisables
- `BusyOverlay.qml` (annulable)
- `ErrorBanner.qml` (message + op_id)
- `ProgressCard.qml` (stage + percent)
- `FileCard.qml` (thumb + infos + boutons)

## 4) Modèles & API internes (C++)

### 4.1 Domain models
- `PhotonsDocument`
  - `format` (enum : PWMB/PWS/PHZ/PHOTONS/PWSZ_LINES/PWSZ_SCENE/UNKNOWN)
  - `capabilities` (BitmapSlices / VectorOrMeta / HasPreviews)
  - meta (machine, résolution, AA, etc.)
  - previews
  - layerIndex[] (si BitmapSlices)
- `LayerSlice`
  - decodedGray (option)
  - maskTruth (bitset)
  - bbox
- `CloudSession` / `CloudFile` / `CloudPrinter`

### 4.2 ViewModels
- `AppViewModel` (navigation globale, état, erreurs)
- `CloudViewModel` (login/listing/download/upload)
- `DocumentViewModel` (doc courant, meta, layer nav)
- `Viewer3DViewModel` (caméra, LOD, clipping)
- `SettingsViewModel` (cache, perf, debug)

### 4.3 Services
- `PhotonsService` (open/detect/decode/index)
- `MaskService` (truth/analysis + stats)
- `Render3DService` (build/upload)
- `CloudService` (API + session)
- `CacheService` (RAM/disque)
- `LogService` (sinks + redact)

## 5) Arborescence repo (proposée)

```
accloud/
  CMakeLists.txt
  cmake/
  vcpkg.json               # optionnel
  README.md
  LICENSE

  app/
    main.cpp
    App.cpp
    App.h
    resources.qrc

  ui/
    qml/
      MainWindow.qml
      pages/
        CloudLoginPage.qml
        CloudFilesPage.qml
        ViewerPage.qml
        SettingsPage.qml
        DebugPage.qml
      components/
        FileCard.qml
        BusyOverlay.qml
        ErrorBanner.qml
        ProgressCard.qml
      panes/
        PreviewPane.qml
        LayerInspectorPane.qml
        Viewer3DPane.qml
    icons/
    theme/

  domain/
    photons/
      PhotonsDocument.h
      PhotonsMeta.h
      PhotonsFormat.h
      PhotonsCapabilities.h
      LayerIndex.h
      LayerSlice.h
      drivers/
        IFormatDriver.h
    cloud/
      CloudSession.h
      CloudModels.h
    settings/
      Settings.h

  infra/
    photons/
      PhotonsReader.cpp     # probe + dispatch driver + normalisation vers PhotonsDocument
      drivers/
        pwmb/
          PwmbDriver.cpp
          PwmbTables.cpp
          PwmbSections.cpp
          codec/
            Pw0Decoder.cpp
            PwsDecoder.cpp
          preview/
            PreviewDecoder.cpp

        pws/
          PwsDriver.cpp
          codec/
            PwsDecoder.cpp

        phz/
          PhzDriver.cpp      # offsets + layer descriptors + PageNumber addressing
          preview/
            PhzPreviewDecoder.cpp

        photons/
          PhotonsDriver.cpp  # BE header + preview RGB565 + layer blobs
          preview/
            Rgb565Decoder.cpp

        pwsz/
          PwszLinesDriver.cpp
          PwszSceneDriver.cpp

    cloud/
      CloudClient.cpp
      HarImporter.cpp
      SignHeaders.cpp
    cache/
      RamWindowCache.cpp
      DiskLruCache.cpp
      CacheIndex.cpp
    logging/
      JsonlLogger.cpp
      Redactor.cpp
      Rotator.cpp
    jobs/
      JobManager.cpp
      Jobs_*.cpp

  render3d/
    gl/
      Renderer.cpp
      Renderer.h
      UploadQueue.cpp
      MeshlessVolume.cpp
    qtquick/
      QmlGlItem.cpp         # QQuickFramebufferObject / QSGRenderNode

  tests/
    photons/
      test_detect_format.cpp      # PWMB / PWS / PHZ / PHOTONS / PWSZ(lines/scene)
      test_driver_pwmb.cpp
      test_driver_pws.cpp
      test_driver_phz.cpp         # meta/index + adressage page
      test_driver_photons.cpp     # meta + preview RGB565
      test_driver_pwsz.cpp        # parsing lines/scene
      test_decode_pw0.cpp
      test_decode_pws.cpp
      test_goldens.cpp
    cloud/
      test_har_import.cpp

  tools/
    dump_photons_meta.cpp
    dump_layer_raw.cpp

  packaging/
    org.accloud.App.metainfo.xml
    desktop/org.accloud.App.desktop
    flatpak/
    appimage/
```

## 6) Flux clés (résumé)

### 6.1 Open local fichier Photon (PWMB/…)
- UI → `DocumentViewModel.open(path)`
- `JobManager` lance `OpenPhotonsJob(level=Preview/Index)`
- Affiche preview + meta
- À la demande : `FullDecodeJob` + `Build3DJob`

### 6.2 Cloud → Open
- UI sélection fichier cloud
- `CloudDownloadJob` (signed-url) → fichier local cache
- Enchaîne `OpenPhotonsJob`

### 6.3 Build 3D (non bloquant)
- `BuildMasksJob` (truth) par batch + cache fenêtre
- `Build3DJob` (chunk upload) + LOD
- UI : progress + cancel

## 7) Points non négociables (alignés CDF)
- **mask_truth = seuil 0** utilisé par 3D/mesures/exports (si bitmap décodable)
- **UI réactive** : tout >100 ms en job
- **batch + cache fenêtre** sinon app inutilisable sur gros fichiers
- **logs séparés** + redaction stricte
