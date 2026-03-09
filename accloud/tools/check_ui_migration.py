#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
QML_ROOT = REPO_ROOT / "accloud" / "ui" / "qml"


def read_text(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8")


def find_raw_dialogs() -> list[pathlib.Path]:
    offenders: list[pathlib.Path] = []
    for path in QML_ROOT.rglob("*.qml"):
        if path.name == "AppDialogFrame.qml":
            continue
        text = read_text(path)
        if re.search(r"^\s*Dialog\s*\{", text, flags=re.MULTILINE):
            offenders.append(path)
    return offenders


def has_tab_setting(path: pathlib.Path, object_id: str, key: str, value: str) -> bool:
    text = read_text(path)
    pattern = (
        r"AppTabBar\s*\{"
        r"(?:(?!AppTabBar\s*\{).)*?"
        + re.escape(f"id: {object_id}")
        + r"(?:(?!\}).)*?"
        + re.escape(f'{key}: "{value}"')
    )
    return re.search(pattern, text, flags=re.DOTALL) is not None


def check_tabs_config() -> list[str]:
    issues: list[str] = []

    main_window = QML_ROOT / "MainWindow.qml"
    if not has_tab_setting(main_window, "controlTabs", "tabVariant", "navigation"):
        issues.append("MainWindow controlTabs missing `tabVariant: \"navigation\"`")
    if not has_tab_setting(main_window, "controlTabs", "tabSizingMode", "equal"):
        issues.append("MainWindow controlTabs missing `tabSizingMode: \"equal\"`")

    printers_tabs = QML_ROOT / "pages" / "PrintersTabsBar.qml"
    printers_text = read_text(printers_tabs)
    if 'tabVariant: "local"' not in printers_text:
        issues.append("PrintersTabsBar missing `tabVariant: \"local\"`")
    if 'tabSizingMode: "content"' not in printers_text:
        issues.append("PrintersTabsBar missing `tabSizingMode: \"content\"`")

    cloud_details = QML_ROOT / "pages" / "CloudFileDetailsDialog.qml"
    if not has_tab_setting(cloud_details, "detailsTabBar", "tabVariant", "local"):
        issues.append("CloudFileDetailsDialog detailsTabBar missing `tabVariant: \"local\"`")
    if not has_tab_setting(cloud_details, "detailsTabBar", "tabSizingMode", "content"):
        issues.append("CloudFileDetailsDialog detailsTabBar missing `tabSizingMode: \"content\"`")

    return issues


def find_legacy_alias_usage() -> list[str]:
    legacy = ("textPrimary", "textSecondary", "panel", "card", "panelStroke", "cardAlt")
    files_to_check = [
        QML_ROOT / "MainWindow.qml",
        QML_ROOT / "dialogs" / "PrintDraftDialog.qml",
        QML_ROOT / "dialogs" / "UploadDraftDialog.qml",
        QML_ROOT / "dialogs" / "ViewerDraftDialog.qml",
        QML_ROOT / "dialogs" / "SessionSettingsDialog.qml",
        QML_ROOT / "pages" / "CloudFilesPage.qml",
        QML_ROOT / "pages" / "CloudFileDetailsDialog.qml",
        QML_ROOT / "pages" / "PrinterPage.qml",
    ]

    offenders: list[str] = []
    for path in files_to_check:
        text = read_text(path)
        for alias in legacy:
            if re.search(rf"\bTheme\.{re.escape(alias)}\b", text):
                offenders.append(f"{path.relative_to(REPO_ROOT)} uses Theme.{alias}")
    return offenders


def main() -> int:
    errors: list[str] = []

    raw_dialogs = find_raw_dialogs()
    if raw_dialogs:
        errors.append("Raw Dialog usage found outside AppDialogFrame:")
        errors.extend(f"- {p.relative_to(REPO_ROOT)}" for p in raw_dialogs)

    errors.extend(f"- {msg}" for msg in check_tabs_config())

    legacy_usage = find_legacy_alias_usage()
    if legacy_usage:
        errors.append("Legacy theme aliases found in migrated files:")
        errors.extend(f"- {msg}" for msg in legacy_usage)

    if errors:
        print("UI migration check: FAILED")
        print("\n".join(errors))
        return 1

    print("UI migration check: OK")
    print("- No raw Dialog outside AppDialogFrame")
    print("- Tabs configuration conforms to T1/T2/T3")
    print("- No legacy theme aliases in migrated files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
