#!/usr/bin/env python3
"""Validate the repository documentation and public-data contract."""

from __future__ import annotations

import argparse
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


def iter_markdown_files(root: Path) -> list[Path]:
    result = list((root / "docs").rglob("*.md"))
    for relative in (
        "AGENTS.md",
        "readme.md",
        "readme-FR.md",
        "accloud/README.md",
        "codex-patch-mode.md",
        "regles-generales-production-correctifs.md",
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
    expected = root / "src/accloud/app/CloudBridge.cpp"
    if occurrences != [expected]:
        relative = [str(path.relative_to(root)) for path in occurrences]
        errors.append(f"ignoreSslErrors() must occur only in CloudBridge.cpp, found: {relative}")
    if expected.is_file():
        text = read_text(expected)
        for token in ("QImageReader", "QSaveFile", "safeUrlForLogs"):
            if token not in text:
                errors.append(f"thumbnail TLS guard missing {token} in {expected.relative_to(root)}")


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
    check_markdown_links(root, errors)
    check_mqtt_and_ssl_contract(root, errors)
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
