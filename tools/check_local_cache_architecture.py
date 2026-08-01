#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import sys


def fail(errors: list[str]) -> int:
    print("Local cache architecture check failed:")
    for error in errors:
        print(f"- {error}")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, required=True)
    args = parser.parse_args()
    root = args.repo_root.resolve()
    app = root / "src/accloud/app"
    cache = app / "cache"
    cmake_path = root / "accloud/CMakeLists.txt"
    errors: list[str] = []

    expected = {
        "LocalCacheStore.cpp": (
            "LocalCacheStore::ensureReady",
            "LocalCacheStore::databasePath",
        ),
        "cache/LocalCacheSql.cpp": (
            "runSchema",
            "migrateLegacyFiles",
            "kSchemaVersion",
        ),
        "cache/LocalCacheFiles.cpp": (
            "LocalCacheStore::loadFiles",
            "LocalCacheStore::replaceFiles",
            "LocalCacheStore::removeFile",
        ),
        "cache/LocalCachePrinters.cpp": (
            "LocalCacheStore::loadPrinters",
            "LocalCacheStore::replacePrinters",
            "LocalCacheStore::savePrinterDetails",
        ),
        "cache/LocalCacheJobs.cpp": (
            "LocalCacheStore::loadJobsForPrinter",
            "LocalCacheStore::replaceJobsForPrinter",
            "LocalCacheStore::upsertJobsForPrinter",
        ),
        "cache/LocalCacheState.cpp": (
            "LocalCacheStore::loadQuota",
            "LocalCacheStore::savePendingDirectPrint",
            "LocalCacheStore::updateSyncState",
        ),
    }

    texts: dict[str, str] = {}
    for relative, markers in expected.items():
        path = app / relative
        if not path.is_file():
            errors.append(f"missing local-cache component: src/accloud/app/{relative}")
            continue
        text = path.read_text(encoding="utf-8")
        texts[relative] = text
        lines = len(text.splitlines())
        budget = 700 if relative != "LocalCacheStore.cpp" else 160
        if lines > budget:
            errors.append(f"{relative} exceeds its {budget}-line budget ({lines})")
        for marker in markers:
            if marker not in text:
                errors.append(f"{relative} missing responsibility marker {marker!r}")

    facade = texts.get("LocalCacheStore.cpp", "")
    forbidden_facade_markers = (
        "LocalCacheStore::replaceFiles",
        "LocalCacheStore::replacePrinters",
        "LocalCacheStore::replaceJobs",
        "LocalCacheStore::saveQuota",
        "LocalCacheStore::savePendingDirectPrint",
        "LocalCacheStore::updateSyncState",
        "CREATE TABLE",
        "INSERT INTO",
        "DELETE FROM",
    )
    for marker in forbidden_facade_markers:
        if marker in facade:
            errors.append(f"LocalCacheStore.cpp reabsorbed SQL/domain responsibility {marker!r}")

    internal_header = cache / "LocalCacheSql.h"
    if not internal_header.is_file():
        errors.append("missing internal SQL boundary: src/accloud/app/cache/LocalCacheSql.h")
    else:
        header = internal_header.read_text(encoding="utf-8")
        for marker in ("runSchema", "closeAndRemoveDatabase", "enforceMaxRows"):
            if marker not in header:
                errors.append(f"LocalCacheSql.h missing shared SQL marker {marker!r}")

    if not cmake_path.is_file():
        errors.append("accloud/CMakeLists.txt is missing")
    else:
        cmake = cmake_path.read_text(encoding="utf-8")
        if "set(ACCLOUD_LOCAL_CACHE_SOURCES" not in cmake:
            errors.append("CMake does not define ACCLOUD_LOCAL_CACHE_SOURCES")
        if cmake.count("${ACCLOUD_LOCAL_CACHE_SOURCES}") < 2:
            errors.append("ACCLOUD_LOCAL_CACHE_SOURCES must be reused by runtime and SQL regression tests")
        for relative in (
            "app/cache/LocalCacheSql.cpp",
            "app/cache/LocalCacheFiles.cpp",
            "app/cache/LocalCachePrinters.cpp",
            "app/cache/LocalCacheJobs.cpp",
            "app/cache/LocalCacheState.cpp",
        ):
            if relative not in cmake:
                errors.append(f"CMake does not compile {relative}")

    if errors:
        return fail(errors)

    print("Local cache architecture check passed")
    print(f"- LocalCacheStore.cpp: {len(facade.splitlines())} lines")
    print("- schema, files, printers, jobs and state persistence are split by responsibility")
    print("- runtime and SQL regression tests share the same source set")
    return 0


if __name__ == "__main__":
    sys.exit(main())
