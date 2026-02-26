cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  if(APPLE)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.deps/lib/obs-frontend-api.dylib")
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.deps/include/obs")
  else()
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
  endif()
endif()

if(ENABLE_QT)
  if(APPLE)
    set(QT_FRAMEWORK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-deps-qt6-2025-07-11-universal/lib")
    target_include_directories(
      ${CMAKE_PROJECT_NAME}
      PRIVATE
        "${QT_FRAMEWORK_DIR}/QtCore.framework/Headers"
        "${QT_FRAMEWORK_DIR}/QtGui.framework/Headers"
        "${QT_FRAMEWORK_DIR}/QtWidgets.framework/Headers"
        "${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-deps-qt6-2025-07-11-universal/mkspecs/macx-clang"
        "${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-deps-qt6-2025-07-11-universal/include"
    )
    target_link_options(
      ${CMAKE_PROJECT_NAME}
      PRIVATE "-F${QT_FRAMEWORK_DIR}" "-iframework${QT_FRAMEWORK_DIR}"
    )
    target_compile_options(
      ${CMAKE_PROJECT_NAME}
      PRIVATE "-F${QT_FRAMEWORK_DIR}" "-iframework${QT_FRAMEWORK_DIR}"
    )
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "-framework QtCore" "-framework QtGui" "-framework QtWidgets")
  else()
    find_package(Qt6 COMPONENTS Widgets Core)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  endif()
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/bm-hotkey-registry.cpp
    src/bm-hotkey-registry.hpp
    src/bm-marker-controller.cpp
    src/bm-marker-controller.hpp
    src/bm-marker-data.hpp
    src/bm-marker-export-sink.hpp
    src/bm-marker-dialog.cpp
    src/bm-marker-dialog.hpp
    src/bm-fcpxml-writer.cpp
    src/bm-fcpxml-writer.hpp
    src/bm-mp4-mov-embed-engine.cpp
    src/bm-mp4-mov-embed-engine.hpp
    src/bm-recovery-queue.cpp
    src/bm-recovery-queue.hpp
    src/bm-recording-session-tracker.cpp
    src/bm-recording-session-tracker.hpp
    src/bm-xmp-sidecar-writer.cpp
    src/bm-xmp-sidecar-writer.hpp
    src/plugin-main.cpp
    src/bm-colors.cpp
    src/bm-colors.hpp
    src/bm-models.cpp
    src/bm-models.hpp
    src/bm-premiere-xmp-sink.hpp
    src/bm-settings-dialog.cpp
    src/bm-settings-dialog.hpp
    src/bm-scope-store.cpp
    src/bm-scope-store.hpp
    src/bm-template-editor-dialog.cpp
    src/bm-template-editor-dialog.hpp
)

if(ENABLE_QT)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE BETTER_MARKERS_QT=1)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
