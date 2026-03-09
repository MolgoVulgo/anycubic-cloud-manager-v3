# Etat reel vs cible

Date de reference: 2026-03-08

Ce document se base sur l'etat de code actuel de la branche `docs`.

## Resume executif

- Base code source utile (hors `build/`, logs, cache pyc): ~20512 lignes.
- Zones matures: Cloud API, import HAR/session, UI Files/Printers/Logs, cache SQLite local, logging JSONL.
- Zones incompletes: pipeline Photons, rendu 3D natif, jobs infra, cache infra (hors SQLite app), outils photons.
- Fichiers scaffold detects: 39.

## Matrice modules

| Module | Etat reel | Cible doc | Ecart |
| --- | --- | --- | --- |
| App bootstrap / bridges (`accloud/app`) | Implemente | Stable | Faible |
| Cloud client + signatures (`accloud/infra/cloud`) | Implemente | Stable | Faible |
| Session HAR + persistance | Implemente | Stable | Faible |
| UI MainWindow + Files/Printers/Logs | Implemente | Stable | Faible a moyen |
| Cache local SQLite (`LocalCacheStore`) | Implemente | Stable | Faible |
| Logging runtime JSONL | Implemente | Stable | Faible |
| Domain photons (`accloud/domain/photons`) | Contrats type only | Decode multi-format complet | Moyen |
| Infra photons (`accloud/infra/photons`) | Scaffold | Decode + index + previews | Eleve |
| Render3D (`accloud/render3d`) | Scaffold (placeholder) | Viewer OpenGL complet | Eleve |
| Jobs (`accloud/infra/jobs`) | Scaffold | Orchestration async | Eleve |
| Cache infra (`accloud/infra/cache`) | Scaffold | RAM + disk cache LRU | Eleve |
| Tests photons | Scaffold | Non regression decode formats | Eleve |
| Outils photons | Scaffold | Dumps/inspection formats | Eleve |

## Elements verifies comme implementes

- Point d'entree et cycle de vie: `accloud/app/main.cpp`, `accloud/app/App.cpp`.
- Pont cloud QML + telechargement + refresh cache/cloud: `accloud/app/CloudBridge.cpp`.
- Cache SQLite local (files/printers/jobs/quota/sync_state): `accloud/app/LocalCacheStore.cpp`.
- Client API Workbench + signatures XX-* + remote print: `accloud/infra/cloud/CloudClient.cpp`, `accloud/infra/cloud/SignHeaders.cpp`.
- Import HAR/session + persistance 0600: `accloud/infra/cloud/HarImporter.cpp`.
- UI centrale: `accloud/ui/qml/MainWindow.qml`, `accloud/ui/qml/pages/CloudFilesPage.qml`, `accloud/ui/qml/pages/PrinterPage.qml`, `accloud/ui/qml/pages/LogPage.qml`.
- i18n runtime (bridge + TS/QM): `accloud/app/AppI18nBridge.cpp`, `accloud/i18n/accloud_en.ts`, `accloud/i18n/accloud_fr.ts`.

## Fichiers scaffold (39)

Les fichiers suivants contiennent explicitement `Scaffold placeholder`:

- `accloud/infra/cache/CacheIndex.cpp`
- `accloud/infra/cache/DiskLruCache.cpp`
- `accloud/infra/cache/RamWindowCache.cpp`
- `accloud/infra/jobs/JobManager.cpp`
- `accloud/infra/jobs/Jobs_Build3D.cpp`
- `accloud/infra/jobs/Jobs_BuildMasks.cpp`
- `accloud/infra/jobs/Jobs_Cloud.cpp`
- `accloud/infra/jobs/Jobs_ExportReport.cpp`
- `accloud/infra/jobs/Jobs_OpenPhotons.cpp`
- `accloud/infra/photons/PhotonsReader.cpp`
- `accloud/infra/photons/drivers/photons/PhotonsDriver.cpp`
- `accloud/infra/photons/drivers/photons/preview/Rgb565Decoder.cpp`
- `accloud/infra/photons/drivers/phz/PhzDriver.cpp`
- `accloud/infra/photons/drivers/phz/preview/PhzPreviewDecoder.cpp`
- `accloud/infra/photons/drivers/pwmb/PwmbDriver.cpp`
- `accloud/infra/photons/drivers/pwmb/PwmbSections.cpp`
- `accloud/infra/photons/drivers/pwmb/PwmbTables.cpp`
- `accloud/infra/photons/drivers/pwmb/codec/Pw0Decoder.cpp`
- `accloud/infra/photons/drivers/pwmb/codec/PwsDecoder.cpp`
- `accloud/infra/photons/drivers/pwmb/preview/PreviewDecoder.cpp`
- `accloud/infra/photons/drivers/pws/PwsDriver.cpp`
- `accloud/infra/photons/drivers/pws/codec/PwsDecoder.cpp`
- `accloud/infra/photons/drivers/pwsz/PwszLinesDriver.cpp`
- `accloud/infra/photons/drivers/pwsz/PwszSceneDriver.cpp`
- `accloud/render3d/gl/MeshlessVolume.cpp`
- `accloud/render3d/gl/Renderer.cpp`
- `accloud/render3d/gl/UploadQueue.cpp`
- `accloud/render3d/qtquick/QmlGlItem.cpp`
- `accloud/tests/photons/test_decode_pw0.cpp`
- `accloud/tests/photons/test_decode_pws.cpp`
- `accloud/tests/photons/test_detect_format.cpp`
- `accloud/tests/photons/test_driver_photons.cpp`
- `accloud/tests/photons/test_driver_phz.cpp`
- `accloud/tests/photons/test_driver_pwmb.cpp`
- `accloud/tests/photons/test_driver_pws.cpp`
- `accloud/tests/photons/test_driver_pwsz.cpp`
- `accloud/tests/photons/test_goldens.cpp`
- `accloud/tools/dump_layer_raw.cpp`
- `accloud/tools/dump_photons_meta.cpp`

## Tests de reference

Derniere execution verifiee:

```bash
ctest --preset default --output-on-failure
```

Resultat: `3/3` passes (`accloud_ui_qml`, `accloud_smoke`, `accloud_har_import`).

## Utilisation de ce document

- Ce document est normatif pour le statut implementation.
- Les docs de type `SPEC` doivent pointer ici pour eviter de presenter une cible comme deja livree.
