#!/usr/bin/env python3
"""Guard the staged Photon/viewer integration and its production boundary."""

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
    experimental_qt_body = cmake_set_body(cmake, "ACCLOUD_EXPERIMENTAL_VIEWER_QT_SOURCES")
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
    if experimental_qt_body is None:
        errors.append("missing ACCLOUD_EXPERIMENTAL_VIEWER_QT_SOURCES")
        experimental_qt_sources: set[str] = set()
    else:
        experimental_qt_sources = normalized_cmake_sources(experimental_qt_body)

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
        if "render3d/qtquick/" not in str(path.relative_to(source_root))
    }
    if experimental_sources != expected_sources:
        missing = sorted(expected_sources - experimental_sources)
        extra = sorted(experimental_sources - expected_sources)
        if missing:
            errors.append(f"experimental source list is missing: {missing}")
        if extra:
            errors.append(f"experimental source list has unexpected entries: {extra}")

    expected_qt_sources = {
        "${ACCLOUD_SRC_ROOT}/render3d/qtquick/CompactShaderSources.h",
        "${ACCLOUD_SRC_ROOT}/render3d/qtquick/QmlGlItem.cpp",
        "${ACCLOUD_SRC_ROOT}/render3d/qtquick/QmlGlItem.h",
    }
    if experimental_qt_sources != expected_qt_sources:
        missing = sorted(expected_qt_sources - experimental_qt_sources)
        extra = sorted(experimental_qt_sources - expected_qt_sources)
        if missing:
            errors.append(f"experimental Qt source list is missing: {missing}")
        if extra:
            errors.append(f"experimental Qt source list has unexpected entries: {extra}")

    required_cmake_tokens = (
        "if(ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER)",
        "add_library(accloud_experimental_viewer OBJECT",
        "ACCLOUD_EXPERIMENTAL_VIEWER=1",
        "NAME accloud_experimental_viewer_architecture",
        "NAME accloud_experimental_viewer_scaffold",
        "NAME accloud_render_pipeline",
        "add_executable(accloud_render3d_worker_benchmark",
        "NAME accloud_render3d_worker_benchmark_selftest",
        "NAME accloud_render3d_shader_compile",
        "Qt6::OpenGL",
        "ACCLOUD_EXPERIMENTAL_VIEWER_QT_SOURCES",
    )
    for token in required_cmake_tokens:
        if token not in cmake:
            errors.append(f"missing CMake viewer-isolation token: {token}")

    forbidden_links = (
        "target_link_libraries(accloud_infra PRIVATE accloud_experimental_viewer",
        "target_sources(accloud_cli PRIVATE ${ACCLOUD_EXPERIMENTAL_VIEWER_SOURCES}",
    )
    for token in forbidden_links:
        if token in cmake:
            errors.append(f"experimental viewer bypasses its opt-in boundary: {token}")

    qt_link = "target_link_libraries(accloud_cli PRIVATE accloud_experimental_viewer Qt6::OpenGL)"
    viewer_if = cmake.find("if(ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER)", cmake.find("if(ACCLOUD_ENABLE_QT)"))
    qt_link_pos = cmake.find(qt_link)
    if qt_link_pos < 0 or viewer_if < 0 or qt_link_pos < viewer_if:
        errors.append("experimental Qt viewer must link to accloud_cli only inside the opt-in Qt branch")

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
    value = configure.get("default", {}).get("cacheVariables", {}).get(
        "ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER"
    )
    if value != "ON":
        errors.append("preset default must explicitly enable the per-file 3D viewer")
    for name in ("prod", "protected-core"):
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
    value = configure.get("experimental-viewer-qt", {}).get("cacheVariables", {}).get(
        "ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER"
    )
    if value != "ON":
        errors.append("experimental-viewer-qt must explicitly enable the viewer runtime")

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

    main_cpp = read(root / "src/accloud/app/main.cpp")
    qml_test_main = read(root / "tests/ui/tst_ui_main.cpp")
    qml_item_header = read(root / "src/accloud/render3d/qtquick/QmlGlItem.h")
    qml_item_cpp = read(root / "src/accloud/render3d/qtquick/QmlGlItem.cpp")
    shader_sources = read(root / "src/accloud/render3d/qtquick/CompactShaderSources.h")
    mesher_header = read(root / "src/accloud/render3d/meshing/LayerStackMesher.h")
    mesh_chunk_header = read(root / "src/accloud/domain/photons/MeshChunk.h")
    section_cache_header = read(root / "src/accloud/render3d/core/LayerSectionCache.h")
    section_cache_cpp = read(root / "src/accloud/render3d/core/LayerSectionCache.cpp")
    logger_test = read(root / "tests/cloud/test_jsonl_logger.cpp")
    registration_token = "qmlRegisterType<accloud::render3d::QmlGlItem>"
    if registration_token not in main_cpp:
        errors.append("QmlGlItem must remain registered with qmlRegisterType")

    qml_test_source_pattern = re.compile(
        r"target_sources\(accloud_qml_tests\s+PRIVATE\s+"
        r"\$\{ACCLOUD_EXPERIMENTAL_VIEWER_QT_SOURCES\}\s*\)",
        flags=re.DOTALL,
    )
    if not qml_test_source_pattern.search(cmake):
        errors.append("QML tests must compile the experimental Qt viewer sources")

    qml_test_link_bodies = re.findall(
        r"target_link_libraries\(accloud_qml_tests\s+PRIVATE(?P<body>.*?)\)",
        cmake,
        flags=re.DOTALL,
    )
    required_qml_test_links = (
        "accloud_infra",
        "accloud_experimental_viewer",
        "Qt6::OpenGL",
    )
    if not any(
        all(token in body for token in required_qml_test_links)
        for body in qml_test_link_bodies
    ):
        errors.append(
            "QML tests must link the runtime viewer, infrastructure logger and Qt OpenGL"
        )

    qml_test_definition_bodies = re.findall(
        r"target_compile_definitions\(accloud_qml_tests\s+PRIVATE(?P<body>.*?)\)",
        cmake,
        flags=re.DOTALL,
    )
    if not any(
        "ACCLOUD_EXPERIMENTAL_VIEWER=1" in body
        for body in qml_test_definition_bodies
    ):
        errors.append("QML tests must enable ACCLOUD_EXPERIMENTAL_VIEWER")

    for token in (
        '#include "render3d/qtquick/QmlGlItem.h"',
        registration_token,
        '"Accloud.Render3D", 1, 0, "VolumeViewer"',
    ):
        if token not in qml_test_main:
            errors.append(f"QML test bootstrap is missing viewer registration token: {token}")
    if re.search(r"class\s+QmlGlItem\s+final\s*:", qml_item_header):
        errors.append(
            "QmlGlItem registered with qmlRegisterType must not be final because "
            "Qt derives QQmlElement<T> from the registered type"
        )
    if not re.search(
        r"class\s+QmlGlItem\s*:\s*public\s+QQuickFramebufferObject",
        qml_item_header,
    ):
        errors.append("QmlGlItem must derive from QQuickFramebufferObject")
    if "Q_PROPERTY(int layerStep" in qml_item_header or "setLayerStep" in qml_item_header:
        errors.append("QmlGlItem must not expose a runtime sampling-mode API")
    for token in (
        "constexpr std::size_t kViewerLayerStride = 2",
        "options.layerStride = kViewerLayerStride",
        "sampledMaskLayer(std::size_t layer)",
    ):
        if token not in qml_item_cpp:
            errors.append(f"missing fixed quick-preview contract token: {token}")
    if "Q_PROPERTY(int workerCount" not in qml_item_header or "int workerCount_ = 4" not in qml_item_header:
        errors.append("QmlGlItem must expose four mesh workers by default")
    if "std::size_t layerStride = 1" not in mesher_header:
        errors.append("LayerStackMesher must keep an explicit layerStride option")
    for token in (
        "kMinimumMeshWorkerCount = 1",
        "kMaximumMeshWorkerCount = 16",
        "std::size_t workerCount = 4",
        "std::size_t chunkLayerCount = 8",
        "MeshWorkerStats",
        "CutSurfaceBoundary",
        "buildCutSurface",
    ):
        if token not in mesher_header:
            errors.append(f"missing parallel mesher contract token: {token}")
    for token in (
        'kRender3dLogSource = "render3d"',
        "options.layerStride",
        '"chunk_ready"',
        '"build_completed"',
        '"compact_chunk_uploaded"',
        '"cut_surface_uploaded"',
        '"boundary_built"',
        "u_cutSurfacePass",
        "readyCutBatch_",
        "cut_surface_swap_committed",
        "displayedFirstLayer_",
        "LayerSectionCache",
        "glDrawArraysInstanced",
        "glVertexAttribIPointer",
        "glVertexAttribDivisor",
        "shader::kCompactVertexShader",
        "shader::kCompactFragmentShader",
        '"u_clipEpsilon"',
        "shaderInitializationFailed_",
        'reportGpuFailure(QmlGlItem::tr("Unable to initialize the 3D renderer."))',
        "kGpuBudgetBytes",
        '"legacy_equivalent_bytes"',
        '"worker_completed"',
        "options.workerCount",
        "kChunkLayers = 8",
    ):
        if token not in qml_item_cpp:
            errors.append(f"missing Render3D diagnostic token: {token}")
    for obsolete in (
        "cutUploadQueue_",
        "clearCutRequested_",
    ):
        if obsolete in qml_item_cpp or obsolete in qml_item_header:
            errors.append(f"obsolete non-transactional cut-surface token remains: {obsolete}")

    for token in (
        "struct LayerSectionRect",
        "static_assert(sizeof(LayerSectionRect) == 8",
        "class LayerSectionCache",
        "maximumBytes",
        "maximumEntries",
        "materializeLayerSection",
    ):
        if token not in section_cache_header:
            errors.append(f"missing transactional section-cache token: {token}")
    for token in (
        "entries_.splice",
        "evictToFit",
        "packZSurface",
    ):
        if token not in section_cache_cpp:
            errors.append(f"missing section-cache implementation token: {token}")

    for token in (
        "layout(location = 0) in uvec2 a_packed;",
        "uniform vec3 u_pitch;",
        "uniform float u_clipEpsilon;",
        "float epsilon = u_clipEpsilon;",
    ):
        if token not in shader_sources:
            errors.append(f"missing compact shader source token: {token}")
    for obsolete in (
        "const float epsilon",
        "u_pitch.z * 0.001",
    ):
        if obsolete in shader_sources:
            errors.append(f"invalid compact fragment shader token remains: {obsolete}")

    for token in (
        "struct PackedSurfaceQuad",
        "static_assert(sizeof(PackedSurfaceQuad) == 8",
        "std::vector<PackedSurfaceQuad> surfaces",
        "kLegacyBytesPerSurfaceQuad",
    ):
        if token not in mesh_chunk_header:
            errors.append(f"missing compact GPU surface contract token: {token}")
    for obsolete in ("glDrawElements", "QOpenGLBuffer::IndexBuffer", "chunk.vertices", "chunk.indices"):
        if obsolete in qml_item_cpp:
            errors.append(f"legacy expanded GPU mesh path remains active: {obsolete}")
    if 'logDir / "render3d.jsonl"' not in logger_test:
        errors.append("logging regression must verify the dedicated render3d.jsonl sink")

    main_qml = read(root / "src/accloud/ui/qml/MainWindow.qml")
    cloud_files_qml = read(root / "src/accloud/ui/qml/pages/CloudFilesPage.qml")
    cloud_row_qml = read(root / "src/accloud/ui/qml/pages/CloudFilesTableRow.qml")
    resources_qrc = read(root / "src/accloud/app/resources.qrc")
    app_dialog_frame = root / "src/accloud/ui/qml/components/AppDialogFrame.qml"
    viewer_page = root / "src/accloud/ui/qml/pages/VolumeViewerPage.qml"
    layer_range_control = root / "src/accloud/ui/qml/components/VerticalLayerRangeSlider.qml"
    viewer_build_modal = root / "src/accloud/ui/qml/components/ViewerBuildModal.qml"
    viewer_dialog = root / "src/accloud/ui/qml/pages/VolumeViewerDialog.qml"

    for token in (
        "experimentalViewerEnabled",
        "viewerEnabled: root.experimentalViewerEnabled",
        "render3dWorkerCount: 4",
        'render3dWorkerCountSettingsKey: "render3d.workerCount"',
        'objectName: "menuSettingsRender3dWorkers"',
        'objectName: "render3dWorkersDialog"',
        'from: 1',
        'to: 16',
        "render3dWorkerCount: root.render3dWorkerCount",
    ):
        if token not in main_qml:
            errors.append(f"missing file-action viewer QML token: {token}")
    for obsolete in (
        "experimentalViewerTabComponent",
        'objectName: "viewerPageHost"',
        'objectName: "viewerTabButton"',
    ):
        if obsolete in main_qml:
            errors.append(f"obsolete viewer tab remains in MainWindow.qml: {obsolete}")

    for token in (
        'objectName: "viewerDialogLoader"',
        'source: "VolumeViewerDialog.qml"',
        "function requestViewer(fileId, fileName)",
        "onViewerRequested",
    ):
        if token not in cloud_files_qml:
            errors.append(f"missing cloud-file viewer routing token: {token}")
    for token in (
        'objectName: "fileRowViewerButton"',
        'endsWith(".pwsz")',
        "visible: root.viewerSupported",
        "enabled: root.viewerEnabled",
        "viewerRequested(root.fileId, root.fileName)",
    ):
        if token not in cloud_row_qml:
            errors.append(f"missing per-file PWSZ viewer action token: {token}")

    if not app_dialog_frame.is_file():
        errors.append("missing AppDialogFrame.qml")
    else:
        app_dialog_frame_text = read(app_dialog_frame)
        for token in (
            "property bool fullScreen: false",
            "width: fullScreen",
            "height: fullScreen",
            "radius: root.fullScreen ? 0 : Theme.radiusDialog",
        ):
            if token not in app_dialog_frame_text:
                errors.append(f"missing reusable dialog fullscreen token: {token}")

    for token in (
        'property string pendingViewerOpenFileId: ""',
        'function openViewerDialog(localPath, fileName, fileId)',
        'dialog.openFile(root.pendingViewerOpenPath,',
        'root.pendingViewerOpenFileId)',
        'function onPrintRequested(fileId, fileName)',
        'root.requestPrint(fileId, fileName)',
    ):
        if token not in cloud_files_qml:
            errors.append(f"missing viewer-to-print routing token: {token}")

    for resource in (
        "qml/pages/VolumeViewerPage.qml",
        "qml/pages/VolumeViewerDialog.qml",
        "qml/components/VerticalLayerRangeSlider.qml",
        "qml/components/ViewerBuildModal.qml",
    ):
        if resource not in resources_qrc:
            errors.append(f"{resource} is not packaged in resources.qrc")
    if not viewer_page.is_file():
        errors.append("missing experimental VolumeViewerPage.qml")
    else:
        viewer_page_text = read(viewer_page)
        if "import Accloud.Render3D 1.0" not in viewer_page_text:
            errors.append("VolumeViewerPage.qml must use the registered Render3D item")
        for token in (
            "property int workerCount: 4",
            "workerCount: root.workerCount",
            "readonly property int totalLayers: viewer.totalLayers",
            "readonly property int loadedChunkCount: viewer.loadedChunkCount",
            'objectName: "viewerStatusOverlay"',
            'objectName: "viewerFileSummaryLabel"',
            'qsTr("%1 · %2 layers")',
            'objectName: "viewerMachineLabel"',
            'objectName: "viewerNavigationHint"',
            'VerticalLayerRangeSlider {',
            'objectName: "viewerLayerRangeControl"',
            'onLowerLayerMoved: function(layer)',
            'onUpperLayerMoved: function(layer)',
            'ViewerBuildModal {',
            'objectName: "viewerBuildModal"',
            'running: viewer.loading',
            'progress: viewer.progress',
        ):
            if token not in viewer_page_text:
                errors.append(f"missing viewer sampling UI token: {token}")
        machine_index = viewer_page_text.find('objectName: "viewerMachineLabel"')
        summary_index = viewer_page_text.find('objectName: "viewerFileSummaryLabel"')
        hint_index = viewer_page_text.find('objectName: "viewerNavigationHint"')
        if (machine_index < 0 or summary_index < 0 or hint_index < 0
                or not (machine_index < summary_index < hint_index)):
            errors.append("viewer overlay order must be machine, file/layers, navigation help")
        for obsolete in (
            'objectName: "viewerFirstLayerSpin"',
            'objectName: "viewerLastLayerSpin"',
            'text: root.displayFileName.length > 0 ? root.displayFileName',
            'objectName: "viewerVisibleRangeLabel"',
            'objectName: "viewerSamplingModeCombo"',
            'objectName: "viewerResetCameraButton"',
            'objectName: "viewerTotalLayerLabel"',
            "viewer.layerStep",
            'qsTr("Fast preview · 1 layer out of 2")',
            'qsTr("Full detail · every layer")',
            'qsTr("%1 layers · %2 chunks · %3 triangles")',
            'qsTr("%1 to %2 · %3 layers · Z %4–%5 mm")',
            'qsTr("Mesh sampling: every layer")',
            'qsTr("Mesh sampling: 1 layer out of %1")',
            'qsTr("Mesh workers: %1")',
            'Decoding layers and building mesh… %1%',
            'BusyIndicator {',
        ):
            if obsolete in viewer_page_text:
                errors.append(f"obsolete or redundant viewer UI token remains: {obsolete}")
    if not viewer_build_modal.is_file():
        errors.append("missing ViewerBuildModal.qml")
    else:
        viewer_build_modal_text = read(viewer_build_modal)
        for token in (
            'objectName: "viewerBuildModal"',
            'property bool running: false',
            'property real progress: 0.0',
            'readonly property real boundedProgress:',
            'objectName: "viewerBuildInputBlocker"',
            'acceptedButtons: Qt.AllButtons',
            'onWheel: function(wheel) { wheel.accepted = true }',
            'objectName: "viewerBuildProgressCard"',
            'objectName: "viewerBuildProgressBar"',
            'from: 0.0',
            'to: 1.0',
            'value: root.boundedProgress',
            'anchors.centerIn: parent',
            'qsTr("Creating 3D view…")',
            'objectName: "viewerBuildProgressPercent"',
        ):
            if token not in viewer_build_modal_text:
                errors.append(f"missing viewer build modal token: {token}")

    if not layer_range_control.is_file():
        errors.append("missing VerticalLayerRangeSlider.qml")
    else:
        layer_range_text = read(layer_range_control)
        for token in (
            'orientation: Qt.Vertical',
            'objectName: "viewerLayerMaximumLabel"',
            'objectName: "viewerLayerMinimumLabel"',
            'objectName: "viewerLowerLayerHandle"',
            'objectName: "viewerUpperLayerHandle"',
            'ToolTip.visible: lowerLayerHover.hovered',
            'ToolTip.visible: upperLayerHover.hovered',
            'objectName: "viewerUpperLayerWheelArea"',
            'acceptedButtons: Qt.NoButton',
            'root.applyWheel(wheel.angleDelta.y)',
            'root.upperLayerMoved(nextLayer)',
        ):
            if token not in layer_range_text:
                errors.append(f"missing vertical layer-range control token: {token}")
        wheel_body = re.search(
            r"function applyWheel\(angleDeltaY\)\s*\{(?P<body>.*?)\n    \}",
            layer_range_text,
            flags=re.DOTALL,
        )
        if wheel_body is None:
            errors.append("VerticalLayerRangeSlider must expose applyWheel")
        elif "lowerLayerMoved" in wheel_body.group("body"):
            errors.append("mouse wheel must not move the lower layer handle")
    if not viewer_dialog.is_file():
        errors.append("missing experimental VolumeViewerDialog.qml")
    else:
        viewer_dialog_text = read(viewer_dialog)
        if "VolumeViewerPage" not in viewer_dialog_text:
            errors.append("VolumeViewerDialog.qml must host VolumeViewerPage")
        for token in (
            'title: qsTr("3D view")',
            'property string sourceFileId: ""',
            'signal printRequested(string fileId, string fileName)',
            'function openFile(localPath, fileName, fileId)',
            'objectName: "viewerDialogResetButton"',
            'enabled: viewerPage.loadedChunkCount > 0',
            'onClicked: viewerPage.resetView()',
            'objectName: "viewerDialogFullscreenButton"',
            'root.fullScreen ? qsTr("Exit full screen") : qsTr("Full screen")',
            'onClicked: root.toggleFullScreen()',
            'onClosed: root.fullScreen = false',
            'objectName: "viewerDialogPrintButton"',
            'text: qsTr("Print")',
            'enabled: root.sourceFileId.trim().length > 0',
            'root.printRequested(fileId, fileName)',
            'objectName: "viewerDialogCloseButton"',
        ):
            if token not in viewer_dialog_text:
                errors.append(f"missing viewer dialog title/fullscreen/footer token: {token}")
        for obsolete in (
            'qsTr("3D view — %1 · %2 layers")',
            'qsTr("3D view — %1")',
        ):
            if obsolete in viewer_dialog_text:
                errors.append(f"file summary must not remain in viewer dialog title: {obsolete}")
        reset_index = viewer_dialog_text.find('objectName: "viewerDialogResetButton"')
        fullscreen_index = viewer_dialog_text.find('objectName: "viewerDialogFullscreenButton"')
        print_index = viewer_dialog_text.find('objectName: "viewerDialogPrintButton"')
        close_index = viewer_dialog_text.find('objectName: "viewerDialogCloseButton"')
        if reset_index < 0 or fullscreen_index < 0 or reset_index > fullscreen_index:
            errors.append("viewer reset action must be immediately before the fullscreen action")
        if print_index < 0 or close_index < 0 or print_index > close_index:
            errors.append("viewer print action must be immediately before the close action")
        if "footerLeadingData:" in viewer_dialog_text:
            errors.append("viewer dialog actions must remain in header/trailing footer groups")

    qml_test = read(root / "tests/ui/qml/tst_control_room.qml")
    for object_name in (
        "render3dDefaultsDialog",
            "viewerDraftDialog",
        "viewerDialogButton",
        "viewerTabButton",
        "viewerPageHost",
    ):
        expected = f'findObjectByName(window, "{object_name}") === null'
        if expected not in qml_test:
            errors.append(f"QML regression does not assert removal of {object_name}")
    for token in (
        "test_experimental_viewer_is_a_file_action_not_a_tab",
        "test_cloud_file_row_routes_pwsz_viewer_action",
        "test_render3d_worker_setting_defaults_to_four_and_persists_bounds",
        'uiSettingsBridge.values["render3d.workerCount"]',
        'findObjectByName(window, "menuSettingsRender3dWorkers")',
        'findObjectByName(row, "fileRowViewerButton")',
        'compare(disabledRowButton.visible, true)',
        'compare(disabledRowButton.enabled, false)',
        'test_vertical_layer_range_wheel_moves_only_upper_handle',
        'test_viewer_build_modal_tracks_progress_and_hides_when_complete',
        'ViewerBuildModal.qml',
        'findObjectByName(modal, "viewerBuildProgressBar")',
        'compare(percent.text, "42%")',
        'test_volume_viewer_dialog_print_closes_and_forwards_cloud_file',
        'findObjectByName(dialog, "viewerDialogPrintButton")',
        'compare(requestedFileId, "cloud-file-42")',
        'test_app_dialog_frame_full_screen_mode_fills_overlay',
        'VerticalLayerRangeSlider.qml',
        'compare(lowerEvents.length, 0)',
    ):
        if token not in qml_test:
            errors.append(f"QML regression is missing per-file viewer assertion: {token}")

    shader_test = root / "tests/photons/test_render3d_shader_qt.cpp"
    if not shader_test.is_file():
        errors.append("missing Qt/OpenGL compact shader compilation regression")
    else:
        shader_test_text = read(shader_test)
        for token in (
            "kCompactVertexShader",
            "kCompactFragmentShader",
            '"u_clipEpsilon"',
            "QOffscreenSurface",
        ):
            if token not in shader_test_text:
                errors.append(f"compact shader compilation regression is missing token: {token}")

    smoke = root / "tests/photons/test_experimental_viewer_scaffold.cpp"
    if not smoke.is_file():
        errors.append("missing opt-in experimental viewer scaffold smoke test")

    benchmark = root / "tools/render3d_worker_benchmark.cpp"
    if not benchmark.is_file():
        errors.append("missing Render3D worker benchmark tool")
    else:
        benchmark_text = read(benchmark)
        for token in (
            "--workers",
            "--repeats",
            "--layer-stride",
            "--chunk-layers",
            "--output-prefix",
            "--self-test",
            "workerCounts{4, 8, 16}",
            "chunkLayerCounts{8, 16, 32}",
            "generated geometry differs between worker runs for chunk size",
        ):
            if token not in benchmark_text:
                errors.append(f"Render3D worker benchmark is missing token: {token}")

    for relative in (
        "docs/07-development-tests-patches.md",
        "docs/FR/07-developpement-tests-correctifs.md",
    ):
        benchmark_doc = read(root / relative)
        if "accloud_render3d_worker_benchmark" not in benchmark_doc:
            errors.append(f"{relative} does not document the worker benchmark")

    if errors:
        print("Experimental viewer architecture FAILED", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print(
        "Experimental viewer architecture passed: default desktop exposes the "
        "per-file PWSZ action, production remains disabled, and "
        f"{len(experimental_sources)} core sources plus {len(experimental_qt_sources)} Qt sources stay isolated behind the build option."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
