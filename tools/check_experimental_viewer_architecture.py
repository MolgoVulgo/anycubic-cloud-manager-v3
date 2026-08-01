#!/usr/bin/env python3
"""Keep the unfinished Photon/viewer scaffold outside the production runtime."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


EXPERIMENTAL_ROOTS = (
    "infra/photons/",
    "infra/jobs/",
    "infra/cache/",
    "render3d/",
)

PRODUCTION_QML_FORBIDDEN = (
    "render3dDefaultsDialog",
    "menuSettingsRender3d",
    "render3dDefault",
    "Open render3d log",
    "ViewerDraftDialog",
    "viewerDraftDialog",
    "viewerDialogButton",
    "openViewerDialog",
)

PRODUCTION_VIEWER_QML = (
    "src/accloud/ui/qml/dialogs/ViewerDraftDialog.qml",
    "src/accloud/ui/qml/pages/ViewerPage.qml",
    "src/accloud/ui/qml/panes/Viewer3DPane.qml",
)


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def cmake_set_body(cmake: str, variable: str) -> str | None:
    match = re.search(
        rf"set\({re.escape(variable)}\s*\n(?P<body>.*?)\n\)",
        cmake,
        flags=re.DOTALL,
    )
    return match.group("body") if match else None


def normalized_cmake_sources(body: str) -> set[str]:
    sources: set[str] = set()
    for raw in body.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        sources.add(line.strip('"'))
    return sources


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
    )
    root = parser.parse_args().repo_root.resolve()
    errors: list[str] = []

    cmake_path = root / "accloud/CMakeLists.txt"
    presets_path = root / "accloud/CMakePresets.json"
    cmake = read(cmake_path)

    option_pattern = re.compile(
        r"option\(ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER\s+"
        r'"[^"]+"\s+OFF\)',
        flags=re.DOTALL,
    )
    if not option_pattern.search(cmake):
        errors.append("ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER must exist and default to OFF")

    main_body = cmake_set_body(cmake, "ACCLOUD_INFRA_SOURCES")
    experimental_body = cmake_set_body(cmake, "ACCLOUD_EXPERIMENTAL_VIEWER_SOURCES")
    if main_body is None:
        errors.append("missing ACCLOUD_INFRA_SOURCES")
        main_sources: set[str] = set()
    else:
        main_sources = normalized_cmake_sources(main_body)
    if experimental_body is None:
        errors.append("missing ACCLOUD_EXPERIMENTAL_VIEWER_SOURCES")
        experimental_sources: set[str] = set()
    else:
        experimental_sources = normalized_cmake_sources(experimental_body)

    leaked = sorted(
        source
        for source in main_sources
        if source.startswith(EXPERIMENTAL_ROOTS)
    )
    if leaked:
        errors.append(f"experimental sources leaked into accloud_infra: {leaked}")

    source_root = root / "src/accloud"
    compiled_scaffolds = []
    for source in sorted(main_sources):
        path = source_root / source
        if path.is_file() and "Scaffold placeholder" in read(path):
            compiled_scaffolds.append(source)
    if compiled_scaffolds:
        errors.append(
            f"scaffold placeholders compiled in accloud_infra: {compiled_scaffolds}"
        )

    expected_sources = {
        str(path.relative_to(source_root))
        for relative_root in EXPERIMENTAL_ROOTS
        for path in (source_root / relative_root).rglob("*.cpp")
    }
    if experimental_sources != expected_sources:
        missing = sorted(expected_sources - experimental_sources)
        extra = sorted(experimental_sources - expected_sources)
        if missing:
            errors.append(f"experimental source list is missing: {missing}")
        if extra:
            errors.append(f"experimental source list has unexpected entries: {extra}")

    required_cmake_tokens = (
        "if(ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER)",
        "add_library(accloud_experimental_viewer OBJECT",
        "ACCLOUD_EXPERIMENTAL_VIEWER=1",
        "NAME accloud_experimental_viewer_architecture",
        "NAME accloud_experimental_viewer_scaffold",
    )
    for token in required_cmake_tokens:
        if token not in cmake:
            errors.append(f"missing CMake viewer-isolation token: {token}")

    forbidden_links = (
        "target_link_libraries(accloud_cli PRIVATE accloud_experimental_viewer",
        "target_link_libraries(accloud_infra PRIVATE accloud_experimental_viewer",
        "target_sources(accloud_cli PRIVATE ${ACCLOUD_EXPERIMENTAL_VIEWER_SOURCES}",
    )
    for token in forbidden_links:
        if token in cmake:
            errors.append(f"experimental viewer is linked to production runtime: {token}")

    try:
        presets = json.loads(read(presets_path))
    except (OSError, json.JSONDecodeError) as exc:
        errors.append(f"cannot read CMake presets: {exc}")
        presets = {}

    configure = {
        preset.get("name"): preset
        for preset in presets.get("configurePresets", [])
        if isinstance(preset, dict)
    }
    for name in ("default", "prod", "protected-core"):
        value = configure.get(name, {}).get("cacheVariables", {}).get(
            "ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER"
        )
        if value != "OFF":
            errors.append(f"preset {name} must explicitly disable the experimental viewer")
    value = configure.get("experimental-viewer-core", {}).get("cacheVariables", {}).get(
        "ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER"
    )
    if value != "ON":
        errors.append("experimental-viewer-core must explicitly enable the viewer scaffold")

    qml_paths = (
        root / "src/accloud/ui/qml/MainWindow.qml",
        root / "src/accloud/ui/qml/pages/DebugPage.qml",
        root / "src/accloud/app/resources.qrc",
    )
    for path in qml_paths:
        text = read(path)
        for token in PRODUCTION_QML_FORBIDDEN:
            if token in text:
                errors.append(f"production QML still exposes experimental viewer token {token!r} in {path.relative_to(root)}")

    for relative in PRODUCTION_VIEWER_QML:
        if (root / relative).exists():
            errors.append(f"obsolete production viewer QML remains: {relative}")

    qml_test = read(root / "tests/ui/qml/tst_control_room.qml")
    for object_name in (
        "render3dDefaultsDialog",
        "menuSettingsRender3d",
        "viewerDraftDialog",
        "viewerDialogButton",
    ):
        expected = f'findObjectByName(window, "{object_name}") === null'
        if expected not in qml_test:
            errors.append(f"QML regression does not assert removal of {object_name}")

    smoke = root / "tests/photons/test_experimental_viewer_scaffold.cpp"
    if not smoke.is_file():
        errors.append("missing opt-in experimental viewer scaffold smoke test")

    if errors:
        print("Experimental viewer architecture FAILED", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print(
        "Experimental viewer architecture passed: production runtime excludes "
        f"{len(experimental_sources)} scaffold sources; opt-in preset and smoke test are registered."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
