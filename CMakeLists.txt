cmake_minimum_required(VERSION 3.10)

# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Remove per-configuration suffix from output directory added by VSCode
# https://stackoverflow.com/questions/47175912/using-cmake-how-to-stop-the-debug-and-release-subdirectories
# https://cmake.org/cmake/help/v3.4/prop_tgt/RUNTIME_OUTPUT_DIRECTORY.html#prop_tgt:RUNTIME_OUTPUT_DIRECTORY
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_SOURCE_DIR}/bin>)

file(READ "release/version.txt" APP_VER_FULL)

project(beam-inspector VERSION ${APP_VER_FULL} LANGUAGES C CXX)

string(TIMESTAMP BUILD_DATE "%Y-%m-%d")
string(TIMESTAMP APP_VER_YEAR "%Y")

string(REPLACE "." ";" APP_VER_LIST ${APP_VER_FULL})
list(GET APP_VER_LIST 0 APP_VER_MAJOR)
list(GET APP_VER_LIST 1 APP_VER_MINOR)
list(GET APP_VER_LIST 2 APP_VER_PATCH)

configure_file(
    release/version.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(WITH_IDS "Add support for cameras from IDS (SDK must be installed)" ON)
option(CHECK_UPDATES_WITH_CURL "Use external curl program for updates checking" ON)

#add_compile_options(
#    -O3
#    -ffast-math
#    -funsafe-math-optimizations
#    -msse4.2
#)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets PrintSupport)
if(NOT CHECK_UPDATES_WITH_CURL)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Network)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)
endif()

set(PROJECT_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    src/app.rc
    src/app.qrc
    src/main.cpp
    src/app/AppSettings.h src/app/AppSettings.cpp
    src/app/HelpSystem.h src/app/HelpSystem.cpp
    src/app/ImageUtils.h src/app/ImageUtils.cpp
    src/cameras/Camera.h src/cameras/Camera.cpp
    src/cameras/HardConfigPanel.h src/cameras/HardConfigPanel.cpp
    src/cameras/CameraTypes.h src/cameras/CameraTypes.cpp
    src/cameras/CameraWorker.h
    src/cameras/IdsCamera.h src/cameras/IdsCamera.cpp
    src/cameras/IdsCameraConfig.h src/cameras/IdsCameraConfig.cpp
    src/cameras/IdsHardConfig.h src/cameras/IdsHardConfig.cpp
    src/cameras/IdsLib.h src/cameras/IdsLib.cpp
    src/cameras/MeasureSaver.h src/cameras/MeasureSaver.cpp
    src/cameras/StillImageCamera.h src/cameras/StillImageCamera.cpp
    src/cameras/VirtualDemoCamera.h src/cameras/VirtualDemoCamera.cpp
    src/cameras/VirtualImageCamera.h src/cameras/VirtualImageCamera.cpp
    src/cameras/WelcomeCamera.h src/cameras/WelcomeCamera.cpp
    src/plot/BeamGraph.h src/plot/BeamGraph.cpp
    src/plot/BeamPlotItem.h src/plot/BeamPlotItem.cpp
    src/plot/CrosshairOverlay.h src/plot/CrosshairOverlay.cpp
    src/plot/Heatmap.h src/plot/Heatmap.cpp
    src/plot/PlotExport.h src/plot/PlotExport.cpp
    src/plot/RoiRectGraph.h src/plot/RoiRectGraph.cpp
    src/widgets/DataTable.h src/widgets/DataTable.cpp
    src/widgets/FileSelector.h src/widgets/FileSelector.cpp
    src/widgets/Plot.h src/widgets/Plot.cpp
    src/widgets/PlotIntf.h src/widgets/PlotIntf.cpp
    src/widgets/PlotHelpers.h src/widgets/PlotHelpers.cpp
    src/widgets/ProfilesView.h src/widgets/ProfilesView.cpp
    src/widgets/StabilityIntf.h src/widgets/StabilityIntf.cpp
    src/widgets/StabilityView.h src/widgets/StabilityView.cpp
    src/widgets/TableIntf.h src/widgets/TableIntf.cpp
    src/windows/PlotWindow.h src/windows/PlotWindow.cpp
)

add_subdirectory(libs/qcustomplot)
add_subdirectory(libs/beam_render)
add_subdirectory(libs/beam_calc)
add_subdirectory(libs/orion)

set(LIB_RESOURCES
    libs/orion/resources.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${LIB_RESOURCES}
    )
else()
    add_executable(${PROJECT_NAME}
        ${PROJECT_SOURCES}
        ${LIB_RESOURCES}
    )
endif()


target_link_libraries(${PROJECT_NAME} PRIVATE
    version
    qcustomplot
    cgn_beam_render
    cgn_beam_calc
    orion
    Qt::Widgets
)
if(NOT CHECK_UPDATES_WITH_CURL)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt::Network
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER org.orion-project.beam-inspector
    MACOSX_BUNDLE_BUNDLE_VERSION ${APP_VER_FULL}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    BUILD_DATE="${BUILD_DATE}"
    APP_VER_MAJOR=${APP_VER_MAJOR}
    APP_VER_MINOR=${APP_VER_MINOR}
    APP_VER_PATCH=${APP_VER_PATCH}
    APP_VER_YEAR=${APP_VER_YEAR}
    QT_USE_QSTRINGBUILDER
)

if(WITH_IDS)
    if (NOT TARGET ids_peak_comfort_c)
        find_package(ids_peak_comfort_c REQUIRED)
    endif()

    add_definitions(-DWITH_IDS)

    get_target_property(IDS_PEAK_INCLUDE_DIR
        ids_peak_comfort_c::ids_peak_comfort_c
        INTERFACE_INCLUDE_DIRECTORIES
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${IDS_PEAK_INCLUDE_DIR}"
    )
endif()

if(CHECK_UPDATES_WITH_CURL)
    add_definitions(-DCHECK_UPDATES_WITH_CURL)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()
