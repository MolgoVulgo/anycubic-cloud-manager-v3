# Anycubic Cloud Manager V3

Anycubic Cloud Manager V3 is a C++20 / Qt6 / QML desktop application for working with the Anycubic cloud ecosystem from Linux. The application focuses on cloud file management, printer dashboard state, MQTT runtime observation, remote print workflow support, and the technical foundation for a Photon/PWMB viewer.

This repository is not an official Anycubic project. It is a reverse-engineered client built from observed cloud behavior, MQTT traces, runtime tests, and documented decisions. Anycubic can change its endpoints, MQTT topics, signatures, payloads, printer behavior, or cloud-side validation rules at any time.

## Current product scope

| Area | Status | Product meaning |
| --- | --- | --- |
| Qt/QML desktop shell | Active | Main window, files, printers, session import, settings, runtime state and debug-only log views. |
| Anycubic Cloud client | Active / partial | HAR session import, cloud listing, quota, printer dashboard, signed downloads, upload workflow, print order workflow and cache fallback. |
| MQTT runtime | Active | mTLS connection, topic routing, realtime printer store, print overlay state, discovery capture for unknown messages. |
| Remote print workflow | Active / partial | HTTP command flow followed by MQTT observation. Some actions remain printer-model and API dependent. |
| Photon/PWMB viewer | Experimental / partial | Format drivers, PWMB/PWS decoding pieces, render3d skeleton and viewer UI structure exist, but the viewer is not yet a closed product workflow. |
| Documentation | Active | The documentation is part of the project contract. It records behavior, analysis, decisions and known limits. |

## Main capabilities

- Import or persist an Anycubic web session from a HAR capture.
- List cloud files and printer resources exposed by the cloud account.
- Resolve signed download URLs and download cloud files locally.
- Upload local files through the observed lock / presign / register / unlock sequence.
- Send print-related orders when the cloud API and printer compatibility allow it.
- Subscribe to MQTT printer topics and normalize realtime printer/job state.
- Track print preparation, download, checks, preheating, printing, failure and completion states.
- Interpret resin workflow messages according to print phase, including the distinction between pre-print autoload and runtime refill attempts.
- Maintain local cache, logs, thumbnails and runtime settings under a controlled application data directory.
- Keep debug tooling out of production builds unless explicitly enabled.

## Repository layout

```text
accloud/                 CMake project entry point
src/accloud/app/         application bootstrap, Qt bridges, UI-facing models
src/accloud/domain/      stable domain contracts and project types
src/accloud/infra/       cloud, MQTT, cache, logging, Photon drivers, jobs
src/accloud/render3d/    OpenGL / Qt Quick rendering foundation
src/accloud/ui/qml/      QML shell, pages, dialogs, shared controls
tests/                   C++ and QML regression tests
tools/                   CI and maintenance scripts
i18n/                    Qt translation catalogs
resources/mqtt/tls/      local MQTT TLS material fallback
packaging/               package metadata and Arch packaging
docs/                    English documentation
FR/docs/                 French documentation
```

## Quick start

From the repository root:

```bash
# development build, then run with debug UI enabled
./start.sh 1

# run an existing development build
./start.sh 2

# production build, then run production binary
./start.sh 3
```

Manual build from `accloud/`:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

Useful presets:

```bash
cmake --preset default     # Debug, Qt enabled, debug tooling disabled
cmake --preset dev-debug   # Debug, Qt enabled, debug tooling enabled
cmake --preset prod        # Release, debug tooling excluded
```

## CLI utilities

```bash
cd accloud
./build/default/accloud_cli --smoke
./build/default/accloud_cli --import-har /path/to/capture.har
./build/default/accloud_cli --import-har /path/to/capture.har --session-path /path/to/session.json
```

HAR files contain reusable tokens and must be treated as sensitive material. They must not be committed, attached to public issues, or included in public documentation archives.

## Runtime data

Default runtime root:

```text
~/.local/share/accloud
```

Default generated files and directories:

```text
~/.local/share/accloud/accloud.ini
~/.local/share/accloud/session.json
~/.local/share/accloud/settings.ini
~/.local/share/accloud/accloud_cache.db
~/.local/share/accloud/tmp/
~/.local/share/accloud/thumbnails/
~/.local/share/accloud/logs/
```

Useful overrides:

```bash
export ACCLOUD_PATHS_INI=/path/to/accloud.ini
export ACCLOUD_SESSION_PATH=/path/to/session.json
export ACCLOUD_DB_PATH=/path/to/accloud_cache.db
export ACCLOUD_LOG_DIR=/path/to/logs
export ACCLOUD_THUMBNAIL_DIR=/path/to/thumbnails
```

## Documentation

Start with:

```text
docs/README.md
```

French documentation is available under:

```text
FR/docs/README.md
```

The documentation is structured by operational domain: architecture, cloud, MQTT, print workflow, UI/QML, UI performance, Photon/PWMB formats, i18n, operations/security, development rules, decisions and appendices.

## Safety rules

Never log or commit:

- cloud tokens;
- `Authorization` headers;
- cookies;
- full signed URLs;
- HAR captures;
- MQTT TLS private keys;
- raw credentials;
- session files containing reusable tokens.

## Development note and acknowledgements

This application is developed through a vibe-coding workflow with Codex, with repository code and captured behavior used as the final source of truth.

Special thanks to the following projects and maintainers. Their published work, experiments and protocol analysis helped make this project possible:

- [Royrdan/anycubic_cloud](https://github.com/Royrdan/anycubic_cloud), for the early and practical work around Anycubic Cloud behavior.
- [UVtools](https://github.com/sn4k3/UVtools), for the extensive work on Anycubic file formats and Photon-related file analysis.
- [WaresWichall/hass-anycubic_cloud](https://github.com/WaresWichall/hass-anycubic_cloud), for the work on understanding Anycubic MQTT behavior and Home Assistant integration.

## License

See the repository license files. Packaging metadata currently declares the project as MIT-licensed.
