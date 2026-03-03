cmake_minimum_required(VERSION 3.24)

project(accloud VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ACCLOUD_ENABLE_QT "Enable Qt6/QML frontend target" ON)

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
target_link_libraries(accloud_infra PRIVATE accloud_domain)

add_executable(accloud_cli
    app/main.cpp
    app/App.cpp
)
target_link_libraries(accloud_cli PRIVATE accloud_domain accloud_infra)

if(ACCLOUD_ENABLE_QT)
  find_package(Qt6 QUIET COMPONENTS Quick QuickControls2)
  if(Qt6_FOUND)
    target_compile_definitions(accloud_cli PRIVATE ACCLOUD_WITH_QT=1)
    target_sources(accloud_cli PRIVATE app/resources.qrc)
    target_link_libraries(accloud_cli PRIVATE Qt6::Quick Qt6::QuickControls2)
  else()
    message(WARNING "Qt6 not found: accloud_cli will build in headless skeleton mode")
  endif()
endif()

enable_testing()
add_test(NAME accloud_smoke COMMAND accloud_cli --smoke)
