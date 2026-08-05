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
-> conservative model/support semantic bit derived from layer geometry
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

## Estimated support semantics

PWSZ raster layers do not contain exact slicer labels for model, supports or raft. The exposure mask therefore remains the only geometry truth: non-black pixels define material, and semantic analysis may only attach a visual category to that existing material.

The Qt viewer now has two distinct runtime paths:

```text
Supports disabled
-> open the PWSZ
-> decode only the layers required by the fixed stride-two preview
-> build the classic chunked mesh with no semantic analysis

Supports enabled
-> pass 1: analyze every native PWSZ layer sequentially
-> retain a compact per-layer semantic index
-> pass 2: rebuild the usual stride-two chunked mesh from the original masks
   while materializing raft/support tags from that index
```

The disabled path does not invoke `SupportAnalyzer`, does not decode contextual layers for support classification, and passes no support-mask provider to `LayerStackMesher`. Its geometry, chunking and GPU representation remain the classic viewer contract.

The enabled path requires non-empty raft matter on layer 1, determines the end of the raft, and assigns the global phases `Raft`, `SupportsOnly`, `ModelAndSupports` and `ModelMostly`. Matter between the raft and the first detected model layer is support by construction. During mixed phases, candidate components are tracked in a rooted directed forest: one structural parent is retained, branches may split, structural branch merges are not created, and secondary contacts become braces only when their trajectory is approximately diagonal at 45 degrees. A terminal contact becomes `Head` only after a validated local taper. A support starting on an already established model must also start from a locally narrow root; a broad model protrusion cannot become a support merely because its upper end tapers.

When a tapered head reaches a non-compact model component, the analyzer treats the small-to-large section change as the semantic boundary. A branch is eligible for contact only after it has no continuation or split on the current layer; proximity to the part while the branch still narrows cannot create an early contact. The support ends on its last free terminal-head layer. The complete connected component on the following contact layer is model matter, including the exact pixels overlapping the terminal point: no support semantic is projected into the part and no later layer can inherit it. The mandatory sampling plan still retains both the last free head and the first model-contact layer so the second-pass mesh changes colour at the correct Z plane. Untapered contacts are rejected and remain model-coloured.

The analysis result stays compact. Each layer stores its phase and sorted unique identifiers for free support components; decoded masks and full semantic bitmaps are not retained. The compatibility field for projected support runs remains present but is empty for model contacts. Every validated model contact records two mandatory source samples: the last free terminal-head layer and the first contact layer. During mesh construction, `materializeLayerSemantics()` re-extracts only a retained layer and a `SupportMaskProvider` maps `Raft` and free `Support` runs to the existing support bit. When support analysis is enabled, the mesher merges those mandatory samples into the fixed quick-preview stride, then sorts and deduplicates the plan. This prevents extrusion from crossing a skipped support/model transition while keeping the contact component entirely model-coloured. When support analysis is disabled, the mandatory list is ignored and the classic stride is byte-for-byte unchanged. The result is independent from render chunk size and worker count. Dynamic lower/upper cut surfaces select the latest retained semantic sample at or below the requested material layer, so their colours remain consistent with the main mesh.

The semantic tag remains in bit 60 of `PackedSurfaceQuad`; face orientation remains in bits 61..63. The instance size stays at 8 bytes and the structural 15× reduction is unchanged. The semantic provider is validated before meshing: its dimensions must match the material mask and it cannot introduce pixels outside native PWSZ matter.

The **Supports** checkbox controls this optional path. A file initially loaded with the option disabled uses the classic path without analysis. Enabling it on such a scene launches a complete two-pass reload. Disabling it after an analyzed build immediately restores the classic colour in the shader; the already computed index may remain attached to that scene, but later loads made while the option is disabled skip analysis entirely. No mode hides, adds or removes material.

The viewport modal exposes the active phase through `viewer.loadingPhase`. Progress is weighted by actual decoding work: all native layers analyzed in pass 1 plus the effective retained-layer plan meshed in pass 2. Runtime diagnostics are written to `render3d.jsonl` with the `support_analysis` component and the events `started`, `progress`, `phase_detected`, `completed`, `skipped`, `cancelled`, `failed` and `materialization_failed`. The analysis completion reports the number of mandatory semantic samples. Mesher start/completion events report `base_sample_count`, `forced_semantic_sample_count` and `effective_sample_count`. The completion event reports free support runs, confirms zero projected model-contact runs/pixels, and exposes `terminal_support_stops`, `expanding_model_contacts`, `maximum_model_expansion_ratio`, rejected growth pixels and untapered model contacts. The older projection counters remain in the structured event for compatibility and stay zero. Logs contain aggregate counts and phase boundaries, never per-component or per-run payloads.

The diagnostic executable is available only with the experimental viewer core:

```bash
accloud_support_analysis_probe input.pwsz --output analysis.json
accloud_support_analysis_probe input.pwsz --verify-materialization
accloud_support_analysis_probe input.pwsz \
  --dump-layer 101 --dump-ppm layer-101.ppm --downsample 8
```

`--verify-materialization` rereads every native layer and checks that the compact index recreates exactly the recorded raft/support run totals. PPM dumps are diagnostics only and are not runtime assets.

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

The visible range is controlled by a vertical dual-handle slider fixed to the right edge of the viewport. The maximum layer is shown above it and layer `1` below it. Hovering either handle displays its current layer number in a tooltip. Wheel input over the control changes only the upper bound; the lower bound remains mouse-drag only. The previous numeric bound inputs are no longer exposed.

The viewer dialog title remains generic (`3D view`). The viewport overlay displays, in order, the machine name, then `name.pwsz · N layers`, then the navigation hint. It does not show worker count, sampling mode, chunk/triangle counts, visible-range bounds or Z values. A compact **Supports** checkbox is anchored to the lower-left corner of the viewport and enables the estimated support colour. In the header, **Reset view** is placed immediately to the left of **Full screen**; **Exit full screen** restores the workspace size. The dialog footer places **Print** immediately to the left of **Close**. **Print** closes the viewer and triggers exactly the same remote-print configuration flow as the **Print** action in the cloud-file listing.
During the whole initial reconstruction, a modal limited to the viewport covers only the viewer area and blocks its interactions. A determinate progress bar is centred in that modal and follows `viewer.progress` from 0 to 100%. The modal remains visible while `viewer.loading` is true, even when partial chunks are already available, then disappears when construction finishes or fails. The dialog header and footer remain outside this modal.

When either bound cuts through the document, a dedicated background worker decodes only the boundary mask and builds one compact section surface at the corresponding Z plane. The lower section uses a negative-Z normal and the upper section a positive-Z normal. The section is derived from the exact material mask corresponding to the fixed `layerStride=2` preview: a solid part produces a filled cap, a hollow part preserves its cavity, and separate supports or islands remain separate. Existing horizontal faces on the clip plane are suppressed during the base-mesh pass to avoid overlap with the dynamic cap.

Visible-range changes are transactional. The renderer keeps the currently displayed range and section buffers while the replacement request is being built. The new pair of clip planes and section surfaces is uploaded into a staging buffer and committed in one frame, so the old cap is never removed before the new one is ready. Intermediate requests superseded by rapid slider movement are discarded without changing the displayed state.

Decoded XY sections are retained in a compact CPU LRU cache independent of face orientation and Z placement. Each rectangle uses 8 bytes. The cache is bounded to 64 MiB and 2,048 layers, while only the one or two active sections reside on the GPU. Returning to an already visited layer therefore avoids decoding it again and does not duplicate upper/lower copies.

## Navigation contract

The Qt Quick page maps input to the shared orbit-camera state:

- left drag: orbit;
- right, middle or Shift-drag: pan in the screen plane;
- wheel: zoom;
- reset action: fit the complete loaded bounding box.

Pan uses the camera's actual right/up axes. Its displacement remains orthogonal to the view direction, so horizontal or vertical dragging cannot move the piece forward or backward at the same time.

The camera can inspect the piece from every side. The OpenGL shader applies simple two-sided directional lighting so interior walls remain readable when the selected Z range opens the volume.

## Backend decision

The first desktop backend uses `QQuickFramebufferObject` and Qt OpenGL because it is public, stable across the Qt 6 versions targeted by the existing project and matches the current `render3d/gl` boundary. The bootstrap forces the Qt Quick scene graph to OpenGL whenever `ACCLOUD_EXPERIMENTAL_VIEWER` is enabled.

This is not a global production rendering policy. A later QRhi backend may replace it after the supported Qt minor version is fixed and validated. Cloud, MQTT and normal QML builds remain independent of the viewer backend.

## Performance and limitations

Implemented safeguards:

- bit-packed masks;
- sequential neighbouring-layer meshing;
- single rendering mode with base `layerStride=2`; when support analysis is enabled, only mandatory terminal/contact transition layers supplement that base plan;
- exact first/last selected layers and original Z extent preserved when sampling;
- streamed chunk callbacks;
- bounded upload queue: 8 chunks / 256 MiB pending by default;
- no dense grey-raster retention by default;
- GUI-thread isolation;
- cancellation when the viewer is reloaded or destroyed.

The one-layer-out-of-two rendering is an intentional display approximation: the viewer is meant to identify and inspect a part quickly, including third-party files. With support analysis disabled, a support or detail located only on a skipped source layer may be absent. With support analysis enabled, only the last free head and first model-contact layers are added to the plan; unrelated details remain subject to the same approximation. No full-detail mode is exposed, and the PWSZ source and print data are never changed.

### Dedicated Render3D diagnostics

The viewer writes structured JSONL events with source `render3d`. The logger therefore creates `render3d.jsonl` in the configured ACM log directory (or `ACCLOUD_LOG_DIR`). Events cover archive opening, source dimensions, sampling step, requested/effective worker counts, per-worker durations, surface counts, compact bytes, legacy equivalent bytes, compression ratio, upload-queue wait, resident GPU bytes, budget, cancellations and failures. `gpu.compact_chunk_uploaded`, `gpu.cut_surface_uploaded`, `gpu.budget_exceeded` and `gpu.scene_reset` expose allocation and release accounting. `cut_surface.boundary_built` and `cut_surface.build_completed` report the decoded boundary layer, plane and compact surface count. Full temporary paths and signed URLs are not logged.

Known limits:

- compact chunks are retained on the GPU for the loaded document;
- no GPU eviction, LOD or mesh simplification yet;
- the 2 GiB budget rejects a pathological compact model cleanly instead of allowing the driver to abort the process;
- exact pixel-derived models can still contain many surfaces on dense support structures;
- support colouring is heuristic because the PWSZ exposure mask contains no exact slicer semantic labels; fused or ambiguous material deliberately keeps the model colour;
- the desktop backend is OpenGL-only in this experimental phase.

## Validation

The offline core gate includes:

```text
accloud_experimental_viewer_architecture
accloud_experimental_viewer_scaffold
accloud_pw0_decode
accloud_pwsz_reader
accloud_layer_stack_mesher
accloud_support_analyzer
accloud_render_pipeline
accloud_viewer_controls
```

`accloud_support_analyzer` validates the mandatory raft, support-only and mixed phases, a rooted non-merging support forest, branch splits, approximately-45-degree braces, model-rooted supports, terminal tapering, deferred contact until a branch has no structural child, hollow-model rejection, deterministic compact component indices, cancellation and exact per-layer rematerialization.

`accloud_render_pipeline` validates compact queue accounting, the eight-byte format, semantic bit encoding, exact geometry expansion, the unchanged 15× ratio, a simulated GPU budget, a complete synthetic 250 mm part, streamed chunk consumption, cancellation, renderer chunk selection, exact Z clip planes, filled solid sections, preserved hollow cavities, separated support islands, conservative support/fusion/raft classification, semantic preservation through the eight-byte section cache and bounded LRU eviction.

Desktop validation is mandatory on a workstation with native Qt dependencies:

```bash
cmake --preset experimental-viewer-qt
cmake --build --preset experimental-viewer-qt --clean-first
ctest --preset experimental-viewer-qt --output-on-failure
```

Real PWSZ samples may be used as local validation inputs, but they are not distributable fixtures unless their redistribution rights are established.

## Decision

The viewer now has an end-to-end development path from each PWSZ file row to a navigable, range-filtered 3D mesh with compact GPU surfaces and controlled allocation budgeting. Production remains disabled. Production readiness still requires local validation on large PWSZ files and possible LOD/eviction work.
