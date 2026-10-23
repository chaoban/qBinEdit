cmake_minimum_required(VERSION 3.19)

project(qBinEdit
    VERSION 1.0.0
    DESCRIPTION "Firmware binary viewer / editor"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)

option(QBE_BUILD_TESTS "Build console unit tests for parsers and checksums" ON)
option(QBE_BUILD_SHOTS "Build the screenshot generator (developer tool)" OFF)

# ---- core (no GUI) library: formats, checksums ----------------------------
add_library(qbe_core STATIC
    src/core/checksums.cpp   src/core/checksums.h
    src/core/formats.cpp     src/core/formats.h
)
target_include_directories(qbe_core PUBLIC src/core)
target_link_libraries(qbe_core PUBLIC Qt6::Core)

# ---- application -----------------------------------------------------------
set(APP_SOURCES
    src/aboutdialog.cpp      src/aboutdialog.h      src/appinfo.h
    src/appsettings.cpp      src/appsettings.h
    src/document.cpp         src/document.h
    src/hexview.cpp          src/hexview.h
    src/tabpages.cpp         src/tabpages.h
    src/datainspector.cpp    src/datainspector.h
    src/checksumdialog.cpp   src/checksumdialog.h
    src/finddialog.cpp       src/finddialog.h
    src/dialogs.cpp          src/dialogs.h
    src/mainwindow.cpp       src/mainwindow.h
    resources/qbinedit.qrc
)

if(WIN32)
    # Version resource + icon + application manifest
    string(REPLACE "." "," QBE_VERSION_COMMA "${PROJECT_VERSION}.0")
    configure_file(resources/qbinedit.rc.in ${CMAKE_CURRENT_BINARY_DIR}/qbinedit.rc @ONLY)
    list(APPEND APP_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/qbinedit.rc)
endif()

qt_add_executable(qBinEdit WIN32 src/main.cpp ${APP_SOURCES})
target_include_directories(qBinEdit PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR}/resources)
target_compile_definitions(qBinEdit PRIVATE QBE_VERSION="${PROJECT_VERSION}")
target_link_libraries(qBinEdit PRIVATE qbe_core Qt6::Core Qt6::Gui Qt6::Widgets)
if(MSVC)
    # the application manifest is embedded through qbinedit.rc
    target_link_options(qBinEdit PRIVATE "/MANIFEST:NO")
endif()

install(TARGETS qBinEdit RUNTIME DESTINATION .)

# ---- tests -------------------------------------------------------------------
if(QBE_BUILD_TESTS)
    add_executable(qbe_tests tests/test_core.cpp)
    target_link_libraries(qbe_tests PRIVATE qbe_core)
    add_executable(qbe_view_tests tests/test_view.cpp
        src/document.cpp src/document.h src/hexview.cpp src/hexview.h)
    target_include_directories(qbe_view_tests PRIVATE src)
    target_link_libraries(qbe_view_tests PRIVATE qbe_core Qt6::Widgets)
    enable_testing()
    add_test(NAME core COMMAND qbe_tests)
    add_test(NAME view COMMAND qbe_view_tests)
    set_tests_properties(view PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# ---- screenshot generator (developer tool, not shipped) -----------------------
if(QBE_BUILD_SHOTS)
    qt_add_executable(qbe_shots tools/shots.cpp ${APP_SOURCES})
    target_include_directories(qbe_shots PRIVATE src)
    target_compile_definitions(qbe_shots PRIVATE QBE_VERSION="${PROJECT_VERSION}")
    target_link_libraries(qbe_shots PRIVATE qbe_core Qt6::Widgets)
endif()
