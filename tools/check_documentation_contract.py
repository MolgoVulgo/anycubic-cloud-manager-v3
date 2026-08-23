#!/usr/bin/env python3
"""Validate the repository documentation and public-data contract."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from urllib.parse import unquote


DOC_PAIRS = (
    ("readme.md", "readme-FR.md"),
    ("docs/README.md", "docs/FR/README.md"),
    ("docs/01-overview-and-start.md", "docs/FR/01-presentation-demarrage.md"),
    ("docs/02-architecture-runtime.md", "docs/FR/02-architecture-runtime.md"),
    ("docs/03-anycubic-cloud.md", "docs/FR/03-cloud-anycubic.md"),
    ("docs/04-mqtt-realtime.md", "docs/FR/04-mqtt-temps-reel.md"),
    ("docs/05-qml-ui.md", "docs/FR/05-interface-qml.md"),
    ("docs/06-security-data.md", "docs/FR/06-securite-donnees.md"),
    ("docs/07-development-tests-patches.md", "docs/FR/07-developpement-tests-correctifs.md"),
    ("docs/appendices/ui-performance.md", "docs/FR/annexes/performance-ui.md"),
    ("docs/appendices/photon-viewer-formats.md", "docs/FR/annexes/viewer-photon-formats.md"),
    ("docs/appendices/anycubic-file-extensions.md", "docs/FR/annexes/extensions-fichiers-anycubic.md"),
    ("docs/appendices/ui-screens-cloud-client.md", "docs/FR/annexes/ecrans-client-cloud.md"),
    ("docs/appendices/mqtt-json-structures.md", "docs/FR/annexes/structures-json-mqtt.md"),
    ("docs/appendices/cloud-endpoints-runtime.md", "docs/FR/annexes/endpoints-cloud-runtime.md"),
    ("docs/appendices/mqtt-topics.md", "docs/FR/annexes/topics-mqtt.md"),
    ("docs/appendices/environment-variables.md", "docs/FR/annexes/variables-environnement.md"),
    ("docs/appendices/mqtt-print-capture-analysis.md", "docs/FR/annexes/analyse-captures-print-mqtt.md"),
    ("docs/appendices/archive-policy.md", "docs/FR/annexes/politique-archive.md"),
    ("docs/appendices/technical-decisions.md", "docs/FR/annexes/decisions-techniques.md"),
)

PUBLIC_REFERENCE_FILES = {
    "docs/reference-data/README.md",
    "docs/reference-data/mqtt/mqtt_synthetic_sample.jsonl",
    "docs/reference-data/mqtt/mqtt_synthetic_summary.json",
    "docs/reference-data/mqtt/mqtt_synthetic_workflow.md",
    "docs/FR/reference-data/README.md",
}

MARKDOWN_LINK = re.compile(r"(?<!!)\[[^\]]*\]\(([^)]+)\)")
HEX_PRINTER_ID = re.compile(r"/public/\d+/[0-9a-f]{16,}/", re.IGNORECASE)
UUID = re.compile(
    r"\b[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}\b",
    re.IGNORECASE,
)
LONG_NUMERIC_ID = re.compile(r"(?<!\d)\d{8,}(?!\d)")


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def check_required_pairs(root: Path, errors: list[str]) -> None:
    for english, french in DOC_PAIRS:
        for relative in (english, french):
            if not (root / relative).is_file():
                errors.append(f"missing documentation pair member: {relative}")


def check_repository_control_files(root: Path, errors: list[str]) -> None:
    required = root / "codex-patch-mode.md"
    if not required.is_file():
        errors.append("missing local patch application contract: codex-patch-mode.md")

    forbidden = (
        root / "codex-patch-mode-acm.md",
        root / "regles-generales-production-correctifs.md",
        root / "regles-generales-production.md",
    )
    for path in forbidden:
        if path.exists():
            errors.append(f"forbidden local control document: {path.relative_to(root)}")


def check_temporary_documentation(root: Path, errors: list[str]) -> None:
    for relative in ("docs/tmp", "docs/FR/tmp"):
        directory = root / relative
        if not directory.exists():
            continue
        files = sorted(path.relative_to(root) for path in directory.rglob("*") if path.is_file())
        for path in files:
            errors.append(f"unqualified temporary documentation file: {path}")


def iter_markdown_files(root: Path) -> list[Path]:
    result = list((root / "docs").rglob("*.md"))
    for relative in (
        "AGENTS.md",
        "readme.md",
        "readme-FR.md",
        "accloud/README.md",
        "codex-patch-mode.md",
    ):
        path = root / relative
        if path.is_file():
            result.append(path)
    return sorted(set(result))


def check_markdown_links(root: Path, errors: list[str]) -> None:
    for document in iter_markdown_files(root):
        text = read_text(document)
        for raw_target in MARKDOWN_LINK.findall(text):
            target = raw_target.strip().split()[0].strip("<>")
            if not target or target.startswith(("#", "http://", "https://", "mailto:", "qrc:/", "sandbox:")):
                continue
            target = unquote(target.split("#", 1)[0].split("?", 1)[0])
            if not target:
                continue
            resolved = (document.parent / target).resolve()
            try:
                resolved.relative_to(root.resolve())
            except ValueError:
                errors.append(f"link escapes repository: {document.relative_to(root)} -> {raw_target}")
                continue
            if not resolved.exists():
                errors.append(f"broken link: {document.relative_to(root)} -> {raw_target}")


def require_tokens(path: Path, tokens: tuple[str, ...], errors: list[str]) -> None:
    text = read_text(path)
    for token in tokens:
        if token not in text:
            errors.append(f"missing contract token {token!r} in {path}")


def check_mqtt_and_ssl_contract(root: Path, errors: list[str]) -> None:
    shared_tokens = (
        "mqtt-universe.anycubic.com",
        "8883",
        "`3.1.1`",
        "`1.2`",
        "mTLS",
        "slicer",
        "1200",
        "ACCLOUD_MQTT_TLS_ALLOW_INSECURE",
        "VerifyNone",
        "SECLEVEL=0",
        "ACCLOUD_MQTT_TLS_DEV_FALLBACK",
    )
    require_tokens(root / "docs/04-mqtt-realtime.md", shared_tokens + ("Clean session",), errors)
    require_tokens(root / "docs/FR/04-mqtt-temps-reel.md", shared_tokens + ("Clean session",), errors)

    source_root = root / "src"
    occurrences: list[Path] = []
    for source in source_root.rglob("*"):
        if source.suffix not in {".cpp", ".h", ".hpp", ".cc"}:
            continue
        if "ignoreSslErrors(" in read_text(source):
            occurrences.append(source)
    expected = root / "src/accloud/app/cloud/ThumbnailService.cpp"
    if occurrences != [expected]:
        relative = [str(path.relative_to(root)) for path in occurrences]
        errors.append(f"ignoreSslErrors() must occur only in ThumbnailService.cpp, found: {relative}")
    if expected.is_file():
        text = read_text(expected)
        for token in ("QImageReader", "QSaveFile", "safeUrlForLogs"):
            if token not in text:
                errors.append(f"thumbnail TLS guard missing {token} in {expected.relative_to(root)}")



def check_workflow_architecture_contract(root: Path, errors: list[str]) -> None:
    tokens = ("CloudFilesWorkflowBridge", "DeleteCloudFilesUseCase")
    for relative in (
        "docs/02-architecture-runtime.md",
        "docs/05-qml-ui.md",
        "docs/FR/02-architecture-runtime.md",
        "docs/FR/05-interface-qml.md",
    ):
        require_tokens(root / relative, tokens, errors)


def check_cloud_bridge_architecture_contract(root: Path, errors: list[str]) -> None:
    tokens = (
        "CloudBridgeSupport",
        "ThumbnailService",
        "CloudDownloadController",
        "CloudUploadController",
    )
    for relative in (
        "docs/02-architecture-runtime.md",
        "docs/FR/02-architecture-runtime.md",
    ):
        require_tokens(root / relative, tokens, errors)

    require_tokens(root / "docs/06-security-data.md", ("ThumbnailService",), errors)
    require_tokens(root / "docs/FR/06-securite-donnees.md", ("ThumbnailService",), errors)



def check_local_cache_architecture_contract(root: Path, errors: list[str]) -> None:
    architecture_tokens = (
        "LocalCacheStore",
        "LocalCacheSql",
        "LocalCacheFiles",
        "LocalCachePrinters",
        "LocalCacheJobs",
        "LocalCacheState",
        "ACCLOUD_LOCAL_CACHE_SOURCES",
    )
    for relative in (
        "docs/02-architecture-runtime.md",
        "docs/FR/02-architecture-runtime.md",
    ):
        require_tokens(root / relative, architecture_tokens, errors)

    test_tokens = (
        "check_local_cache_architecture.py",
        "accloud_local_cache_architecture",
        "ACCLOUD_LOCAL_CACHE_SOURCES",
    )
    for relative in (
        "docs/07-development-tests-patches.md",
        "docs/FR/07-developpement-tests-correctifs.md",
    ):
        require_tokens(root / relative, test_tokens, errors)

def check_mqtt_bridge_architecture_contract(root: Path, errors: list[str]) -> None:
    architecture_tokens = (
        "MqttBridge",
        "MqttBridgeSession",
        "MqttBridgeMessages",
        "MqttBridgeTelemetry",
        "ACCLOUD_MQTT_BRIDGE_SOURCES",
    )
    for relative in (
        "docs/02-architecture-runtime.md",
        "docs/04-mqtt-realtime.md",
        "docs/FR/02-architecture-runtime.md",
        "docs/FR/04-mqtt-temps-reel.md",
    ):
        require_tokens(root / relative, architecture_tokens, errors)

    test_tokens = (
        "check_mqtt_bridge_architecture.py",
        "accloud_mqtt_bridge_architecture",
        "ACCLOUD_MQTT_BRIDGE_SOURCES",
    )
    for relative in (
        "docs/07-development-tests-patches.md",
        "docs/FR/07-developpement-tests-correctifs.md",
    ):
        require_tokens(root / relative, test_tokens, errors)


def check_experimental_viewer_contract(root: Path, errors: list[str]) -> None:
    tokens = (
        "ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER",
        "experimental-viewer-core",
        "accloud_experimental_viewer",
    )
    for relative in (
        "docs/02-architecture-runtime.md",
        "docs/07-development-tests-patches.md",
        "docs/appendices/photon-viewer-formats.md",
        "docs/FR/02-architecture-runtime.md",
        "docs/FR/07-developpement-tests-correctifs.md",
        "docs/FR/annexes/viewer-photon-formats.md",
    ):
        require_tokens(root / relative, tokens, errors)



def check_documented_build_and_test_inventory(root: Path, errors: list[str]) -> None:
    presets_path = root / "accloud/CMakePresets.json"
    try:
        presets = json.loads(read_text(presets_path))
    except (OSError, json.JSONDecodeError) as exc:
        errors.append(f"cannot read configure presets for documentation inventory: {exc}")
        return

    preset_names = {
        preset.get("name")
        for preset in presets.get("configurePresets", [])
        if isinstance(preset, dict) and isinstance(preset.get("name"), str)
    }
    for relative in (
        "accloud/README.md",
        "docs/02-architecture-runtime.md",
        "docs/FR/02-architecture-runtime.md",
    ):
        text = read_text(root / relative)
        missing = sorted(name for name in preset_names if f"`{name}`" not in text)
        if missing:
            errors.append(f"{relative} does not document configure presets: {missing}")

    cmake = read_text(root / "accloud/CMakeLists.txt")
    viewer_group = re.search(
        r'set_tests_properties\(\s*accloud_experimental_viewer_scaffold(?P<tests>.*?)\s*PROPERTIES LABELS "core;experimental;viewer"\)',
        cmake,
        flags=re.DOTALL,
    )
    if viewer_group is None:
        errors.append("cannot derive the experimental viewer core test inventory from CMake")
        return
    viewer_tests = {"accloud_experimental_viewer_scaffold"}
    viewer_tests.update(
        re.findall(r"\baccloud_[a-z0-9_]+\b", viewer_group.group("tests"))
    )
    for relative in (
        "docs/07-development-tests-patches.md",
        "docs/appendices/photon-viewer-formats.md",
        "docs/FR/07-developpement-tests-correctifs.md",
        "docs/FR/annexes/viewer-photon-formats.md",
    ):
        text = read_text(root / relative)
        missing = sorted(name for name in viewer_tests if name not in text)
        if missing:
            errors.append(f"{relative} omits CMake viewer tests: {missing}")


def check_documentation_freshness_contract(root: Path, errors: list[str]) -> None:
    english_dev = read_text(root / "docs/07-development-tests-patches.md")
    french_dev = read_text(root / "docs/FR/07-developpement-tests-correctifs.md")
    for relative, text in (
        ("docs/07-development-tests-patches.md", english_dev),
        ("docs/FR/07-developpement-tests-correctifs.md", french_dev),
    ):
        if "regles-generales-production.md" not in text:
            errors.append(f"{relative} does not name the external normative governance file")
        if "regles-generales-production-correctifs.md" in text:
            errors.append(f"{relative} still names the obsolete governance file")

    for relative in (
        "docs/appendices/archive-policy.md",
        "docs/FR/annexes/politique-archive.md",
    ):
        text = read_text(root / relative)
        if "make-a.sh" in text:
            errors.append(f"{relative} still documents the removed archive script")
        for token in ("tests", "acm.zip", "regles-generales-production.md", "accloud-build-deps.zip"):
            if token not in text:
                errors.append(f"{relative} is missing archive-policy token {token!r}")

    mqtt_owner = "src/accloud/app/mqtt/MqttBridgeSession.cpp"
    for relative in (
        "docs/appendices/mqtt-topics.md",
        "docs/FR/annexes/topics-mqtt.md",
    ):
        if mqtt_owner not in read_text(root / relative):
            errors.append(f"{relative} does not document the runtime subscription owner")
    if not (root / mqtt_owner).is_file():
        errors.append(f"documented MQTT subscription owner does not exist: {mqtt_owner}")

    stale_viewer_phrases = (
        "experimental 3D viewer foundation",
        "base expérimentale de viewer 3D",
        "complete viewer workflow is not closed",
        "workflow complet n'est pas finalisé",
    )
    for relative in (
        "readme.md",
        "readme-FR.md",
        "docs/01-overview-and-start.md",
        "docs/FR/01-presentation-demarrage.md",
    ):
        text = read_text(root / relative)
        for phrase in stale_viewer_phrases:
            if phrase in text:
                errors.append(f"{relative} retains stale viewer status: {phrase!r}")

    ui_tests = (
        "accloud_local_cache_architecture",
        "accloud_cloud_core_regressions",
        "accloud_cloud_bridge_architecture",
        "accloud_security_redaction",
        "accloud_mqtt_flow",
        "accloud_ui_qml",
        "accloud_ui_models",
    )
    for relative in (
        "docs/appendices/ui-performance.md",
        "docs/FR/annexes/performance-ui.md",
    ):
        text = read_text(root / relative)
        missing = [name for name in ui_tests if name not in text]
        if missing:
            errors.append(f"{relative} has an incomplete UI validation command: {missing}")
        if "accloud_cache|" in text:
            errors.append(f"{relative} still contains the obsolete accloud_cache regex")

def check_i18n_contract(root: Path, errors: list[str]) -> None:
    expected = {root / "i18n/accloud_en.ts", root / "i18n/accloud_fr.ts"}
    actual = set(root.rglob("*.ts"))
    if actual != expected:
        rendered = sorted(str(path.relative_to(root)) for path in actual)
        errors.append(f"active TS catalogs must be unique at repository root, found: {rendered}")
    cmake = read_text(root / "accloud/CMakeLists.txt")
    if "set(ACCLOUD_I18N_ROOT ${ACCLOUD_REPO_ROOT}/i18n)" not in cmake:
        errors.append("CMake does not point to the repository-root i18n directory")


def check_public_reference_data(root: Path, errors: list[str]) -> None:
    actual: set[str] = set()
    for directory in (root / "docs/reference-data", root / "docs/FR/reference-data"):
        if not directory.exists():
            errors.append(f"missing public reference directory: {directory.relative_to(root)}")
            continue
        actual.update(str(path.relative_to(root)) for path in directory.rglob("*") if path.is_file())
    if actual != PUBLIC_REFERENCE_FILES:
        errors.append(
            "unexpected public reference-data files: "
            + repr(sorted(actual.symmetric_difference(PUBLIC_REFERENCE_FILES)))
        )

    for relative in sorted(actual):
        path = root / relative
        if path.stat().st_size > 100_000:
            errors.append(f"public reference file exceeds 100 KB: {relative}")
        text = read_text(path)
        if HEX_PRINTER_ID.search(text):
            errors.append(f"persistent printer-like identifier in {relative}")
        if UUID.search(text):
            errors.append(f"UUID-like capture identifier in {relative}")
        if LONG_NUMERIC_ID.search(text):
            errors.append(f"long numeric task/timestamp-like identifier in {relative}")
        lowered = text.lower()
        for forbidden in ("mqtt_topic_capture", "session.json\"", "authorization\":", "cookie\":"):
            if forbidden in lowered:
                errors.append(f"capture or secret marker {forbidden!r} in {relative}")


def check_qml_runtime(root: Path, errors: list[str]) -> None:
    main_cpp = read_text(root / "src/accloud/app/main.cpp")
    qrc = read_text(root / "src/accloud/app/resources.qrc")
    if "qrc:/qml/MainWindow.qml" not in main_cpp:
        errors.append("main.cpp no longer loads qrc:/qml/MainWindow.qml")
    if "qml/MainWindow.qml" not in qrc and "../ui/qml/MainWindow.qml" not in qrc:
        errors.append("resources.qrc does not include MainWindow.qml")


def check_cmake_registration(root: Path, errors: list[str]) -> None:
    cmake = read_text(root / "accloud/CMakeLists.txt")
    if "NAME accloud_documentation_contract" not in cmake:
        errors.append("accloud_documentation_contract is not registered in CTest")
    if "check_documentation_contract.py" not in cmake:
        errors.append("CMake does not invoke check_documentation_contract.py")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root; defaults to the parent of tools/",
    )
    args = parser.parse_args()
    root = args.repo_root.resolve()
    errors: list[str] = []

    check_required_pairs(root, errors)
    check_repository_control_files(root, errors)
    check_temporary_documentation(root, errors)
    check_markdown_links(root, errors)
    check_mqtt_and_ssl_contract(root, errors)
    check_workflow_architecture_contract(root, errors)
    check_cloud_bridge_architecture_contract(root, errors)
    check_local_cache_architecture_contract(root, errors)
    check_mqtt_bridge_architecture_contract(root, errors)
    check_experimental_viewer_contract(root, errors)
    check_documented_build_and_test_inventory(root, errors)
    check_documentation_freshness_contract(root, errors)
    check_i18n_contract(root, errors)
    check_public_reference_data(root, errors)
    check_qml_runtime(root, errors)
    check_cmake_registration(root, errors)

    if errors:
        print("Documentation contract FAILED", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print(
        "Documentation contract passed: "
        f"{len(DOC_PAIRS)} bilingual pairs, "
        f"{len(iter_markdown_files(root))} Markdown files, "
        f"{len(PUBLIC_REFERENCE_FILES)} public reference files."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
