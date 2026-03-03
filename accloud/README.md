# accloud

C++20 Qt/Kirigami skeleton for Anycubic Cloud Manager V3.

This directory materializes the architecture from `Docs/structure_application_photons.md`:
- layered modules (`ui`, `domain`, `infra`, `render3d`, `tests`)
- Photon multi-format drivers (`PWMB`, `PWS`, `PHZ`, `PHOTONS`, `PWSZ`)
- async jobs, cache, logging, and cloud workflows

## Build

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

When Qt6 is unavailable, the build falls back to a headless skeleton mode.
