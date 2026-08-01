#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import sys


def fail(errors: list[str]) -> int:
    print("CloudBridge architecture check failed:")
    for error in errors:
        print(f"- {error}")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, required=True)
    args = parser.parse_args()
    root = args.repo_root.resolve()
    app = root / "src/accloud/app"
    bridge_path = app / "CloudBridge.cpp"
    cmake = (root / "accloud/CMakeLists.txt").read_text(encoding="utf-8")
    errors: list[str] = []

    if not bridge_path.is_file():
        return fail(["src/accloud/app/CloudBridge.cpp is missing"])

    bridge = bridge_path.read_text(encoding="utf-8")
    if len(bridge.splitlines()) > 1600:
        errors.append("CloudBridge.cpp exceeds the 1600-line facade budget")

    required_includes = (
        'app/cloud/CloudBridgeSupport.h',
        'app/cloud/CloudDownloadController.h',
        'app/cloud/CloudUploadController.h',
        'app/cloud/ThumbnailService.h',
    )
    for token in required_includes:
        if token not in bridge:
            errors.append(f"CloudBridge.cpp does not delegate through {token}")

    forbidden_bridge_tokens = (
        "ignoreSslErrors(",
        "QNetworkAccessManager",
        "QNetworkReply",
        "QSaveFile",
        "QImageReader",
        "UploadLocalFileUseCase",
        "UpdateCloudPwszPreviewsUseCase",
        "inspectPwszPreviewArchive",
        "rawRequestHeaders(",
    )
    for token in forbidden_bridge_tokens:
        if token in bridge:
            errors.append(f"CloudBridge.cpp still owns extracted responsibility token {token!r}")

    components = {
        "CloudBridgeSupport.cpp": ("finalizeUiMessage", "printerProjectToMap"),
        "CloudDownloadController.cpp": ("QNetworkAccessManager", "setTransferTimeout(0)"),
        "CloudUploadController.cpp": ("UploadLocalFileUseCase", "UpdateCloudPwszPreviewsUseCase"),
        "ThumbnailService.cpp": ("ignoreSslErrors()", "QSaveFile", "QImageReader", "safeUrlForLogs"),
    }
    cloud_dir = app / "cloud"
    for filename, required in components.items():
        path = cloud_dir / filename
        if not path.is_file():
            errors.append(f"missing extracted CloudBridge component: {path.relative_to(root)}")
            continue
        text = path.read_text(encoding="utf-8")
        for token in required:
            if token not in text:
                errors.append(f"{filename} missing responsibility marker {token!r}")
        cmake_token = f"app/cloud/{filename}"
        if cmake_token not in cmake:
            errors.append(f"CMake does not compile {cmake_token}")

    download = (cloud_dir / "CloudDownloadController.cpp").read_text(encoding="utf-8")
    if "setRawHeader(" in download:
        errors.append("signed-URL download controller must not inject Workbench headers")

    thumbnail = (cloud_dir / "ThumbnailService.cpp").read_text(encoding="utf-8")
    if thumbnail.count("ignoreSslErrors()") != 1:
        errors.append("ThumbnailService must contain exactly one thumbnail-only ignoreSslErrors() call")

    if errors:
        return fail(errors)

    print("CloudBridge architecture check passed")
    print(f"- CloudBridge.cpp: {len(bridge.splitlines())} lines")
    print("- thumbnail, upload/PWSZ and signed-download responsibilities extracted")
    print("- mapping/message support extracted")
    return 0


if __name__ == "__main__":
    sys.exit(main())
