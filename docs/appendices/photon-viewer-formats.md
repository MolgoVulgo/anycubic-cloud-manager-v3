# Photon/PWMB formats and viewer — technical appendix

> Status: EXPERIMENTAL / PARTIAL. This appendix does not declare a production-ready viewer.


Status: `PARTIAL` for implementation, `SPEC` for the target viewer contract.

## Product position

The Photon/PWMB viewer is a project trajectory, not the main finished workflow. The repository contains domain contracts plus placeholder format drivers, decode components, jobs, future RAM/disk cache components and a `render3d` skeleton. These scaffold `.cpp` files are excluded from normal builds and from `accloud_cli`.

The only supported opt-in is `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER=ON`, exposed by the `experimental-viewer-core` preset. It builds the isolated `accloud_experimental_viewer` object target and a compile smoke test. It does not expose viewer controls in production QML, decode real files or provide a render/navigation/export workflow.

## Supported format families under study

- `PWMB`;
- `PWS`;
- `PHZ`;
- `PHOTONS`;
- `PWSZ`.

Each format must be handled by a driver. The driver contract should expose metadata, previews, layer index, decode levels and diagnostics without forcing full decode at open time.

## PWMB parsing rule

PWMB must be parsed tables-first:

```text
FileMark -> table addresses -> section table -> sections -> layer index -> layer decode
```

Requirements:

- validate signature such as `ANYCUBIC` and version;
- support section versions;
- tolerate unknown sections;
- never assume table addresses are sorted;
- bound every read;
- support legacy fallback only when the table path is incomplete.

## Versioned sections

Observed version gates include:

| Version | Section family |
| --- | --- |
| `>=515` | `LayerImageColorTable` |
| `>=516` | `Extra`, `Machine` |
| `>=517` | `Software`, `Model` |
| `>=518` | `SubLayerDefinition`, `Preview2` |

Missing optional sections must produce warnings, not crashes.

## Layer decode contract

Decode selection uses `Machine.LayerImageFormat`.

### `pw0Img`

- stream is read as 16-bit big-endian words;
- high nibble is `color_index`;
- low 12 bits are `run_len`;
- `run_len == 0` is invalid;
- final run can be clamped when it exceeds `W*H`;
- trailing data after image completion is ignored with diagnostics.

### `pwsImg`

- byte RLE;
- bit 7 indicates exposed state;
- bits 0..6 encode repetition count;
- both `run_len = reps` and `run_len = reps + 1` conventions must be handled by deterministic dry-run selection;
- anti-aliasing passes accumulate exposed counts and project to `uint8`.

## Geometry truth

The viewer’s geometry rule is non-negotiable:

```text
material truth = every non-black pixel
```

This means threshold 0. Anti-aliased grey pixels are still material for geometry, measurement and future exports.

Two masks must remain separate:

| Mask | Rule | Allowed use |
| --- | --- | --- |
| `mask_truth` | threshold 0 / non-black | viewer geometry, dimensions, area, volume, exports, goldens. |
| `mask_analysis` | configurable heuristic | issue analysis only, such as islands, traps or overhang heuristics. |

The viewer must not build primary geometry from contour vectorization. Contours can be optional analysis/export material, but they are not the source of truth for the main 3D representation.

## Raster and world mapping

- decoded raster is flat row-major `W*H`;
- `x = i % W`, `y = i // W`;
- image origin is top-left;
- XY pitch is `PixelSizeUm / 1000` in mm;
- Z pitch is `LayerHeight`;
- world-space Y is inverted to represent Y-up.

## Performance model

The viewer must be asynchronous and cache-aware:

- open metadata/previews quickly;
- build layer index before full decode;
- decode and mask layers in batches;
- use a RAM sliding window for active layers/masks;
- use disk LRU for downloads, previews and derived data;
- support cancellation;
- expose progress stages such as `read`, `decode`, `mask`, `build`, `upload`, `draw`, `cache`, `done`.

## Golden tests

For real files, maintain golden values computed before binarization:

- nonzero pixel count;
- bounding box in pixels;
- deterministic sample checksum;
- layer count and dimensions;
- orientation checks for flip/mirror regressions.

## Decision

The cloud manager must not depend on the viewer being complete. Default, local-full and production presets keep `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER=OFF`; production QML contains no viewer settings or actions. The viewer path advances only through the isolated target, strict format contracts and diagnostic tests, and remains experimental until decode, render, navigation and export workflows are closed.
