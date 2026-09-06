# accloud — build and runtime reference

`accloud/` is the CMake entry point of Anycubic Cloud Manager V3. The executable is `accloud_cli`; `main.cpp` selects CLI mode (`--smoke`, `--import-har`) or the Qt/QML desktop runtime.

## Presets

| Preset | Purpose |
| --- | --- |
| `default` | Debug desktop build, Qt enabled, viewer enabled, debug tooling disabled. |
| `dev-debug` | Debug desktop build with debug resources and bridges. |
| `prod` | Release desktop build; debug tooling excluded, standard PWSZ 3D viewer enabled. |
| `protected-core` | Offline portable core gate without Qt/QML/live services. |
| `local-full` | Strict complete local Qt/QML/SQL/MQTT non-live gate. |
| `experimental-viewer-core` | Isolated non-Qt PWSZ, meshing and viewer-core gate. |
| `experimental-viewer-qt` | Strict Qt/OpenGL desktop viewer gate. |

Normal desktop build:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

The authoritative preset definitions are in `CMakePresets.json`; the documentation guard checks that every configure preset remains listed here and in the architecture guide.

## Active paths

- CMake: `accloud/CMakeLists.txt`;
- bootstrap: `src/accloud/app/main.cpp`;
- main QML: `qrc:/qml/MainWindow.qml`;
- translations: `i18n/accloud_en.ts`, `i18n/accloud_fr.ts`;
- runtime data: `~/.local/share/accloud` by default.

## Documentation

- [Overview](../docs/01-overview-and-start.md)
- [Architecture and runtime](../docs/02-architecture-runtime.md)
- [Anycubic cloud](../docs/03-anycubic-cloud.md)
- [MQTT and realtime](../docs/04-mqtt-realtime.md)
- [Development and tests](../docs/07-development-tests-patches.md)

Protocol and security invariants are documented before environment overrides. Do not infer a generic MQTT or REST configuration from external conventions.
