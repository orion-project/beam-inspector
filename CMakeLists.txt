cmake_minimum_required(VERSION 3.5)

file(READ "release/version.txt" APP_VER_FULL)

project(cignus VERSION ${APP_VER_FULL} LANGUAGES C CXX)

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

#add_compile_options(
#    -O3
#    -ffast-math
#    -funsafe-math-optimizations
#    -msse4.2
#)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets PrintSupport)

set(PROJECT_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    src/app.rc
    src/app.qrc
    src/main.cpp
    src/app/HelpSystem.cpp
    src/cameras/Camera.cpp
    src/cameras/CameraTypes.cpp
    src/cameras/IdsComfortCamera.cpp
    src/cameras/MeasureSaver.cpp
    src/cameras/StillImageCamera.cpp
    src/cameras/VirtualDemoCamera.cpp
    src/cameras/WelcomeCamera.cpp
    src/plot/BeamGraph.cpp
    src/plot/PlotExport.cpp
    src/widgets/FileSelector.cpp
    src/widgets/Plot.cpp
    src/widgets/PlotIntf.cpp
    src/widgets/TableIntf.cpp
    src/windows/PlotWindow.cpp
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

if (NOT TARGET ids_peak_comfort_c)
    find_package(ids_peak_comfort_c REQUIRED)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    ids_peak_comfort_c::ids_peak_comfort_c
    qcustomplot
    cgn_beam_render
    cgn_beam_calc
    orion
    Qt::Widgets
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER org.orion-project.cignus
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

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()
