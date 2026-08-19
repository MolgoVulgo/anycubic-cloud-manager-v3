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
-> decode/describe every native PWSZ layer once while semantic pass 1
   propagates support provenance from the first layer after the raft
-> semantic pass 2 (top-down): reuse the retained compact layer descriptions
   and propagate independent model provenance downward
-> reconcile both provenances per component/run into support, model or mixed matter
-> rebuild the usual stride-two chunked mesh from the original masks while
   materializing the reconciled raft/support index
```

The disabled path does not invoke `SupportAnalyzer`, does not decode contextual layers for support classification, and passes no support-mask provider to `LayerStackMesher`. Its geometry, chunking and GPU representation remain the classic viewer contract.

The enabled path requires non-empty raft matter on layer 1 and assigns the global phases `Raft`, `SupportsOnly`, `ModelAndSupports` and `ModelMostly`. The raft is the repeated native-mask prefix that starts on layer 1; a bounded number of changed antialias pixels is tolerated, but no fixed layer count or physical-height window defines it. Its first materially different successor is the first support layer and is the root truth of the bottom-up pass. Independently, every exposed pixel on the final native layer is the root truth of the top-down model pass. The PWSZ layer height is used for Z placement only and cannot change semantic classification for an identical layer sequence.

The analyzer first builds the same native connected-component/relationship information while walking upward, but semantic pass 1 now records **support evidence**, not a final model verdict. Raft-rooted provenance is propagated through overlap, exact material distance, bounded motion, branches and support fusions. Surface ratios, taper history, motion residuals and fusion coverage are confidence/evidence values: they may mark an edge as a plausible terminal support boundary, but they do not independently own the final semantic classification. Semantic pass 2 reuses the retained compact layer descriptions in reverse order and propagates an independent sparse model mask downward. A model island disconnected from the current reverse lineage can seed a new model lineage only when pass 1 does not already claim it as support provenance and never below the earliest model layer observed by the bottom-up pass. Reconciliation then compares both provenances on the same component. Support-only evidence yields support, model-only evidence yields model, and simultaneous evidence is partitioned into sparse mixed runs instead of forcing the whole connected component into one category.

Support and model are not separated by an absolute component diameter or area. Light, normal and heavy supports are followed through their relative section evolution. During mixed phases, components are tracked in a rooted directed forest: one structural parent is retained, branches may split, structural branch merges are not created, and secondary contacts become braces only when their trajectory has the configured layer-native raster drift. Parent matching uses overlap between native raster runs and the exact run-to-run material distance; overlap of bounding boxes alone cannot create a support parent through an empty hole. A branch rooted in the raft keeps its support identity across local section changes, temporary raster merges and overlap with an already established model component. Such overlap alone cannot cut the branch. If at least two previous support sections remain substantially preserved and together explain a significant part of the current component, the event is classified as `support_fusion_continuation`: the merged component remains support instead of opening a part contact. Only a model-dominant connected merge that is far larger than the recent support profile can bypass that continuity, and a tapered expansion is always handled as a provisional contact first. A support starting on an already established model must also start from a locally narrow root; a broad model protrusion cannot become a support merely because its upper end tapers.

In the bottom-up evidence pass, a reduction followed by local growth opens the state `contact candidate`; it does not immediately turn the current component into model matter and is no longer sufficient to settle the final semantic boundary. A terminal taper is not established by one old drop relative to the largest section in the lookback window. The relative reduction must also be supported by an immediate final collapse, by several meaningful decreases in the recent lineage, or by a bounded rebound after such a real taper. A stable support plateau following an earlier crossbar or branch fusion therefore cannot become a terminal tip. The decision is local to the tracked branch and never depends on model matter existing elsewhere in the print. A single growth of at least the configured relative threshold may open the candidate. A smaller growth may open it only when the centre also leaves the normal per-layer motion envelope **and** the parent section, translated onto the current centre, no longer preserves the configured support-shape overlap. Centre displacement alone is never evidence of a part contact. If the translated shape and the predicted branch trajectory remain coherent, the component stays support with the diagnostic reason `support_motion_continuation`. A candidate must then persist. It is confirmed either by a locally abrupt first section relative to the terminal tip or by cumulative growth from the first candidate layer across the following native layers. Stationary, shrinking, motion-explained, fusion-explained or otherwise unconfirmed sequences are committed back to the support branch. This keeps inclined supports, braces, local widening and temporary fusions as support while allowing a progressively growing part section to be reclassified from its true first layer.

A forward contact confirmation remains useful evidence and still records the terminal/head sampling pair, but the final boundary is committed only after descending model provenance reaches the same area. When the two passes agree, the last valid terminal section and its lower support ancestors remain support while the model-side region is model. When they disagree, the current pass-1 support core is protected rather than eroded from only one parent intersection; reverse model matter cannot leak pixel-by-pixel through a support continuation. Conversely, when all active raft-rooted parent provenances terminate on the lower edge and descending model evidence reaches the upper component, the support core is released and the upper region becomes model. The mandatory sampling plan retains the validated terminal/model boundary so the mesh changes colour at the correct Z plane.

The reverse pass owns model provenance. It starts from the final native layer, follows exact overlap or a bounded translation that maximizes retained model matter, and uses the next older reverse mask to disambiguate equal-overlap shifts. When support evidence competes for an equal-overlap translation, the selected model shift maximizes retained model pixels outside that support core before historical/shortest-shift tie breaking. This prevents model lineage from jumping onto a nearby support. In a mixed component, the current pass-1 support core is retained as a whole when at least one active support parent still continues; reconstructing only the immediate parent intersection would progressively erode moving supports. Model pixels are therefore the connected remainder outside that protected core. If every active support parent provides terminal evidence on the lower edge, the core may end there and reverse model provenance claims the upper region. This symmetric arbitration allows model continuity to survive later support attachments without allowing support provenance to consume the model or reverse model provenance to consume a still-continuing support.

The analysis result stays compact. Each layer stores its phase, sorted unique identifiers for whole support components and sparse projected support runs only for mixed components; decoded masks and full semantic bitmaps are not retained in the final index. Every validated model contact records two mandatory source samples: the last free terminal-head layer and the first contact layer. During mesh construction, `materializeLayerSemantics()` re-extracts only a retained layer and a `SupportMaskProvider` maps `Raft`, whole `Support` components and projected support runs to the existing support bit. The compact final index is produced after the two independent semantic traversals. Each native PWSZ layer is now decoded and submitted to connected-component extraction only once, during the bottom-up preparation window. The resulting compact `LayerDescription` data (component bounds, areas, centres and native runs) is retained for the duration of support analysis and reused directly by the top-down semantic pass. P6.5 prepares all compact native descriptions before semantic reconciliation, then partitions every adjacent semantic layer pair into contiguous worker lots. Each lot builds immutable sparse evidence edges containing overlap, exact material distance and centre distance; because the pair crossing a lot boundary is itself an edge, continuity is explicit and this stage needs no duplicated halo. Forward and reverse graph mutation/classification commits remain strictly ordered between native layers and in original component order, preserving the existing deterministic semantic authority. P4's persistent priority scheduler remains the single worker resource for this preparation, evidence construction and per-component semantic work instead of stacking analyzer-local pools. P6.3 sizes the preparation window adaptively from the configured worker count and a 256 MiB default native-mask memory budget; on a workstation a 16-worker run may therefore keep up to 16 preparations in flight when the raster size permits it, while masks are still released after the raft comparison no longer needs them. Sources that opt into concurrent mask loads use that bounded window directly; other sources serialize only `loadMask()` while the shared scheduler continues to parallelize the independent work around it. This removes analyzer-local pool oversubscription, keeps 1-worker and N-worker semantic results identical and allows the ordered coordinator to overlap with preparation without widening retained-mask memory with the semantic worker count. Parent match geometry is evaluated once per candidate pair and the resulting overlap/material-distance/centre-distance tuple is reused for structural classification, contact checks and same-layer semantic propagation instead of recomputing the same pair. Full semantic bitmaps are still not retained between layers or across passes. When support analysis is enabled, the mesher merges mandatory samples into the fixed quick-preview stride, then sorts and deduplicates the plan. This prevents extrusion from crossing a skipped support/model transition while preserving both semantics through mixed components. When support analysis is disabled, the mandatory list is ignored and the classic stride is byte-for-byte unchanged. The semantic result is independent from render chunk size and worker count. Allocation pressure inside this CPU path is bounded further by reusing scanline and per-layer preparation buffers, using dense disjoint-set root indexing with exact per-component run reservation, keeping graph nodes as references to the immutable retained components instead of duplicating run vectors, and normalizing/intersecting sparse run masks in place over touched rows only. P5 adds a portable CPU acceleration layer without changing those semantics: connected-component scanlines read the contiguous `BinaryMask` word rows directly and, on x86 CPUs with AVX2, skip four zero 64-bit words at a time before falling back to the same scalar run extraction. Highly fragmented sparse rows may also be promoted to bounded 64-bit bitsets only when the bitset word footprint is at most one quarter of the canonical interval count; `countSet`, translated overlap and mask intersection can then use word operations, with AVX2 intersection dispatch on supported x86 CPUs and the scalar implementation everywhere else. Sparse runs remain authoritative, the bitset cache is row-local/transient, and full semantic bitmaps are still never retained across layers. Dynamic lower/upper cut surfaces select the latest retained semantic sample at or below the requested material layer, so their colours remain consistent with the main mesh.


P6/P6.1/P6.2 adds an optional Vulkan Compute accelerator for the most regular hot loop of reverse/model lineage matching: translated-overlap counts. It does **not** move the semantic graph, layer ordering, tie-breaking, reconciliation or commit logic to the GPU. `auto` is the normal hybrid runtime mode. CMake enables the backend only when the Vulkan SDK and a SPIR-V shader compiler (`glslc` or `glslangValidator`) are both available; otherwise the same binary path is built with the canonical CPU implementation only. At runtime, `auto` also falls back to CPU when no usable Vulkan compute queue can be created or when a particular GPU job fails. `cpu` forces the reference implementation. The former full-GPU `vulkan` runtime preference has been removed. Only components at or above `vulkanMinimumComponentAreaPixels` (32,768 pixels by default) are candidates for GPU work, so small sparse components stay on CPU and avoid transfer/dispatch overhead. P6.1 introduced concurrent request coalescing through a dedicated Vulkan dispatcher with persistent staging and reusable `DEVICE_LOCAL` buffers. P6.2 changes the shader geometry from one long-running workgroup per translation to a 3D grid of `tile × translation × job`. Dense fallback jobs use 4096-word tiles; the main stable-model lineage path avoids rebuilding a dense source bitmap and uploads the component's canonical semantic runs in 64-run tiles. Each tile produces a partial popcount which is reduced into the translation counter on the GPU. The full stable-model reference is rasterised once per semantic layer and tagged with a generation key so compatible jobs can reuse the same device-local reference instead of uploading it per component. Concurrent jobs are still coalesced up to 64 requests subject to Vulkan storage-buffer and workgroup-count limits, and results return to the exact CPU candidate ordering and deterministic tie-breaking. This increases workgroup-level parallelism and reduces host rasterisation/PCIe traffic without making GPU results a semantic authority. The backend remains independent from the OpenGL 3.3 renderer and retains a mandatory CPU fallback.

The semantic tag remains in bit 60 of `PackedSurfaceQuad`; face orientation remains in bits 61..63. The instance size stays at 8 bytes and the structural 15× reduction is unchanged. The semantic provider is validated before meshing: its dimensions must match the material mask and it cannot introduce pixels outside native PWSZ matter.

The **Supports** checkbox controls this optional path. A file initially loaded with the option disabled uses the classic path without analysis. Enabling it on such a scene launches both semantic traversals and then rebuilds the preview mesh. Disabling it after an analyzed build immediately restores the classic colour in the shader; the already computed index may remain attached to that scene, but later loads made while the option is disabled skip analysis entirely. No mode hides, adds or removes material.

The viewport modal exposes the active phase through `viewer.loadingPhase`. P6.5 keeps progress monotonic across three support-analysis work phases before meshing: native geometry/evidence preparation, forward semantic reconciliation and reverse semantic reconciliation. The effective retained-layer plan is then meshed for the preview. Runtime diagnostics are written to `render3d.jsonl` with the `support_analysis` component and the events `started`, `progress`, `phase_detected`, `completed`, `skipped`, `cancelled`, `failed` and `materialization_failed`. The `started` event records `requested_workers` and whether the source advertises `concurrent_mask_loads`; the analysis completion reports the number of mandatory semantic samples. Mesher start/completion events report `base_sample_count`, `forced_semantic_sample_count` and `effective_sample_count`. The completion event reports whole-component support runs, projected support runs used by mixed components, `terminal_support_stops`, `expanding_model_contacts`, `maximum_model_expansion_ratio`, rejected growth pixels and untapered model contacts. `projected_contact_pixels` remains zero because support pixels are never projected into a confirmed model-contact region. Logs contain aggregate counts and phase boundaries, never per-component or per-run payloads.

The diagnostic executable is available only with the experimental viewer core:

```bash
accloud_support_analysis_probe input.pwsz --output analysis.json --workers 4
accloud_support_analysis_probe input.pwsz --verify-materialization --workers 4
accloud_support_analysis_probe input.pwsz \
  --dump-layer 101 --dump-ppm layer-101.ppm --downsample 8 --workers 4
```

`--workers N` accepts `1..16`, defaults to `1` for reproducible diagnostics, and sizes the shared support-analysis worker pool used by bounded layer preparation, forward component classification, model-lineage search and reverse reconciliation. P6.3 makes the preparation window adaptive (worker count + 256 MiB default native-mask budget) instead of fixing it at four, while semantic output must remain identical. The JSON report records the selected value as `analysis_workers`. `--verify-materialization` rereads every native layer and checks that the compact index recreates exactly the recorded raft/support run totals. `--compute auto|cpu` selects the standard P6 compute mode and `--vulkan-min-area N` changes only the component-area threshold used to decide whether translated-overlap work is worth batching on Vulkan. The JSON summary reports whether Vulkan Compute was compiled/activated, the selected device, eligible/submitted/GPU/fallback job counts, successful/failed dispatches, maximum coalesced batch size, transfer bytes, host preparation time, queue-wait time and batch execution time. P6.2 also exposes compact-run jobs, resident-reference uploads/reuses and the number of submitted workgroups; on a real accelerated run, `vulkan_submitted_workgroups` should be far larger than the GPU job count because each large component is split into many tiles. The viewer emits the activation/device status immediately and repeats the live compute counters in decile progress logs, so a cancelled long analysis is still diagnosable. P6.3 adds preparation-window, prepared-layer, maximum-inflight and phase timing counters to the same progress logs and JSON summary. P6.4 adds semantic layer-batch call/job counters plus forward classification/commit/lineage/lineage-commit and reverse prepare/commit timings. The long-file benchmark showed the zero-shift layer batch to be slower in normal `auto` because the backend still creates one host request per component; the stabilisation removes that runtime path entirely. `auto` returns exact/envelope zero-shift counts to sparse CPU masks while retaining Vulkan translated-lineage acceleration, and `cpu` uses the canonical CPU path throughout. P6.5 adds `support_semantic_evidence_us`, `support_semantic_evidence_lots`, `support_semantic_evidence_layer_pairs` and `support_semantic_evidence_edges` for the parallel adjacent-layer graph build, and bounds several per-layer scratch arrays by the immediately previous candidate layer rather than cumulative graph size. P3 further hardens the acceleration/semantics boundary for both traversals: `prepareTranslatedOverlapEvidence()` may use the CPU or Vulkan path to produce translated-overlap counts only, then `selectBestLineageMotion()` applies the same historical deterministic CPU tie-breaks without knowing the compute backend. The GPU therefore cannot select a lineage or alter a parent, contact, or support/model category. PPM dumps are diagnostics only and are not runtime assets.

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
accloud_support_analysis_diagnostics
accloud_render3d_worker_benchmark_selftest
accloud_render_pipeline
accloud_viewer_controls
```

`accloud_support_analyzer` validates plate, grid and pad raft prefixes with bounded antialias variation, support-only and mixed phases, a rooted non-merging support forest, branch splits, layer-native braces, light/normal/heavy diameter independence, small and progressively growing first-part sections after a tapered tip, translated Torus-like taper continuations that must remain support, stable plateaus after an old taper that must not reopen a contact, multi-parent support fusions that remain support, rejection of false parents through empty bounding-box holes, structural-parent-only local contact decisions, PWSZ layer-height invariance of semantics, model-rooted supports, deferred contact until a branch has no structural child, persistent model lineage after a confirmed contact, bounded motion-aware model continuity through consecutive raster displacements, independent support-footprint prediction inside a mixed component, semantic partition of one raster-connected mixed model/support component, hollow-model rejection, deterministic compact component indices, cancellation and exact per-layer rematerialization.

`accloud_support_analysis_diagnostics` validates the complete bundle schema, three visible PNG panels plus one aligned pick-map PNG and one lazy JSON file per layer, selection identifiers, crop geometry, surface comparisons, selected semantics, stable reason codes and human-readable decision explanations.

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

## Complete support-analysis diagnostic bundle

The development probe can export the complete decision trace for one local PWSZ file:

```bash
./build/experimental-viewer-core/accloud_support_analysis_probe \
  ../pwsz/Beetle.pwsz \
  --bundle /tmp/beetle-support-analysis \
  --verify-materialization
```

The bundle is intended for development diagnostics and contains:

```text
manifest.json                 layer/image index and source metadata
summary.json                  compact global summary used by the UI
analysis.json                 complete layer, node and edge analysis
decisions.json                every semantic decision and its comparisons
images/layer_XXXXXX_raw.png       raw exposure mask
images/layer_XXXXXX_semantic.png  model/support/raft result
images/layer_XXXXXX_nodes.png     decisions and rendered node identifiers
images/layer_XXXXXX_pick.png      hidden RGB24 component selection map
layers/layer_XXXXXX.json          lazy per-layer decisions and image geometry
```

`decisions.json` records the current and parent surfaces, their ratio, raw and centre-aligned overlap, exact material distance, added and removed pixels after alignment, predicted branch motion, motion residual, all matched support-parent node identifiers and overlaps, preserved-parent counts and coverage, terminal-taper decrease evidence, state before and after the decision, the selected semantic, a stable `reason_code` and a human-readable `why` field. Mixed-component decisions additionally expose `model_lineage_pixels`, `model_lineage_shift_pixels` and `model_lineage_continued`, plus `reverse_model_evidence_pixels`, `reverse_support_core_pixels`, `final_support_pixels`, `final_model_pixels`, `reverse_model_lineage_continued`, `reverse_model_seed` and `bidirectional_conflict`. These fields make the bottom-up support evidence, top-down model evidence and final reconciliation auditable. Decision tracing is opt-in and is not enabled by the normal viewer path. Each source layer exports three separate full-width images: raw mask, semantic result and decision overlay. The decision image prints the real `node_id` on model, contact, mixed and non-standard support decisions; default support continuations remain unlabeled to keep dense support grids readable. A fourth hidden pick image encodes `component_id + 1` as RGB24 for every downsampled diagnostic pixel. The per-layer JSON records the crop bounds, image size, panel paths and the same layer-local `selection_id`.

With `ACCLOUD_DEBUG` and the experimental viewer enabled, `MainWindow.qml` exposes the **Support analysis** tab. The user selects a local `.pwsz` part, then `SupportAnalysisBridge` launches the probe asynchronously beside the desktop executable. The tab is divided into a synchronized 3D viewer, a vertically scrollable stack of the three diagnostic images and a lower JSON inspector. Direct buttons jump to the raw, semantic or node-ID view and the vertical scrollbar remains available. Clicking the semantic or node-ID image reads the hidden pick map, selects the exact component under the cursor, outlines its diagnostic bounds and replaces the decisions array with the matching decision object in the JSON inspector. Changing the viewer range or layer selector clears the selection and updates all three images and the per-layer JSON. Large global files are not parsed eagerly by QML: the bridge loads `summary.json` and the selected `layers/layer_XXXXXX.json` only. The tab and bridge are absent from production resources.

