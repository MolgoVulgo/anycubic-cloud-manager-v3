#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import sys

API_FILES = (
    "AuthApi.cpp",
    "FilesApi.cpp",
    "QuotaApi.cpp",
    "DownloadsApi.cpp",
    "PrintersApi.cpp",
    "ProjectsApi.cpp",
    "ReasonCatalogApi.cpp",
    "PrintOrderApi.cpp",
)


def fail(errors: list[str]) -> int:
    print("Cloud API architecture check failed:")
    for error in errors:
        print(f"- {error}")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, required=True)
    args = parser.parse_args()
    root = args.repo_root.resolve()
    api_dir = root / "src/accloud/infra/cloud/api"
    cmake = (root / "accloud/CMakeLists.txt").read_text(encoding="utf-8")
    errors: list[str] = []

    for legacy_name in ("CloudLegacyImpl.cpp", "CloudLegacyImpl.h"):
        if (api_dir / legacy_name).exists():
            errors.append(f"legacy backend still exists: src/accloud/infra/cloud/api/{legacy_name}")
        if legacy_name in cmake:
            errors.append(f"CMake still references {legacy_name}")

    if "infra/cloud/api/ApiSupport.cpp" not in cmake:
        errors.append("CMake does not compile ApiSupport.cpp")

    for filename in API_FILES:
        path = api_dir / filename
        if not path.is_file():
            errors.append(f"missing API owner: {path.relative_to(root)}")
            continue
        text = path.read_text(encoding="utf-8")
        if '#include "ApiSupport.h"' not in text:
            errors.append(f"{filename} does not use the shared transport/JSON support")
        if "CloudLegacyImpl" in text or "legacy::" in text:
            errors.append(f"{filename} still delegates to the legacy backend")

    support = api_dir / "ApiSupport.cpp"
    support_header = api_dir / "ApiSupport.h"
    if not support.is_file() or not support_header.is_file():
        errors.append("ApiSupport.cpp/.h must exist")
    else:
        support_text = support.read_text(encoding="utf-8") + support_header.read_text(encoding="utf-8")
        for domain_type in (
            "CloudFileInfo",
            "CloudPrinterInfo",
            "CloudPrinterProjectItem",
            "CloudReasonCatalogItem",
            "CloudPrintOrderResult",
        ):
            if domain_type in support_text:
                errors.append(f"ApiSupport must not own domain parsing ({domain_type})")
        for required in ("WorkbenchRequestBuilder", "HttpClient"):
            if required not in support_text:
                errors.append(f"ApiSupport is missing shared transport dependency {required}")

    client = (root / "src/accloud/infra/cloud/CloudClient.cpp").read_text(encoding="utf-8")
    if "CloudLegacyImpl" in client or "legacy::" in client:
        errors.append("CloudClient still references the legacy backend")

    if errors:
        return fail(errors)

    print("Cloud API architecture check passed")
    print("- CloudLegacyImpl removed")
    print("- API facades own endpoint behavior")
    print("- ApiSupport limited to transport and generic JSON helpers")
    return 0


if __name__ == "__main__":
    sys.exit(main())
