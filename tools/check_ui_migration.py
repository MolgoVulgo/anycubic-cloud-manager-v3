#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC_ROOT = REPO_ROOT / "src" / "accloud"
QML_ROOT = SRC_ROOT / "ui" / "qml"
APP_ROOT = SRC_ROOT / "app"


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


def has_tab_property(path: pathlib.Path, object_id: str, expression: str) -> bool:
    text = read_text(path)
    pattern = (
        r"AppTabBar\s*\{"
        r"(?:(?!AppTabBar\s*\{).)*?"
        + re.escape(f"id: {object_id}")
        + r"(?:(?!\}).)*?"
        + re.escape(expression)
    )
    return re.search(pattern, text, flags=re.DOTALL) is not None


def has_tab_setting(path: pathlib.Path, object_id: str, key: str, value: str) -> bool:
    return has_tab_property(path, object_id, f'{key}: "{value}"')


def has_object_property(path: pathlib.Path, object_type: str, object_id: str, expression: str) -> bool:
    text = read_text(path)
    pattern = (
        re.escape(object_type)
        + r"\s*\{"
        + r"(?:(?!\}).)*?"
        + re.escape(f"id: {object_id}")
        + r"(?:(?!\}).)*?"
        + re.escape(expression)
    )
    return re.search(pattern, text, flags=re.DOTALL) is not None


def check_tabs_config() -> list[str]:
    issues: list[str] = []

    main_window = QML_ROOT / "MainWindow.qml"
    if not has_tab_setting(main_window, "controlTabs", "tabVariant", "navigation"):
        issues.append("MainWindow controlTabs missing `tabVariant: \"navigation\"`")
    if not has_tab_setting(main_window, "controlTabs", "tabSizingMode", "content"):
        issues.append("MainWindow controlTabs missing `tabSizingMode: \"content\"`")
    if not has_tab_property(main_window, "controlTabs", "minTabWidth: 120"):
        issues.append("MainWindow controlTabs missing `minTabWidth: 120`")
    if not has_tab_property(main_window, "controlTabs", 'inactiveColor: "transparent"'):
        issues.append("MainWindow controlTabs missing transparent inactive background")
    if not has_tab_property(main_window, "controlTabs", 'stripColor: "transparent"'):
        issues.append("MainWindow controlTabs missing transparent strip background")

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
        'inactiveColor: "transparent"',
        'stripColor: "transparent"',
        "tabTopCornerRadius: Theme.radiusControl",
    ):
        if required not in main_text:
            issues.append(f"MainWindow tabsPanel missing `{required}`")

    if not has_object_property(main_window, "Rectangle", "tabsPanel", 'color: "transparent"'):
        issues.append("MainWindow tabsPanel missing transparent background")
    for required in (
        "Layout.leftMargin: Theme.borderWidth",
        "Layout.rightMargin: Theme.borderWidth",
        "Layout.bottomMargin: Theme.borderWidth",
    ):
        if not has_object_property(main_window, "StackLayout", "controlRoomStack", required):
            issues.append(f"MainWindow controlRoomStack missing `{required}`")

    cloud_files_page = QML_ROOT / "pages" / "CloudFilesPage.qml"
    cloud_text = read_text(cloud_files_page)
    if "property bool embeddedInTabsContainer: false" not in cloud_text:
        issues.append("CloudFilesPage missing `embeddedInTabsContainer` property")
    if "embeddedInTabsContainer: root.embeddedInTabsContainer" not in cloud_text:
        issues.append("CloudFilesPage pageFrame missing embedded propagation")
    if "readonly property int tableRowHorizontalMargin: 12" not in cloud_text:
        issues.append("CloudFilesPage missing coherent 12 px table side inset")
    if "scrollbarReserve: root.tableScrollbarReserve" not in cloud_text:
        issues.append("CloudFilesPage missing stable scrollbar reserve propagation")

    cloud_toolbar = QML_ROOT / "pages" / "CloudFilesToolbar.qml"
    toolbar_text = read_text(cloud_toolbar)
    for required in (
        'objectName: "filesPrimaryActionsHost"',
        'objectName: "filesPrimaryActions"',
        'objectName: "deleteSelectedFilesButton"',
        "visible: root.selectedFilesCount > 0 || root.batchDeleteRunning",
        "anchors.centerIn: parent",
    ):
        if required not in toolbar_text:
            issues.append(f"CloudFilesToolbar missing `{required}`")

    cloud_table_panel = QML_ROOT / "pages" / "CloudFilesTablePanel.qml"
    table_panel_text = read_text(cloud_table_panel)
    for required in (
        'objectName: "filesTablePanel"',
        "border.width: Theme.borderWidth",
        "Layout.leftMargin: root.tableRowHorizontalMargin",
        "Layout.rightMargin: root.tableRowHorizontalMargin",
        "horizontalMargin: root.tableRowHorizontalMargin",
        "signal fileSelectionToggled(string fileId, string fileName, bool checked)",
    ):
        if required not in table_panel_text:
            issues.append(f"CloudFilesTablePanel missing `{required}`")

    cloud_table_row = QML_ROOT / "pages" / "CloudFilesTableRow.qml"
    table_row_text = read_text(cloud_table_row)
    for required in (
        'objectName: "fileRowSelectionCheckBox"',
        "property bool batchSelected: false",
        "signal selectionToggled(string fileId, string fileName, bool checked)",
    ):
        if required not in table_row_text:
            issues.append(f"CloudFilesTableRow missing `{required}`")

    for required in (
        "property var selectedFiles: []",
        "readonly property int selectedFilesCount: selectedFiles.length",
        "function startBatchDelete()",
        "id: batchDeleteConfirmDialog",
        "onDeleteSelectedRequested: root.requestDeleteSelected()",
    ):
        if required not in cloud_text:
            issues.append(f"CloudFilesPage missing `{required}`")

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


def check_runtime_status_rules() -> list[str]:
    issues: list[str] = []
    status_files = [
        QML_ROOT / "MainWindow.qml",
        QML_ROOT / "pages" / "CloudFilesPage.qml",
        QML_ROOT / "pages" / "PrinterPage.qml",
        QML_ROOT / "dialogs" / "SessionSettingsDialog.qml",
    ]
    status_targets = r"(statusMsg|statusMessage|statusText|globalStatusMsg)"

    # Rule A: no status text built by free string concatenation.
    concat_re = re.compile(rf"\b{status_targets}\b\s*=\s*[^\n;]*\+")
    # Rule B: no raw backend text assigned directly to user-visible statuses.
    raw_re = re.compile(rf"\b{status_targets}\b\s*=\s*String\(")

    for path in status_files:
        text = read_text(path)
        for match in concat_re.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            issues.append(
                f"{path.relative_to(REPO_ROOT)}:{line} status assignment uses concatenation"
            )
        for match in raw_re.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            issues.append(
                f"{path.relative_to(REPO_ROOT)}:{line} status assignment uses raw String(...)"
            )

    return issues


def _extract_function_block(text: str, fn_name: str) -> str:
    fn_match = re.search(rf"\b{re.escape(fn_name)}\s*\([^)]*\)\s*\{{", text)
    if not fn_match:
        return ""
    start = fn_match.end() - 1
    depth = 0
    for i in range(start, len(text)):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[start : i + 1]
    return ""


def check_backend_message_contract() -> list[str]:
    issues: list[str] = []
    backend_files = [
        APP_ROOT / "CloudBridge.cpp",
        APP_ROOT / "SessionImportBridge.cpp",
    ]
    required_tokens = (
        'out.insert("messageKey"',
        'out.insert("fallbackMessage"',
        'out.insert("params"',
    )

    for path in backend_files:
        text = read_text(path)
        block = _extract_function_block(text, "finalizeUiMessage")
        if not block:
            issues.append(f"{path.relative_to(REPO_ROOT)} missing finalizeUiMessage()")
            continue
        for token in required_tokens:
            if token not in block:
                issues.append(
                    f"{path.relative_to(REPO_ROOT)} finalizeUiMessage() missing `{token}`"
                )

        if text.count('out.insert("messageKey"') < 5:
            issues.append(
                f"{path.relative_to(REPO_ROOT)} has too few explicit messageKey assignments"
            )

    return issues



def check_incremental_printer_models() -> list[str]:
    issues: list[str] = []
    printer_page = QML_ROOT / "pages" / "PrinterPage.qml"
    text = read_text(printer_page)

    forbidden = (
        "remoteCompatiblePrintersModel",
        "printCloudFilesModel",
        "printerLocalFilesModel",
    )
    for object_id in forbidden:
        pattern = rf"ListModel\s*\{{(?:(?!\}}).)*?id:\s*{re.escape(object_id)}\b"
        if re.search(pattern, text, flags=re.DOTALL):
            issues.append(
                f"PrinterPage still rebuilds `{object_id}` as a QML ListModel"
            )

    required = (
        "PrinterFilesModel {",
        "remoteCompatiblePrintersModel.replaceOrPatchPrinters(compatiblePrinters)",
        "printCloudFilesModel.replaceOrPatchFiles(compatibleFiles)",
        "printerLocalFilesModel.replaceOrPatchFiles(localFiles)",
    )
    for token in required:
        if token not in text:
            issues.append(f"PrinterPage incremental model contract missing `{token}`")

    model_header = APP_ROOT / "PrinterFilesModel.h"
    model_source = APP_ROOT / "PrinterFilesModel.cpp"
    if not model_header.exists() or not model_source.exists():
        issues.append("PrinterFilesModel C++ implementation is missing")

    return issues

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
    errors.extend(f"- {msg}" for msg in check_runtime_status_rules())
    errors.extend(f"- {msg}" for msg in check_backend_message_contract())
    errors.extend(f"- {msg}" for msg in check_incremental_printer_models())

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
    print("- Runtime statuses avoid raw text and free concatenation")
    print("- Backend UI message envelope contract is enforced")
    print("- Printer selection lists use incremental C++ models")
    print("- No legacy theme aliases in migrated files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
