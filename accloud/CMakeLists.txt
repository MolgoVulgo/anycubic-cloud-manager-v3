cmake_minimum_required(VERSION 3.24)

project(accloud VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(ACCLOUD_ENABLE_QT "Enable Qt6/QML frontend target" ON)
option(ACCLOUD_ENABLE_QML_TESTS "Enable Qt Quick Test UI tests" ON)
option(ACCLOUD_DEBUG "Enable global debug features (UI tools + debug payloads + debug traces)" OFF)

find_package(nlohmann_json CONFIG REQUIRED)

add_library(accloud_domain INTERFACE)
target_include_directories(accloud_domain INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

set(ACCLOUD_INFRA_SOURCES
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
    infra/photons/drivers/pwsz/PwszLinesDriver.cpp
    infra/photons/drivers/pwsz/PwszSceneDriver.cpp
    infra/cloud/CloudClient.cpp
    infra/cloud/HarImporter.cpp
    infra/cloud/SignHeaders.cpp
    infra/cloud/core/HttpClient.cpp
    infra/cloud/core/WorkbenchSigner.cpp
    infra/cloud/core/WorkbenchRequestBuilder.cpp
    infra/cloud/core/EndpointRegistry.cpp
    infra/cloud/core/ResponseEnvelopeParser.cpp
    infra/cache/RamWindowCache.cpp
    infra/cache/DiskLruCache.cpp
    infra/cache/CacheIndex.cpp
    infra/logging/JsonlLogger.cpp
    infra/logging/Redactor.cpp
    infra/logging/Rotator.cpp
    infra/jobs/JobManager.cpp
    infra/jobs/Jobs_OpenPhotons.cpp
    infra/jobs/Jobs_BuildMasks.cpp
    infra/jobs/Jobs_Build3D.cpp
    infra/jobs/Jobs_Cloud.cpp
    infra/jobs/Jobs_ExportReport.cpp
    render3d/gl/Renderer.cpp
    render3d/gl/UploadQueue.cpp
    render3d/gl/MeshlessVolume.cpp
    render3d/qtquick/QmlGlItem.cpp
)

add_library(accloud_infra OBJECT ${ACCLOUD_INFRA_SOURCES})
target_include_directories(accloud_infra PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(accloud_infra PRIVATE accloud_domain nlohmann_json::nlohmann_json)

add_executable(accloud_cli
    app/main.cpp
    app/App.cpp
)
set_source_files_properties(app/main.cpp PROPERTIES SKIP_AUTOMOC ON)
target_link_libraries(accloud_cli PRIVATE accloud_domain accloud_infra)

if(ACCLOUD_DEBUG)
  target_compile_definitions(accloud_infra PRIVATE ACCLOUD_DEBUG=1)
  target_compile_definitions(accloud_cli PRIVATE ACCLOUD_DEBUG=1)
endif()

if(ACCLOUD_ENABLE_QT)
  find_package(Qt6 QUIET COMPONENTS Quick QuickControls2 Network Sql)
  if(Qt6_FOUND)
    target_compile_definitions(accloud_infra PRIVATE ACCLOUD_WITH_QT=1)
    target_link_libraries(accloud_infra PRIVATE Qt6::Network)
    target_compile_definitions(accloud_cli PRIVATE ACCLOUD_WITH_QT=1)
    set(ACCLOUD_APP_QT_SOURCES
        app/resources.qrc
        app/AppI18nBridge.cpp
        app/AppI18nBridge.h
        app/CloudBridge.cpp
        app/CloudBridge.h
        app/LocalCacheStore.cpp
        app/LocalCacheStore.h
        app/UiSettingsBridge.cpp
        app/UiSettingsBridge.h
        app/SessionImportBridge.cpp
        app/SessionImportBridge.h
    )

    if(ACCLOUD_DEBUG)
      list(APPEND ACCLOUD_APP_QT_SOURCES
          app/resources_debug.qrc
          app/LogBridge.cpp
          app/LogBridge.h
          app/UiClickTracer.cpp
          app/UiClickTracer.h
      )
    endif()

    target_sources(accloud_cli PRIVATE ${ACCLOUD_APP_QT_SOURCES})
    target_link_libraries(accloud_cli PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Sql)

    find_package(Qt6 QUIET COMPONENTS LinguistTools)
    if(TARGET Qt6::lrelease)
      set(ACCLOUD_I18N_TS_FILES
          ${CMAKE_CURRENT_SOURCE_DIR}/i18n/accloud_en.ts
          ${CMAKE_CURRENT_SOURCE_DIR}/i18n/accloud_fr.ts
      )
      file(GLOB_RECURSE ACCLOUD_I18N_QML_FILES
          ${CMAKE_CURRENT_SOURCE_DIR}/ui/qml/*.qml
      )
      qt_add_translations(accloud_cli
          TS_FILES ${ACCLOUD_I18N_TS_FILES}
          SOURCES
              app/main.cpp
              app/AppI18nBridge.cpp
              app/CloudBridge.cpp
              app/SessionImportBridge.cpp
              ${ACCLOUD_I18N_QML_FILES}
          RESOURCE_PREFIX "/i18n"
      )
    else()
      message(WARNING "Qt6 LinguistTools not found: skipping .ts/.qm generation")
    endif()
  else()
    message(WARNING "Qt6 not found: accloud_cli will build in headless skeleton mode")
  endif()

  if(ACCLOUD_ENABLE_QML_TESTS)
    find_package(Qt6 QUIET COMPONENTS QuickTest)
    if(TARGET Qt6::QuickTest)
      add_executable(accloud_qml_tests tests/ui/tst_ui_main.cpp)
      target_link_libraries(accloud_qml_tests PRIVATE Qt6::QuickTest Qt6::Quick Qt6::QuickControls2)
      target_compile_definitions(accloud_qml_tests PRIVATE
          QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/ui/qml"
      )
      add_test(
        NAME accloud_ui_qml
        COMMAND accloud_qml_tests -platform offscreen
      )
    else()
      message(WARNING "Qt6 QuickTest not found: skipping QML UI tests")
    endif()
  endif()
endif()

enable_testing()
add_test(NAME accloud_smoke COMMAND accloud_cli --smoke)

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME accloud_ui_migration_check
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_ui_migration.py
  )
else()
  message(WARNING "Python3 interpreter not found: skipping UI migration guard test")
endif()

add_executable(accloud_har_tests
    tests/cloud/test_har_import.cpp
)
target_link_libraries(accloud_har_tests PRIVATE accloud_domain accloud_infra)
add_test(NAME accloud_har_import COMMAND accloud_har_tests)
