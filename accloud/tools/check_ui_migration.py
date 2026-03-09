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


def check_tabs_geometry_v2() -> list[str]:
    issues: list[str] = []

    app_page_frame = QML_ROOT / "components" / "AppPageFrame.qml"
    app_page_frame_text = read_text(app_page_frame)
    if "property bool embeddedInTabsContainer: false" not in app_page_frame_text:
        issues.append("AppPageFrame missing `embeddedInTabsContainer` property")
    if "radius: embeddedInTabsContainer ? 0 : Theme.radiusDialog" not in app_page_frame_text:
        issues.append("AppPageFrame missing conditional radius for embedded tabs container")
    if "border.width: embeddedInTabsContainer ? 0 : Theme.borderWidth" not in app_page_frame_text:
        issues.append("AppPageFrame missing conditional border width for embedded tabs container")

    main_window = QML_ROOT / "MainWindow.qml"
    main_text = read_text(main_window)
    for required in (
        "anchors.margins: 0",
        "spacing: 0",
        "embeddedInTabsContainer: true",
        "stripColor: Theme.bgSurface",
        "tabTopCornerRadius: Theme.radiusControl",
    ):
        if required not in main_text:
            issues.append(f"MainWindow tabsPanel missing `{required}`")

    cloud_files_page = QML_ROOT / "pages" / "CloudFilesPage.qml"
    cloud_text = read_text(cloud_files_page)
    if "property bool embeddedInTabsContainer: false" not in cloud_text:
        issues.append("CloudFilesPage missing `embeddedInTabsContainer` property")
    if "embeddedInTabsContainer: root.embeddedInTabsContainer" not in cloud_text:
        issues.append("CloudFilesPage pageFrame missing embedded propagation")

    printer_page = QML_ROOT / "pages" / "PrinterPage.qml"
    printer_page_text = read_text(printer_page)
    if "property bool embeddedInTabsContainer: false" not in printer_page_text:
        issues.append("PrinterPage missing `embeddedInTabsContainer` property")
    if "embeddedInTabsContainer: root.embeddedInTabsContainer" not in printer_page_text:
        issues.append("PrinterPage missing embedded propagation to PrinterMainPanel")

    printer_main_panel = QML_ROOT / "pages" / "PrinterMainPanel.qml"
    printer_main_text = read_text(printer_main_panel)
    for required in (
        "id: printerTabsContainer",
        "spacing: 0",
        "embeddedInTabsContainer: true",
    ):
        if required not in printer_main_text:
            issues.append(f"PrinterMainPanel missing `{required}` for unified tabs container")

    printers_tabs = QML_ROOT / "pages" / "PrintersTabsBar.qml"
    printers_tabs_text = read_text(printers_tabs)
    for required in (
        "property bool embeddedInTabsContainer: false",
        "radius: embeddedInTabsContainer ? 0 : Theme.radiusControl",
        "border.width: embeddedInTabsContainer ? 0 : Theme.borderWidth",
        "tabTopCornerRadius: embeddedInTabsContainer ? Theme.radiusControl : root.radius",
    ):
        if required not in printers_tabs_text:
            issues.append(f"PrintersTabsBar missing `{required}`")

    printer_details = QML_ROOT / "pages" / "PrinterDetailPanel.qml"
    printer_details_text = read_text(printer_details)
    for required in (
        "property bool embeddedInTabsContainer: false",
        "radius: embeddedInTabsContainer ? 0 : Theme.radiusControl",
        "border.width: embeddedInTabsContainer ? 0 : Theme.borderWidth",
    ):
        if required not in printer_details_text:
            issues.append(f"PrinterDetailPanel missing `{required}`")

    cloud_details = QML_ROOT / "pages" / "CloudFileDetailsDialog.qml"
    cloud_details_text = read_text(cloud_details)
    for required in (
        "id: detailsTabsContainer",
        "spacing: 0",
        "stripColor: Theme.bgDialog",
        "tabTopCornerRadius: detailsTabsContainer.radius",
    ):
        if required not in cloud_details_text:
            issues.append(f"CloudFileDetailsDialog missing `{required}`")

    app_tab_bar = QML_ROOT / "components" / "AppTabBar.qml"
    app_tab_bar_text = read_text(app_tab_bar)
    for required in (
        "property int tabTopCornerRadius:",
        "readonly property bool _hasActiveTab:",
        "width: root._hasActiveTab ? Math.max(0, root._activeTabLeft) : parent.width",
        "width: root._hasActiveTab ? Math.max(0, parent.width - root._activeTabRight) : 0",
    ):
        if required not in app_tab_bar_text:
            issues.append(f"AppTabBar missing v2 geometry rule `{required}`")

    app_tab_button = QML_ROOT / "components" / "AppTabButton.qml"
    app_tab_button_text = read_text(app_tab_button)
    for required in (
        "readonly property bool lastVisibleTab:",
        "readonly property int verticalBorderBottomMargin:",
        "id: tabStrokeCanvas",
        "ctx.arc(",
        "onLastVisibleTabChanged: tabStrokeCanvas.requestPaint()",
    ):
        if required not in app_tab_button_text:
            issues.append(f"AppTabButton missing v2 geometry rule `{required}`")

    return issues


def check_tab_stroke_tokens() -> list[str]:
    issues: list[str] = []

    theme_js = QML_ROOT / "components" / "Theme.js"
    theme_text = read_text(theme_js)
    for required in (
        "var tabStrokeWidth = 1",
        "var tabStrokeColor =",
        "var tabBaselineColor =",
        "tabStrokeWidth = borderWidth",
        "tabStrokeColor = palette.borderDefault",
        "tabBaselineColor = palette.borderDefault",
    ):
        if required not in theme_text:
            issues.append(f"Theme.js missing tab stroke token `{required}`")

    app_tab_bar = QML_ROOT / "components" / "AppTabBar.qml"
    bar_text = read_text(app_tab_bar)
    for required in (
        "property color baselineColor: Theme.tabBaselineColor",
        "property int baselineWidth: Theme.tabStrokeWidth",
    ):
        if required not in bar_text:
            issues.append(f"AppTabBar missing token wiring `{required}`")

    app_tab_button = QML_ROOT / "components" / "AppTabButton.qml"
    button_text = read_text(app_tab_button)
    for required in (
        "readonly property int strokeWidth: Theme.tabStrokeWidth",
        "return Theme.tabStrokeColor",
    ):
        if required not in button_text:
            issues.append(f"AppTabButton missing token wiring `{required}`")

    return issues


def check_form_components() -> list[str]:
    issues: list[str] = []

    form_label = QML_ROOT / "components" / "FormLabel.qml"
    form_row = QML_ROOT / "components" / "FormRow.qml"
    if not form_label.exists():
        issues.append("FormLabel.qml missing")
    else:
        form_label_text = read_text(form_label)
        for required in (
            "font.pixelSize: Theme.fontBodyPx",
            "color: Theme.fgPrimary",
        ):
            if required not in form_label_text:
                issues.append(f"FormLabel missing `{required}`")

    if not form_row.exists():
        issues.append("FormRow.qml missing")
    else:
        form_row_text = read_text(form_row)
        for required in (
            "default property alias fieldData: fieldsRow.data",
            "FormLabel {",
            "spacing: Theme.gapRow",
        ):
            if required not in form_row_text:
                issues.append(f"FormRow missing `{required}`")

    for dialog in (
        QML_ROOT / "dialogs" / "PrintDraftDialog.qml",
        QML_ROOT / "dialogs" / "UploadDraftDialog.qml",
        QML_ROOT / "dialogs" / "SessionSettingsDialog.qml",
    ):
        text = read_text(dialog)
        if "FormRow {" not in text:
            issues.append(f"{dialog.relative_to(REPO_ROOT)} missing FormRow usage")

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
        QML_ROOT / "components" / "BusyOverlay.qml",
        QML_ROOT / "components" / "ErrorBanner.qml",
        QML_ROOT / "components" / "ProgressCard.qml",
        QML_ROOT / "components" / "FileCard.qml",
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
    errors.extend(f"- {msg}" for msg in check_tabs_geometry_v2())
    errors.extend(f"- {msg}" for msg in check_tab_stroke_tokens())
    errors.extend(f"- {msg}" for msg in check_form_components())

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
    print("- Tabs geometry/structure conforms to v2 corrections")
    print("- Tab stroke tokens standardized in Theme.js")
    print("- Form components conform to T3 corrections")
    print("- No legacy theme aliases in migrated files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
