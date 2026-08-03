cmake_minimum_required(VERSION 3.24)

project(accloud VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ACCLOUD_ENABLE_QT "Enable Qt6/QML frontend target" ON)
option(ACCLOUD_REQUIRE_QT "Fail configuration when the requested Qt stack is incomplete" OFF)
option(ACCLOUD_ENABLE_QML_TESTS "Enable Qt Quick Test UI tests" ON)
option(ACCLOUD_ENABLE_LIVE_TESTS "Enable tests requiring controlled external services" ON)
option(ACCLOUD_DEBUG "Enable global debug features (UI tools + debug payloads + debug traces)" OFF)
option(ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER
       "Build the isolated Photon/PWMB viewer core (not linked to the production runtime)"
       OFF)

if(ACCLOUD_ENABLE_QT)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)
else()
  set(CMAKE_AUTOMOC OFF)
  set(CMAKE_AUTORCC OFF)
endif()

set(ACCLOUD_REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(ACCLOUD_SRC_ROOT ${ACCLOUD_REPO_ROOT}/src/accloud)
set(ACCLOUD_TESTS_ROOT ${ACCLOUD_REPO_ROOT}/tests)
set(ACCLOUD_TOOLS_ROOT ${ACCLOUD_REPO_ROOT}/tools)
set(ACCLOUD_I18N_ROOT ${ACCLOUD_REPO_ROOT}/i18n)
set(ACCLOUD_DEPENDENCY_ARCHIVE "" CACHE FILEPATH
    "Optional offline dependency ZIP containing nlohmann_jsonConfig.cmake")

set(ACCLOUD_LOCAL_CACHE_SOURCES
    ${ACCLOUD_SRC_ROOT}/app/LocalCacheStore.cpp
    ${ACCLOUD_SRC_ROOT}/app/LocalCacheStore.h
    ${ACCLOUD_SRC_ROOT}/app/cache/LocalCacheSql.cpp
    ${ACCLOUD_SRC_ROOT}/app/cache/LocalCacheSql.h
    ${ACCLOUD_SRC_ROOT}/app/cache/LocalCacheFiles.cpp
    ${ACCLOUD_SRC_ROOT}/app/cache/LocalCachePrinters.cpp
    ${ACCLOUD_SRC_ROOT}/app/cache/LocalCacheJobs.cpp
    ${ACCLOUD_SRC_ROOT}/app/cache/LocalCacheState.cpp
)

set(ACCLOUD_MQTT_BRIDGE_SOURCES
    ${ACCLOUD_SRC_ROOT}/app/MqttBridge.cpp
    ${ACCLOUD_SRC_ROOT}/app/MqttBridge.h
    ${ACCLOUD_SRC_ROOT}/app/mqtt/MqttBridgeSession.cpp
    ${ACCLOUD_SRC_ROOT}/app/mqtt/MqttBridgeMessages.cpp
    ${ACCLOUD_SRC_ROOT}/app/mqtt/MqttBridgeTelemetry.cpp
)


find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
  set(_accloud_dependency_archive_candidates)
  if(ACCLOUD_DEPENDENCY_ARCHIVE)
    list(APPEND _accloud_dependency_archive_candidates "${ACCLOUD_DEPENDENCY_ARCHIVE}")
  endif()
  if(DEFINED ENV{ACCLOUD_DEPENDENCY_ARCHIVE} AND NOT "$ENV{ACCLOUD_DEPENDENCY_ARCHIVE}" STREQUAL "")
    list(APPEND _accloud_dependency_archive_candidates "$ENV{ACCLOUD_DEPENDENCY_ARCHIVE}")
  endif()
  list(APPEND _accloud_dependency_archive_candidates
      "${ACCLOUD_REPO_ROOT}/accloud-build-deps.zip"
      "${CMAKE_CURRENT_SOURCE_DIR}/accloud-build-deps.zip")

  set(_accloud_dependency_archive "")
  foreach(_candidate IN LISTS _accloud_dependency_archive_candidates)
    get_filename_component(_candidate_abs "${_candidate}" ABSOLUTE BASE_DIR "${ACCLOUD_REPO_ROOT}")
    if(EXISTS "${_candidate_abs}")
      set(_accloud_dependency_archive "${_candidate_abs}")
      break()
    endif()
  endforeach()

  if(_accloud_dependency_archive)
    set(_accloud_dependency_extract_root "${CMAKE_BINARY_DIR}/_deps/accloud-build-deps")
    file(REMOVE_RECURSE "${_accloud_dependency_extract_root}")
    file(MAKE_DIRECTORY "${_accloud_dependency_extract_root}")
    file(ARCHIVE_EXTRACT
        INPUT "${_accloud_dependency_archive}"
        DESTINATION "${_accloud_dependency_extract_root}")
    file(GLOB_RECURSE _accloud_nlohmann_configs
        LIST_DIRECTORIES FALSE
        "${_accloud_dependency_extract_root}/nlohmann_jsonConfig.cmake")
    list(LENGTH _accloud_nlohmann_configs _accloud_nlohmann_config_count)
    if(_accloud_nlohmann_config_count EQUAL 0)
      message(FATAL_ERROR
          "Offline dependency archive does not contain nlohmann_jsonConfig.cmake: ${_accloud_dependency_archive}")
    endif()
    list(GET _accloud_nlohmann_configs 0 _accloud_nlohmann_config)
    get_filename_component(_accloud_nlohmann_dir "${_accloud_nlohmann_config}" DIRECTORY)
    set(nlohmann_json_DIR "${_accloud_nlohmann_dir}" CACHE PATH
        "Directory containing nlohmann_jsonConfig.cmake" FORCE)
    message(STATUS "Using offline dependency archive: ${_accloud_dependency_archive}")
  endif()

  find_package(nlohmann_json CONFIG REQUIRED)
endif()

add_library(accloud_domain INTERFACE)

target_include_directories(accloud_domain INTERFACE ${ACCLOUD_SRC_ROOT})

set(ACCLOUD_EXPERIMENTAL_VIEWER_SOURCES
    infra/photons/PhotonsReader.cpp
    infra/photons/drivers/pwmb/PwmbDriver.cpp
    infra/photons/drivers/pwmb/PwmbTables.cpp
    infra/photons/drivers/pwmb/PwmbSections.cpp
    infra/photons/drivers/pwmb/codec/Pw0Decoder.cpp
    infra/photons/drivers/pwmb/codec/PwsDecoder.cpp
    infra/photons/drivers/pwmb/preview/PreviewDecoder.cpp
    infra/photons/drivers/pws/PwsDriver.cpp
    infra/photons/drivers/pws/codec/PwsDecoder.cpp
    infra/photons/drivers/phz/PhzDriver.cpp
    infra/photons/drivers/phz/preview/PhzPreviewDecoder.cpp
    infra/photons/drivers/photons/PhotonsDriver.cpp
    infra/photons/drivers/photons/preview/Rgb565Decoder.cpp
    infra/photons/drivers/pwsz/PwszArchiveReader.cpp
    infra/photons/drivers/pwsz/PwszLinesDriver.cpp
    infra/photons/drivers/pwsz/PwszSceneDriver.cpp
    infra/jobs/JobManager.cpp
    infra/jobs/Jobs_OpenPhotons.cpp
    infra/jobs/Jobs_BuildMasks.cpp
    infra/jobs/Jobs_Build3D.cpp
    infra/jobs/Jobs_Cloud.cpp
    infra/jobs/Jobs_ExportReport.cpp
    infra/cache/RamWindowCache.cpp
    infra/cache/DiskLruCache.cpp
    infra/cache/CacheIndex.cpp
    render3d/core/LayerVisibility.cpp
    render3d/core/LayerSectionCache.cpp
    render3d/core/OrbitCamera.cpp
    render3d/meshing/LayerStackMesher.cpp
    render3d/gl/Renderer.cpp
    render3d/gl/UploadQueue.cpp
    render3d/gl/MeshlessVolume.cpp
)

set(ACCLOUD_EXPERIMENTAL_VIEWER_QT_SOURCES
    ${ACCLOUD_SRC_ROOT}/render3d/qtquick/CompactShaderSources.h
    ${ACCLOUD_SRC_ROOT}/render3d/qtquick/QmlGlItem.cpp
    ${ACCLOUD_SRC_ROOT}/render3d/qtquick/QmlGlItem.h
)

set(ACCLOUD_INFRA_SOURCES
    infra/cloud/CloudClient.cpp
    infra/cloud/HarImporter.cpp
    infra/cloud/SignHeaders.cpp
    infra/cloud/core/HttpClient.cpp
    infra/cloud/core/WorkbenchSigner.cpp
    infra/cloud/core/WorkbenchRequestBuilder.cpp
    infra/cloud/core/EndpointRegistry.cpp
    infra/cloud/core/ResponseEnvelopeParser.cpp
    infra/cloud/core/SessionProvider.cpp
    infra/mqtt/core/MqttTlsPathResolver.cpp
    infra/mqtt/core/TlsMaterialProvider.cpp
    infra/mqtt/core/MqttSessionManager.cpp
    infra/mqtt/core/MqttCredentialProvider.cpp
    infra/mqtt/routing/MqttTopicBuilder.cpp
    infra/mqtt/routing/MqttMessageRouter.cpp
    infra/mqtt/observability/MqttTelemetry.cpp
    infra/mqtt/observability/TelemetryObservationStore.cpp
    infra/cloud/api/ApiSupport.cpp
    infra/cloud/api/AuthApi.cpp
    infra/cloud/api/FilesApi.cpp
    infra/cloud/api/QuotaApi.cpp
    infra/cloud/api/PrintersApi.cpp
    infra/cloud/api/ProjectsApi.cpp
    infra/cloud/api/DownloadsApi.cpp
    infra/cloud/api/UploadsApi.cpp
    infra/cloud/api/ReasonCatalogApi.cpp
    infra/cloud/api/PrintOrderApi.cpp
    infra/cloud/api/ThumbnailCandidateBuilder.cpp
    infra/cloud/thumbnail/ThumbnailValidation.cpp
    infra/cloud/archive/PwszPreviewArchive.cpp
    infra/logging/JsonlLogger.cpp
    infra/logging/RawTrafficLogger.cpp
    infra/logging/Redactor.cpp
    infra/logging/Rotator.cpp
    app/realtime/PrinterRealtimeStore.cpp
    app/usecases/cloud/ApplyRealtimeOverlayUseCase.cpp
    app/usecases/cloud/OrderResponseTracker.cpp
    app/usecases/cloud/ResyncCloudStateUseCase.cpp
    app/usecases/cloud/CheckCloudSessionUseCase.cpp
    app/usecases/cloud/LoadCloudFilesUseCase.cpp
    app/usecases/cloud/LoadCloudQuotaUseCase.cpp
    app/usecases/cloud/LoadPrintersDashboardUseCase.cpp
    app/usecases/cloud/GetDownloadUrlUseCase.cpp
    app/usecases/cloud/UploadLocalFileUseCase.cpp
    app/usecases/cloud/CloudPwszReplacementWorkflow.cpp
    app/usecases/cloud/UpdateCloudPwszPreviewsUseCase.cpp
    app/usecases/cloud/FetchPrinterCompatibilityByExtUseCase.cpp
    app/usecases/cloud/FetchPrinterCompatibilityByFileIdUseCase.cpp
    app/usecases/cloud/FetchPrinterDetailsUseCase.cpp
    app/usecases/cloud/FetchReasonCatalogUseCase.cpp
    app/usecases/cloud/FetchPrinterProjectsUseCase.cpp
    app/usecases/cloud/SendPrintOrderUseCase.cpp
    app/usecases/cloud/SendPrinterOrderUseCase.cpp
    app/usecases/cloud/DeleteCloudFileUseCase.cpp
    app/usecases/cloud/DeleteCloudFilesUseCase.cpp
    app/usecases/printing/DirectPrintLifecycleUseCase.cpp
)
list(TRANSFORM ACCLOUD_INFRA_SOURCES PREPEND "${ACCLOUD_SRC_ROOT}/")

add_library(accloud_infra OBJECT ${ACCLOUD_INFRA_SOURCES})
target_include_directories(accloud_infra PRIVATE ${ACCLOUD_SRC_ROOT})
target_compile_definitions(accloud_infra PRIVATE ACCLOUD_REPO_ROOT_PATH="${ACCLOUD_REPO_ROOT}")
target_link_libraries(accloud_infra PRIVATE accloud_domain)
target_link_libraries(accloud_infra PUBLIC nlohmann_json::nlohmann_json)

if(ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER)
  set(_accloud_experimental_viewer_sources ${ACCLOUD_EXPERIMENTAL_VIEWER_SOURCES})
  list(TRANSFORM _accloud_experimental_viewer_sources PREPEND "${ACCLOUD_SRC_ROOT}/")

  find_package(ZLIB REQUIRED)

  add_library(accloud_experimental_viewer OBJECT ${_accloud_experimental_viewer_sources})
  target_include_directories(accloud_experimental_viewer PRIVATE ${ACCLOUD_SRC_ROOT})
  target_link_libraries(accloud_experimental_viewer PRIVATE
      accloud_domain
      nlohmann_json::nlohmann_json
      ZLIB::ZLIB)
  target_compile_definitions(accloud_experimental_viewer PRIVATE
      ACCLOUD_EXPERIMENTAL_VIEWER=1)
endif()

add_executable(accloud_cli
    ${ACCLOUD_SRC_ROOT}/app/main.cpp
    ${ACCLOUD_SRC_ROOT}/app/App.cpp
)
set_source_files_properties(${ACCLOUD_SRC_ROOT}/app/main.cpp PROPERTIES SKIP_AUTOMOC ON)
target_link_libraries(accloud_cli PRIVATE accloud_domain accloud_infra)

find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
  target_link_libraries(accloud_infra PRIVATE OpenSSL::Crypto)
  target_link_libraries(accloud_cli PRIVATE OpenSSL::Crypto)
  target_compile_definitions(accloud_infra PRIVATE ACCLOUD_WITH_OPENSSL=1)
  target_compile_definitions(accloud_cli PRIVATE ACCLOUD_WITH_OPENSSL=1)
endif()

find_library(ACCLOUD_CRYPT_LIBRARY crypt)
if(ACCLOUD_CRYPT_LIBRARY)
  target_link_libraries(accloud_infra PRIVATE ${ACCLOUD_CRYPT_LIBRARY})
  target_link_libraries(accloud_cli PRIVATE ${ACCLOUD_CRYPT_LIBRARY})
endif()

if(ACCLOUD_DEBUG)
  target_compile_definitions(accloud_infra PRIVATE ACCLOUD_DEBUG=1)
  target_compile_definitions(accloud_cli PRIVATE ACCLOUD_DEBUG=1)
endif()

if(ACCLOUD_ENABLE_QT)
  find_package(Qt6 QUIET COMPONENTS Quick QuickControls2 Network Sql Gui)
  if(Qt6_FOUND)
    target_compile_definitions(accloud_infra PRIVATE ACCLOUD_WITH_QT=1)
    target_link_libraries(accloud_infra PRIVATE Qt6::Network Qt6::Gui)
    find_package(Qt6 QUIET COMPONENTS Mqtt)
    if(TARGET Qt6::Mqtt)
      target_link_libraries(accloud_infra PRIVATE Qt6::Mqtt)
      target_link_libraries(accloud_cli PRIVATE Qt6::Mqtt)
      target_compile_definitions(accloud_infra PRIVATE ACCLOUD_WITH_MQTT=1)
      target_compile_definitions(accloud_cli PRIVATE ACCLOUD_WITH_MQTT=1)
    else()
      if(ACCLOUD_REQUIRE_QT)
        message(FATAL_ERROR "Qt6::Mqtt is required by the selected validation profile")
      endif()
      message(WARNING "Qt6::Mqtt not found: MQTT runtime is unavailable on this machine")
    endif()
    target_compile_definitions(accloud_cli PRIVATE ACCLOUD_WITH_QT=1)
    set(ACCLOUD_APP_QT_SOURCES
        ${ACCLOUD_SRC_ROOT}/app/resources.qrc
        ${ACCLOUD_SRC_ROOT}/app/AppI18nBridge.cpp
        ${ACCLOUD_SRC_ROOT}/app/AppI18nBridge.h
        ${ACCLOUD_SRC_ROOT}/app/CloudFilesModel.cpp
        ${ACCLOUD_SRC_ROOT}/app/CloudFilesModel.h
        ${ACCLOUD_SRC_ROOT}/app/CloudBridge.cpp
        ${ACCLOUD_SRC_ROOT}/app/CloudBridge.h
        ${ACCLOUD_SRC_ROOT}/app/cloud/CloudBridgeSupport.cpp
        ${ACCLOUD_SRC_ROOT}/app/cloud/CloudBridgeSupport.h
        ${ACCLOUD_SRC_ROOT}/app/cloud/CloudDownloadController.cpp
        ${ACCLOUD_SRC_ROOT}/app/cloud/CloudDownloadController.h
        ${ACCLOUD_SRC_ROOT}/app/cloud/CloudUploadController.cpp
        ${ACCLOUD_SRC_ROOT}/app/cloud/CloudUploadController.h
        ${ACCLOUD_SRC_ROOT}/app/cloud/ThumbnailService.cpp
        ${ACCLOUD_SRC_ROOT}/app/cloud/ThumbnailService.h
        ${ACCLOUD_SRC_ROOT}/app/CloudFilesWorkflowBridge.cpp
        ${ACCLOUD_SRC_ROOT}/app/CloudFilesWorkflowBridge.h
        ${ACCLOUD_SRC_ROOT}/app/PrintWorkflowBridge.cpp
        ${ACCLOUD_SRC_ROOT}/app/PrintWorkflowBridge.h
        ${ACCLOUD_SRC_ROOT}/app/printing/PendingDirectPrintStore.cpp
        ${ACCLOUD_SRC_ROOT}/app/printing/PendingDirectPrintStore.h
        ${ACCLOUD_SRC_ROOT}/app/printing/PrinterFileCompatibility.cpp
        ${ACCLOUD_SRC_ROOT}/app/printing/PrinterFileCompatibility.h
        ${ACCLOUD_SRC_ROOT}/app/usecases/printing/PrepareRemotePrintUseCase.cpp
        ${ACCLOUD_SRC_ROOT}/app/usecases/printing/PrepareRemotePrintUseCase.h
        ${ACCLOUD_SRC_ROOT}/app/ThumbnailCachePolicy.cpp
        ${ACCLOUD_SRC_ROOT}/app/ThumbnailCachePolicy.h
        ${ACCLOUD_MQTT_BRIDGE_SOURCES}
        ${ACCLOUD_SRC_ROOT}/app/MqttTailModel.cpp
        ${ACCLOUD_SRC_ROOT}/app/MqttTailModel.h
        ${ACCLOUD_SRC_ROOT}/app/PrintersModel.cpp
        ${ACCLOUD_SRC_ROOT}/app/PrintersModel.h
        ${ACCLOUD_SRC_ROOT}/app/PrinterFilesModel.cpp
        ${ACCLOUD_SRC_ROOT}/app/PrinterFilesModel.h
        ${ACCLOUD_SRC_ROOT}/app/RecentJobsModel.cpp
        ${ACCLOUD_SRC_ROOT}/app/RecentJobsModel.h
        ${ACCLOUD_LOCAL_CACHE_SOURCES}
        ${ACCLOUD_SRC_ROOT}/app/UiSettingsBridge.cpp
        ${ACCLOUD_SRC_ROOT}/app/UiSettingsBridge.h
        ${ACCLOUD_SRC_ROOT}/app/SessionImportBridge.cpp
        ${ACCLOUD_SRC_ROOT}/app/SessionImportBridge.h
    )

    if(ACCLOUD_DEBUG)
      list(APPEND ACCLOUD_APP_QT_SOURCES
          ${ACCLOUD_SRC_ROOT}/app/resources_debug.qrc
          ${ACCLOUD_SRC_ROOT}/app/LogBridge.cpp
          ${ACCLOUD_SRC_ROOT}/app/LogBridge.h
          ${ACCLOUD_SRC_ROOT}/app/LogTailModel.cpp
          ${ACCLOUD_SRC_ROOT}/app/LogTailModel.h
          ${ACCLOUD_SRC_ROOT}/app/UiClickTracer.cpp
          ${ACCLOUD_SRC_ROOT}/app/UiClickTracer.h
      )
    endif()

    if(ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER)
      find_package(Qt6 QUIET COMPONENTS OpenGL)
      if(TARGET Qt6::OpenGL)
        list(APPEND ACCLOUD_APP_QT_SOURCES ${ACCLOUD_EXPERIMENTAL_VIEWER_QT_SOURCES})
        target_link_libraries(accloud_cli PRIVATE accloud_experimental_viewer Qt6::OpenGL)
        target_compile_definitions(accloud_cli PRIVATE ACCLOUD_EXPERIMENTAL_VIEWER=1)
      elseif(ACCLOUD_REQUIRE_QT)
        message(FATAL_ERROR "Qt6::OpenGL is required by the experimental Qt Quick viewer")
      else()
        message(WARNING "Qt6::OpenGL not found: experimental Qt Quick viewer is unavailable")
      endif()
    endif()

    target_sources(accloud_cli PRIVATE ${ACCLOUD_APP_QT_SOURCES})
    target_link_libraries(accloud_cli PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Sql)

    find_package(Qt6 QUIET COMPONENTS LinguistTools)
    if(TARGET Qt6::lrelease)
      set(ACCLOUD_I18N_TS_FILES
          ${ACCLOUD_I18N_ROOT}/accloud_en.ts
          ${ACCLOUD_I18N_ROOT}/accloud_fr.ts
      )
      file(GLOB_RECURSE ACCLOUD_I18N_QML_FILES
          ${ACCLOUD_SRC_ROOT}/ui/qml/*.qml
      )
      qt_add_translations(accloud_cli
          TS_FILES ${ACCLOUD_I18N_TS_FILES}
          SOURCES
              ${ACCLOUD_SRC_ROOT}/app/main.cpp
              ${ACCLOUD_SRC_ROOT}/app/AppI18nBridge.cpp
              ${ACCLOUD_SRC_ROOT}/app/CloudBridge.cpp
              ${ACCLOUD_SRC_ROOT}/app/SessionImportBridge.cpp
              ${ACCLOUD_I18N_QML_FILES}
          RESOURCE_PREFIX "/i18n"
      )
    else()
      message(WARNING "Qt6 LinguistTools not found: skipping .ts/.qm generation")
    endif()
  else()
    if(ACCLOUD_REQUIRE_QT)
      message(FATAL_ERROR
          "Required Qt6 desktop components are unavailable: Quick, QuickControls2, Network, Sql and Gui")
    endif()
    message(WARNING "Qt6 not found: accloud_cli will build in headless skeleton mode")
  endif()

  if(ACCLOUD_ENABLE_QML_TESTS)
    find_package(Qt6 QUIET COMPONENTS QuickTest)
    if(TARGET Qt6::QuickTest)
      add_executable(accloud_qml_tests
          ${ACCLOUD_TESTS_ROOT}/ui/tst_ui_main.cpp
          ${ACCLOUD_SRC_ROOT}/app/CloudFilesModel.cpp
          ${ACCLOUD_SRC_ROOT}/app/CloudFilesModel.h
          ${ACCLOUD_SRC_ROOT}/app/LogTailModel.cpp
          ${ACCLOUD_SRC_ROOT}/app/LogTailModel.h
          ${ACCLOUD_SRC_ROOT}/app/MqttTailModel.cpp
          ${ACCLOUD_SRC_ROOT}/app/MqttTailModel.h
          ${ACCLOUD_SRC_ROOT}/app/PrintersModel.cpp
          ${ACCLOUD_SRC_ROOT}/app/PrintersModel.h
          ${ACCLOUD_SRC_ROOT}/app/PrinterFilesModel.cpp
          ${ACCLOUD_SRC_ROOT}/app/PrinterFilesModel.h
          ${ACCLOUD_SRC_ROOT}/app/RecentJobsModel.cpp
          ${ACCLOUD_SRC_ROOT}/app/RecentJobsModel.h
      )
      target_link_libraries(accloud_qml_tests PRIVATE Qt6::QuickTest Qt6::Quick Qt6::QuickControls2)
      target_include_directories(accloud_qml_tests PRIVATE ${ACCLOUD_SRC_ROOT})
      target_compile_definitions(accloud_qml_tests PRIVATE
          QUICK_TEST_SOURCE_DIR="${ACCLOUD_TESTS_ROOT}/ui/qml"
      )
      add_test(
        NAME accloud_ui_qml
        COMMAND accloud_qml_tests -platform offscreen
      )
      add_test(
        NAME accloud_ui_qml_upload
        COMMAND accloud_qml_tests -platform offscreen -input ${ACCLOUD_TESTS_ROOT}/ui/qml/tst_cloud_files_upload.qml
      )
      set_tests_properties(accloud_ui_qml accloud_ui_qml_upload PROPERTIES
        LABELS "qt;qml;ui"
      )
    else()
      if(ACCLOUD_REQUIRE_QT)
        message(FATAL_ERROR "Qt6::QuickTest is required by the selected validation profile")
      endif()
      message(WARNING "Qt6 QuickTest not found: skipping QML UI tests")
    endif()
  endif()
endif()

enable_testing()
add_test(NAME accloud_smoke COMMAND accloud_cli --smoke)
set_tests_properties(accloud_smoke PROPERTIES LABELS "core")

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME accloud_ui_migration_check
    COMMAND ${Python3_EXECUTABLE} ${ACCLOUD_TOOLS_ROOT}/check_ui_migration.py
  )
  add_test(
    NAME accloud_documentation_contract
    COMMAND ${Python3_EXECUTABLE} ${ACCLOUD_TOOLS_ROOT}/check_documentation_contract.py
            --repo-root ${ACCLOUD_REPO_ROOT}
  )
  add_test(
    NAME accloud_cloud_api_architecture
    COMMAND ${Python3_EXECUTABLE} ${ACCLOUD_TOOLS_ROOT}/check_cloud_api_architecture.py
            --repo-root ${ACCLOUD_REPO_ROOT}
  )
  add_test(
    NAME accloud_cloud_bridge_architecture
    COMMAND ${Python3_EXECUTABLE} ${ACCLOUD_TOOLS_ROOT}/check_cloud_bridge_architecture.py
            --repo-root ${ACCLOUD_REPO_ROOT}
  )
  add_test(
    NAME accloud_experimental_viewer_architecture
    COMMAND ${Python3_EXECUTABLE} ${ACCLOUD_TOOLS_ROOT}/check_experimental_viewer_architecture.py
            --repo-root ${ACCLOUD_REPO_ROOT}
  )
  add_test(
    NAME accloud_local_cache_architecture
    COMMAND ${Python3_EXECUTABLE} ${ACCLOUD_TOOLS_ROOT}/check_local_cache_architecture.py
            --repo-root ${ACCLOUD_REPO_ROOT}
  )
  add_test(
    NAME accloud_mqtt_bridge_architecture
    COMMAND ${Python3_EXECUTABLE} ${ACCLOUD_TOOLS_ROOT}/check_mqtt_bridge_architecture.py
            --repo-root ${ACCLOUD_REPO_ROOT}
  )
  set_tests_properties(accloud_ui_migration_check PROPERTIES LABELS "static;ui")
  set_tests_properties(accloud_documentation_contract PROPERTIES LABELS "static;documentation")
  set_tests_properties(accloud_cloud_api_architecture PROPERTIES LABELS "static;cloud;architecture")
  set_tests_properties(accloud_cloud_bridge_architecture PROPERTIES LABELS "static;cloud;architecture")
  set_tests_properties(accloud_experimental_viewer_architecture PROPERTIES
    LABELS "static;architecture;experimental;viewer")
  set_tests_properties(accloud_local_cache_architecture PROPERTIES
    LABELS "static;architecture;cache;sql")
  set_tests_properties(accloud_mqtt_bridge_architecture PROPERTIES
    LABELS "static;architecture;mqtt;qt")
else()
  message(WARNING "Python3 interpreter not found: skipping Python documentation and UI guard tests")
endif()

if(ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER)
  add_executable(accloud_experimental_viewer_scaffold_tests
      ${ACCLOUD_TESTS_ROOT}/photons/test_experimental_viewer_scaffold.cpp)
  target_include_directories(accloud_experimental_viewer_scaffold_tests PRIVATE ${ACCLOUD_SRC_ROOT})
  target_link_libraries(accloud_experimental_viewer_scaffold_tests PRIVATE
      accloud_domain
      accloud_experimental_viewer)
  add_test(NAME accloud_experimental_viewer_scaffold
           COMMAND accloud_experimental_viewer_scaffold_tests)

  add_executable(accloud_pw0_decode_tests
      ${ACCLOUD_TESTS_ROOT}/photons/test_decode_pw0.cpp)
  target_include_directories(accloud_pw0_decode_tests PRIVATE ${ACCLOUD_SRC_ROOT})
  target_link_libraries(accloud_pw0_decode_tests PRIVATE
      accloud_domain
      accloud_experimental_viewer)
  add_test(NAME accloud_pw0_decode COMMAND accloud_pw0_decode_tests)

  add_executable(accloud_pwsz_reader_tests
      ${ACCLOUD_TESTS_ROOT}/photons/test_driver_pwsz.cpp)
  target_include_directories(accloud_pwsz_reader_tests PRIVATE ${ACCLOUD_SRC_ROOT})
  target_link_libraries(accloud_pwsz_reader_tests PRIVATE
      accloud_domain
      accloud_experimental_viewer
      ZLIB::ZLIB)
  add_test(NAME accloud_pwsz_reader COMMAND accloud_pwsz_reader_tests)

  add_executable(accloud_layer_stack_mesher_tests
      ${ACCLOUD_TESTS_ROOT}/photons/test_goldens.cpp)
  target_include_directories(accloud_layer_stack_mesher_tests PRIVATE ${ACCLOUD_SRC_ROOT})
  target_link_libraries(accloud_layer_stack_mesher_tests PRIVATE
      accloud_domain
      accloud_experimental_viewer)
  add_test(NAME accloud_layer_stack_mesher COMMAND accloud_layer_stack_mesher_tests)

  add_executable(accloud_render_pipeline_tests
      ${ACCLOUD_TESTS_ROOT}/photons/test_render_pipeline.cpp)
  target_include_directories(accloud_render_pipeline_tests PRIVATE ${ACCLOUD_SRC_ROOT})
  target_link_libraries(accloud_render_pipeline_tests PRIVATE
      accloud_domain
      accloud_experimental_viewer)
  add_test(NAME accloud_render_pipeline COMMAND accloud_render_pipeline_tests)

  add_executable(accloud_viewer_controls_tests
      ${ACCLOUD_TESTS_ROOT}/photons/test_viewer_controls.cpp)
  target_include_directories(accloud_viewer_controls_tests PRIVATE ${ACCLOUD_SRC_ROOT})
  target_link_libraries(accloud_viewer_controls_tests PRIVATE
      accloud_domain
      accloud_experimental_viewer)
  add_test(NAME accloud_viewer_controls COMMAND accloud_viewer_controls_tests)

  add_executable(accloud_render3d_worker_benchmark
      ${ACCLOUD_TOOLS_ROOT}/render3d_worker_benchmark.cpp)
  target_include_directories(accloud_render3d_worker_benchmark PRIVATE ${ACCLOUD_SRC_ROOT})
  target_link_libraries(accloud_render3d_worker_benchmark PRIVATE
      accloud_domain
      accloud_experimental_viewer
      ZLIB::ZLIB)
  add_test(NAME accloud_render3d_worker_benchmark_selftest
           COMMAND accloud_render3d_worker_benchmark --self-test)

  set_tests_properties(
      accloud_experimental_viewer_scaffold
      accloud_pw0_decode
      accloud_pwsz_reader
      accloud_layer_stack_mesher
      accloud_render_pipeline
      accloud_viewer_controls
      accloud_render3d_worker_benchmark_selftest
      PROPERTIES LABELS "core;experimental;viewer")
  set_tests_properties(accloud_render3d_worker_benchmark_selftest PROPERTIES
      LABELS "core;experimental;viewer;benchmark")

  if(ACCLOUD_ENABLE_QT AND TARGET Qt6::Gui AND TARGET Qt6::OpenGL)
    add_executable(accloud_render3d_shader_tests
        ${ACCLOUD_TESTS_ROOT}/photons/test_render3d_shader_qt.cpp)
    target_include_directories(accloud_render3d_shader_tests PRIVATE ${ACCLOUD_SRC_ROOT})
    target_link_libraries(accloud_render3d_shader_tests PRIVATE Qt6::Gui Qt6::OpenGL)
    add_test(NAME accloud_render3d_shader_compile COMMAND accloud_render3d_shader_tests)
    set_tests_properties(accloud_render3d_shader_compile PROPERTIES
        LABELS "qt;opengl;experimental;viewer"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endif()
endif()

add_executable(accloud_har_tests
    ${ACCLOUD_TESTS_ROOT}/cloud/test_har_import.cpp
)
target_link_libraries(accloud_har_tests PRIVATE accloud_domain accloud_infra)
if(OpenSSL_FOUND)
  target_link_libraries(accloud_har_tests PRIVATE OpenSSL::Crypto)
endif()
if(ACCLOUD_CRYPT_LIBRARY)
  target_link_libraries(accloud_har_tests PRIVATE ${ACCLOUD_CRYPT_LIBRARY})
endif()
add_test(NAME accloud_har_import COMMAND accloud_har_tests)
set_tests_properties(accloud_har_import PROPERTIES LABELS "core;cloud")

add_executable(accloud_direct_print_lifecycle_tests
    ${ACCLOUD_TESTS_ROOT}/printing/test_direct_print_lifecycle.cpp
)
target_link_libraries(accloud_direct_print_lifecycle_tests PRIVATE accloud_domain accloud_infra)
add_test(NAME accloud_direct_print_lifecycle COMMAND accloud_direct_print_lifecycle_tests)
set_tests_properties(accloud_direct_print_lifecycle PROPERTIES LABELS "core;printing")

add_executable(accloud_cloud_files_delete_tests
    ${ACCLOUD_TESTS_ROOT}/cloud/test_delete_cloud_files.cpp
)
target_link_libraries(accloud_cloud_files_delete_tests PRIVATE accloud_domain accloud_infra)
add_test(NAME accloud_cloud_files_delete COMMAND accloud_cloud_files_delete_tests)
set_tests_properties(accloud_cloud_files_delete PROPERTIES LABELS "core;cloud")

add_executable(accloud_cloud_api_support_tests
    ${ACCLOUD_TESTS_ROOT}/cloud/test_api_support.cpp
)
target_link_libraries(accloud_cloud_api_support_tests PRIVATE accloud_domain accloud_infra)
add_test(NAME accloud_cloud_api_support COMMAND accloud_cloud_api_support_tests)
set_tests_properties(accloud_cloud_api_support PROPERTIES LABELS "core;cloud;architecture")

if(ACCLOUD_ENABLE_QT)
  if(TARGET Qt6::Sql)
    add_executable(accloud_cloud_core_tests
        ${ACCLOUD_TESTS_ROOT}/cloud/test_cloud_core_regressions.cpp
        ${ACCLOUD_LOCAL_CACHE_SOURCES}
        ${ACCLOUD_SRC_ROOT}/app/CloudFilesWorkflowBridge.cpp
        ${ACCLOUD_SRC_ROOT}/app/CloudFilesWorkflowBridge.h
        ${ACCLOUD_SRC_ROOT}/app/PrintWorkflowBridge.cpp
        ${ACCLOUD_SRC_ROOT}/app/PrintWorkflowBridge.h
        ${ACCLOUD_SRC_ROOT}/app/printing/PendingDirectPrintStore.cpp
        ${ACCLOUD_SRC_ROOT}/app/printing/PendingDirectPrintStore.h
        ${ACCLOUD_SRC_ROOT}/app/printing/PrinterFileCompatibility.cpp
        ${ACCLOUD_SRC_ROOT}/app/printing/PrinterFileCompatibility.h
        ${ACCLOUD_SRC_ROOT}/app/usecases/printing/PrepareRemotePrintUseCase.cpp
        ${ACCLOUD_SRC_ROOT}/app/usecases/printing/PrepareRemotePrintUseCase.h
    )
    target_link_libraries(accloud_cloud_core_tests PRIVATE accloud_domain accloud_infra Qt6::Core Qt6::Sql)
    add_test(NAME accloud_cloud_core_regressions COMMAND accloud_cloud_core_tests)
    set_tests_properties(accloud_cloud_core_regressions PROPERTIES LABELS "qt;sql;integration")
  else()
    message(WARNING "Qt6::Sql unavailable: skipping accloud_cloud_core_regressions test")
  endif()
endif()


add_executable(accloud_thumbnail_candidate_tests
    ${ACCLOUD_TESTS_ROOT}/cloud/test_thumbnail_candidates.cpp
    ${ACCLOUD_SRC_ROOT}/infra/cloud/api/ThumbnailCandidateBuilder.cpp
)
target_include_directories(accloud_thumbnail_candidate_tests PRIVATE ${ACCLOUD_SRC_ROOT})
target_link_libraries(accloud_thumbnail_candidate_tests PRIVATE accloud_domain)
add_test(NAME accloud_thumbnail_candidates COMMAND accloud_thumbnail_candidate_tests)
set_tests_properties(accloud_thumbnail_candidates PROPERTIES LABELS "core;cloud")

if(ACCLOUD_ENABLE_QT AND TARGET Qt6::Core)
  add_executable(accloud_thumbnail_cache_policy_tests
      ${ACCLOUD_TESTS_ROOT}/cloud/test_thumbnail_cache_policy.cpp
      ${ACCLOUD_SRC_ROOT}/app/ThumbnailCachePolicy.cpp
  )
  target_include_directories(accloud_thumbnail_cache_policy_tests PRIVATE ${ACCLOUD_SRC_ROOT})
  target_link_libraries(accloud_thumbnail_cache_policy_tests PRIVATE accloud_domain Qt6::Core)
  add_test(NAME accloud_thumbnail_cache_policy COMMAND accloud_thumbnail_cache_policy_tests)
  set_tests_properties(accloud_thumbnail_cache_policy PROPERTIES LABELS "qt;cloud")
endif()

if(ACCLOUD_ENABLE_QT AND TARGET Qt6::Gui)
  add_executable(accloud_thumbnail_validation_tests
      ${ACCLOUD_TESTS_ROOT}/cloud/test_thumbnail_validation.cpp
      ${ACCLOUD_SRC_ROOT}/infra/cloud/thumbnail/ThumbnailValidation.cpp
  )
  target_include_directories(accloud_thumbnail_validation_tests PRIVATE ${ACCLOUD_SRC_ROOT})
  target_compile_definitions(accloud_thumbnail_validation_tests PRIVATE ACCLOUD_WITH_QT=1)
  target_link_libraries(accloud_thumbnail_validation_tests PRIVATE accloud_domain Qt6::Core Qt6::Gui)
  add_test(NAME accloud_thumbnail_validation COMMAND accloud_thumbnail_validation_tests)
  set_tests_properties(accloud_thumbnail_validation PROPERTIES LABELS "qt;gui;integration")
endif()

add_executable(accloud_pwsz_preview_archive_tests
    ${ACCLOUD_TESTS_ROOT}/cloud/test_pwsz_preview_archive.cpp
    ${ACCLOUD_SRC_ROOT}/infra/cloud/archive/PwszPreviewArchive.cpp
)
target_include_directories(accloud_pwsz_preview_archive_tests PRIVATE ${ACCLOUD_SRC_ROOT})
target_link_libraries(accloud_pwsz_preview_archive_tests PRIVATE accloud_domain)
add_test(NAME accloud_pwsz_preview_archive COMMAND accloud_pwsz_preview_archive_tests)
set_tests_properties(accloud_pwsz_preview_archive PROPERTIES LABELS "core;cloud")

add_executable(accloud_pwsz_cloud_preview_update_order_tests
    ${ACCLOUD_TESTS_ROOT}/cloud/test_pwsz_cloud_preview_update_order.cpp
    ${ACCLOUD_SRC_ROOT}/app/usecases/cloud/CloudPwszReplacementWorkflow.cpp
)
target_include_directories(accloud_pwsz_cloud_preview_update_order_tests PRIVATE ${ACCLOUD_SRC_ROOT})
target_link_libraries(accloud_pwsz_cloud_preview_update_order_tests PRIVATE accloud_domain)
add_test(NAME accloud_pwsz_cloud_preview_update_order COMMAND accloud_pwsz_cloud_preview_update_order_tests)
set_tests_properties(accloud_pwsz_cloud_preview_update_order PROPERTIES LABELS "core;cloud")

add_executable(accloud_security_tests
    ${ACCLOUD_TESTS_ROOT}/cloud/test_redactor.cpp
)
target_link_libraries(accloud_security_tests PRIVATE accloud_domain accloud_infra)
if(OpenSSL_FOUND)
  target_link_libraries(accloud_security_tests PRIVATE OpenSSL::Crypto)
endif()
if(ACCLOUD_CRYPT_LIBRARY)
  target_link_libraries(accloud_security_tests PRIVATE ${ACCLOUD_CRYPT_LIBRARY})
endif()
add_test(NAME accloud_security_redaction COMMAND accloud_security_tests)
set_tests_properties(accloud_security_redaction PROPERTIES LABELS "core;security")

add_executable(accloud_jsonl_logger_tests
    ${ACCLOUD_TESTS_ROOT}/cloud/test_jsonl_logger.cpp
)
target_link_libraries(accloud_jsonl_logger_tests PRIVATE accloud_domain accloud_infra)
add_test(NAME accloud_jsonl_logger_timestamp COMMAND accloud_jsonl_logger_tests)
set_tests_properties(accloud_jsonl_logger_timestamp PROPERTIES LABELS "core;logging")

if(ACCLOUD_DEBUG)
  add_executable(accloud_raw_traffic_log_tests
      ${ACCLOUD_TESTS_ROOT}/cloud/test_raw_traffic_logger.cpp
  )
  target_link_libraries(accloud_raw_traffic_log_tests PRIVATE accloud_domain accloud_infra)
  add_test(NAME accloud_dev_raw_traffic_log COMMAND accloud_raw_traffic_log_tests)
  set_tests_properties(accloud_dev_raw_traffic_log PROPERTIES LABELS "core;debug;security")
endif()

if(ACCLOUD_ENABLE_QT)
  if(TARGET Qt6::Core)
    add_executable(accloud_ui_model_tests
        ${ACCLOUD_TESTS_ROOT}/ui/test_models.cpp
        ${ACCLOUD_SRC_ROOT}/app/CloudFilesModel.cpp
        ${ACCLOUD_SRC_ROOT}/app/MqttTailModel.cpp
        ${ACCLOUD_SRC_ROOT}/app/PrintersModel.cpp
        ${ACCLOUD_SRC_ROOT}/app/PrinterFilesModel.cpp
        ${ACCLOUD_SRC_ROOT}/app/RecentJobsModel.cpp
    )
    target_link_libraries(accloud_ui_model_tests PRIVATE accloud_domain Qt6::Core)
    add_test(NAME accloud_ui_models COMMAND accloud_ui_model_tests)
    set_tests_properties(accloud_ui_models PROPERTIES LABELS "qt;ui")

    add_executable(accloud_log_tests
        ${ACCLOUD_TESTS_ROOT}/cloud/test_log_bridge.cpp
        ${ACCLOUD_SRC_ROOT}/app/LogBridge.cpp
        ${ACCLOUD_SRC_ROOT}/app/LogTailModel.cpp
    )
    target_link_libraries(accloud_log_tests PRIVATE accloud_domain accloud_infra Qt6::Core)
    add_test(NAME accloud_log_flow COMMAND accloud_log_tests)
    set_tests_properties(accloud_log_flow PROPERTIES LABELS "qt;logging;integration")
  else()
    message(WARNING "Qt6::Core unavailable: skipping Qt model and log regression tests")
  endif()
endif()

add_executable(accloud_mqtt_tests
    ${ACCLOUD_TESTS_ROOT}/mqtt/test_mqtt_flow.cpp
)
target_link_libraries(accloud_mqtt_tests PRIVATE accloud_domain accloud_infra)
if(OpenSSL_FOUND)
  target_link_libraries(accloud_mqtt_tests PRIVATE OpenSSL::Crypto)
  target_compile_definitions(accloud_mqtt_tests PRIVATE ACCLOUD_WITH_OPENSSL=1)
endif()
if(ACCLOUD_CRYPT_LIBRARY)
  target_link_libraries(accloud_mqtt_tests PRIVATE ${ACCLOUD_CRYPT_LIBRARY})
endif()
add_test(NAME accloud_mqtt_flow COMMAND accloud_mqtt_tests)
set_tests_properties(accloud_mqtt_flow PROPERTIES LABELS "core;mqtt")

if(ACCLOUD_ENABLE_LIVE_TESTS)
  if(ACCLOUD_ENABLE_QT AND TARGET Qt6::Network AND TARGET Qt6::Mqtt)
    add_executable(accloud_mqtt_live_check
        ${ACCLOUD_TESTS_ROOT}/mqtt/test_mqtt_live_broker.cpp
    )
    target_link_libraries(accloud_mqtt_live_check PRIVATE
        accloud_domain
        accloud_infra
        Qt6::Network
        Qt6::Mqtt
    )
    target_compile_definitions(accloud_mqtt_live_check PRIVATE
        ACCLOUD_WITH_QT=1
        ACCLOUD_WITH_MQTT=1
    )
    if(OpenSSL_FOUND)
      target_link_libraries(accloud_mqtt_live_check PRIVATE OpenSSL::Crypto)
    endif()
    if(ACCLOUD_CRYPT_LIBRARY)
      target_link_libraries(accloud_mqtt_live_check PRIVATE ${ACCLOUD_CRYPT_LIBRARY})
    endif()
    add_test(NAME accloud_mqtt_live_broker COMMAND accloud_mqtt_live_check)
    set_tests_properties(accloud_mqtt_live_broker PROPERTIES
      LABELS "qt;mqtt;live"
      SKIP_RETURN_CODE 77
    )
  else()
    message(WARNING
        "Qt6::Network and Qt6::Mqtt unavailable: skipping accloud_mqtt_live_broker test")
  endif()
endif()
