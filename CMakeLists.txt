cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME cutefish-terminal)
project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT Core Gui Network Qml Quick Widgets DBus LinguistTools)
find_package(Qt6 REQUIRED ${QT})
find_package(CutefishFramework REQUIRED COMPONENTS Application)

# Where QML modules go, so the terminal widget plugin lands next to FishUI.
find_program(QT_PATHS_EXECUTABLE NAMES qtpaths6 REQUIRED)
execute_process(COMMAND ${QT_PATHS_EXECUTABLE} -query QT_INSTALL_QML
    OUTPUT_VARIABLE INSTALL_QMLDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(INSTALL_QMLDIR)
    message(STATUS "qml directory:" "${INSTALL_QMLDIR}")
else()
    message(FATAL_ERROR "qml directory cannot be detected.")
endif()

set(SRCS
    src/main.cpp
    src/processhelper.h
    src/processhelper.cpp
    src/utils.cpp
    src/fonts.cpp
)

set(RESOURCES
    src/resources.qrc
)

add_subdirectory(qmltermwidget)
add_executable(${PROJECT_NAME} ${SRCS} ${RESOURCES})
target_link_libraries(${PROJECT_NAME}
        Qt6::Core
        Qt6::Qml
        Qt6::Quick
        Qt6::Widgets
        Qt6::DBus
        Cutefish::Application
)

file(GLOB TS_FILES translations/*.ts)
qt6_add_translation(QM_FILES ${TS_FILES})
add_custom_target(translations DEPENDS ${QM_FILES} SOURCES ${TS_FILES})
add_dependencies(${PROJECT_NAME} translations)
install(FILES ${QM_FILES} DESTINATION /usr/share/cutefish-terminal/translations)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION /usr/bin)

install(FILES
    cutefish-terminal.desktop
    DESTINATION /usr/share/applications/
    COMPONENT Runtime
)
