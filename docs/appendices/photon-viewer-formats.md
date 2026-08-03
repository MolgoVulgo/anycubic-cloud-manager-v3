# Photon/PWMB formats and viewer — technical appendix

> Status: EXPERIMENTAL / PARTIAL. This appendix does not declare a production-ready viewer.

Status: `IMPLEMENTED` for the isolated PWSZ decode/mesh core, compact GPU representation and dynamic closed Z sections, `PARTIAL` for the Qt Quick/OpenGL desktop viewer enabled in development presets, and `SPEC` for production integration and LOD.

## Product position

The viewer remains isolated from `accloud_infra`. The `default` preset enables it, and `dev-debug` plus `local-full` inherit that setting. `prod` and `protected-core` explicitly keep `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER=OFF`.

Two dedicated validation presets also exist:

```text
experimental-viewer-core
-> no Qt
-> PWSZ decode, masks, mesh, chunks, range and camera tests

experimental-viewer-qt
-> inherits local-full
-> Qt Quick + Qt OpenGL
-> conditional 3D button on each PWSZ file row
-> temporary download followed by a viewer dialog
-> asynchronous PWSZ decode/mesh and GPU upload
```

`VolumeViewerPage.qml` and `VolumeViewerDialog.qml` are present in the normal resource bundle so the same QML source is tested and packaged consistently. The 3D action is present on every PWSZ row. When the feature flag is off, the action is disabled, the dialog is not loaded and the `Accloud.Render3D` type is not registered.

## Implemented pipeline

```text
PWSZ ZIP
-> machine/layer metadata
-> numeric layer index
-> pw0Img decode
-> bit-packed material mask
-> stacked-layer surface mesh
-> 8-layer chunks
-> eight-byte axis-aligned surface instances
-> bounded CPU-to-GPU upload queue
-> instanced OpenGL buffers without duplicated vertices/indices
-> GPU budget checked before allocation
-> exact dynamic Z clipping
-> dynamic lower/upper section surfaces derived from boundary masks
-> Qt Quick navigation and range controls
-> launch from the 3D button on the selected PWSZ file row
```

The PWSZ reading and meshing work never runs on the GUI thread. A coordinator job distributes independent mesh-chunk tasks to a configurable pool of workers. The default is 4, the accepted user range is 1 to 16, and the persisted key is `render3d.workerCount`. Adjacent tasks reload one boundary sample so horizontal transitions and vertical walls remain exact. Every surface rectangle is retained as an eight-byte `PackedSurfaceQuad`. Mesh chunks are transferred through a bounded `UploadQueue`; the render thread drains that queue and creates compact instance buffers only.

## Supported format families under study

- `PWMB`;
- `PWS`;
- `PHZ`;
- `PHOTONS`;
- `PWSZ`.

PWSZ is the current pilot format. Other drivers remain scaffold or partial until their parsing and tests are closed independently.

## PWSZ container contract

The reader supports ordinary single-disk ZIP archives with stored or Deflate entries. It rejects ZIP64, encrypted entries, unsupported compression methods, duplicate names and unsafe paths.

Required entries for the current pilot path are:

```text
anycubic_photon_resins.pwsp
layers_controller.conf
layer_images/layer_<numeric-index>.pw0Img
```

Layer files are sorted by numeric index, never lexicographically. The count must match `layers_controller.conf`, and numbering must be contiguous from zero. Independent X/Y pixel pitches are retained when the metadata provides them.

## `pw0Img` decode contract

The observed stream is mixed-width RLE:

```text
color index 0 or 15:
  two bytes, big-endian 16-bit word
  high nibble = color index
  low 12 bits = run length

color index 1 through 14:
  one byte
  high nibble = color index
  low nibble = run length
```

Rules:

- `run_len == 0` is invalid;
- truncated two-byte runs are invalid;
- the final run may be clamped if it exceeds the remaining raster;
- trailing bytes after raster completion are ignored with diagnostics;
- intermediate grey levels are detected from layer data, not inferred from slicer metadata;
- antialiasing is optional: a valid file may contain only levels `0` and `15`.

The exact 4-bit exposure values are retained only when requested. Geometry decode can omit the dense grey raster and keep only a bit-packed material mask.

## Geometry truth

```text
material truth = every non-black pixel
```

Model, supports and raft use the same exposure mask and must all be preserved. The primary mesh must not keep only the largest component, discard support tips, fill internal voids, reduce a layer to one exterior contour, merge disconnected components or require antialiasing.

The CPU mesher emits only material/void interfaces:

```text
XY neighbour transition within one layer
-> vertical wall

previous layer void, current layer material
-> lower horizontal face

current layer material, next layer void
-> upper horizontal face
```

This preserves exterior walls, interior cavity walls, through-holes, supports, raft and disconnected islands without creating one voxel cube per exposed pixel. Coplanar runs and identical vertical spans are merged where possible.

## Layer chunks and visible ranges

Meshes are split into inclusive chunks, currently 8 layers each. The 8-layer default is based on the Beetle benchmark: it reduces first-chunk latency while keeping total build time comparable to 16- and 32-layer chunks. Each chunk carries exact `first`/`last` layer numbers and a world-space bounding box. Chunk boundaries may split one coplanar wall into more triangles, but they must not produce overlapping triangles, change the exposed surface area, or alter the volume bounds.

The UI uses one-based inclusive values; the core and renderer use zero-based inclusive indices. For a 1,247-layer document:

```text
user range 415..1021
-> internal range 414..1020
-> Z clip 20.70..51.05 mm at 0.05 mm/layer
-> 607 visible layers
```

The OpenGL fragment shader clips every triangle against the exact lower and upper Z planes. Intersecting chunks are selected first to avoid drawing unrelated buffers. Slider movement therefore does not rebuild the full mesh.

When either bound cuts through the document, a dedicated background worker decodes only the boundary mask and builds one compact section surface at the corresponding Z plane. The lower section uses a negative-Z normal and the upper section a positive-Z normal. The section is derived from the exact material mask used by the active sampling mode: a solid part produces a filled cap, a hollow part preserves its cavity, and separate supports or islands remain separate. Existing horizontal faces on the clip plane are suppressed during the base-mesh pass to avoid overlap with the dynamic cap.

Visible-range changes are transactional. The renderer keeps the currently displayed range and section buffers while the replacement request is being built. The new pair of clip planes and section surfaces is uploaded into a staging buffer and committed in one frame, so the old cap is never removed before the new one is ready. Intermediate requests superseded by rapid slider movement are discarded without changing the displayed state.

Decoded XY sections are retained in a compact CPU LRU cache independent of face orientation and Z placement. Each rectangle uses 8 bytes. The cache is bounded to 64 MiB and 2,048 layers, while only the one or two active sections reside on the GPU. Returning to an already visited layer therefore avoids decoding it again and does not duplicate upper/lower copies.

## Navigation contract

The Qt Quick page maps input to the shared orbit-camera state:

- left drag: orbit;
- right, middle or Shift-drag: pan;
- wheel: zoom;
- reset action: fit the complete loaded bounding box.

The camera can inspect the piece from every side. The OpenGL shader applies simple two-sided directional lighting so interior walls remain readable when the selected Z range opens the volume.

## Backend decision

The first desktop backend uses `QQuickFramebufferObject` and Qt OpenGL because it is public, stable across the Qt 6 versions targeted by the existing project and matches the current `render3d/gl` boundary. The bootstrap forces the Qt Quick scene graph to OpenGL whenever `ACCLOUD_EXPERIMENTAL_VIEWER` is enabled.

This is not a global production rendering policy. A later QRhi backend may replace it after the supported Qt minor version is fixed and validated. Cloud, MQTT and normal QML builds remain independent of the viewer backend.

## Performance and limitations

Implemented safeguards:

- bit-packed masks;
- sequential neighbouring-layer meshing;
- default fast preview with `layerStride=2`, decoding one source layer out of two;
- exact first/last selected layers and original Z extent preserved when sampling;
- optional full-detail rebuild with `layerStride=1`;
- streamed chunk callbacks;
- bounded upload queue: 8 chunks / 256 MiB pending by default;
- no dense grey-raster retention by default;
- GUI-thread isolation;
- cancellation when the viewer is reloaded or destroyed.

The fast preview is an approximation for display only. A one-layer support or detail located exclusively on a skipped source layer can be absent until full-detail mode is selected. It never changes the PWSZ source or print data.

### Dedicated Render3D diagnostics

The viewer writes structured JSONL events with source `render3d`. The logger therefore creates `render3d.jsonl` in the configured ACM log directory (or `ACCLOUD_LOG_DIR`). Events cover archive opening, source dimensions, sampling step, requested/effective worker counts, per-worker durations, surface counts, compact bytes, legacy equivalent bytes, compression ratio, upload-queue wait, resident GPU bytes, budget, cancellations and failures. `gpu.compact_chunk_uploaded`, `gpu.cut_surface_uploaded`, `gpu.budget_exceeded` and `gpu.scene_reset` expose allocation and release accounting. `cut_surface.boundary_built` and `cut_surface.build_completed` report the decoded boundary layer, plane and compact surface count. Full temporary paths and signed URLs are not logged.

Known limits:

- compact chunks are retained on the GPU for the loaded document;
- no GPU eviction, LOD or mesh simplification yet;
- the 2 GiB budget rejects a pathological compact model cleanly instead of allowing the driver to abort the process;
- exact pixel-derived models can still contain many surfaces on dense support structures;
- no semantic colour separation between model, supports and raft;
- the desktop backend is OpenGL-only in this experimental phase.

## Validation

The offline core gate includes:

```text
accloud_experimental_viewer_architecture
accloud_experimental_viewer_scaffold
accloud_pw0_decode
accloud_pwsz_reader
accloud_layer_stack_mesher
accloud_render_pipeline
accloud_viewer_controls
```

`accloud_render_pipeline` validates compact queue accounting, the eight-byte format, exact geometry expansion, the 15× ratio, a simulated GPU budget, a complete synthetic 250 mm part, streamed chunk consumption, cancellation, renderer chunk selection, exact Z clip planes, filled solid sections, preserved hollow cavities, separated support islands, the eight-byte section-cache record and bounded LRU eviction.

Desktop validation is mandatory on a workstation with native Qt dependencies:

```bash
cmake --preset experimental-viewer-qt
cmake --build --preset experimental-viewer-qt --clean-first
ctest --preset experimental-viewer-qt --output-on-failure
```

Real PWSZ samples may be used as local validation inputs, but they are not distributable fixtures unless their redistribution rights are established.

## Decision

The viewer now has an end-to-end development path from each PWSZ file row to a navigable, range-filtered 3D mesh with compact GPU surfaces and controlled allocation budgeting. Production remains disabled. Production readiness still requires local validation on large PWSZ files and possible LOD/eviction work.
