find_package(GTest REQUIRED)
include(GoogleTest)

set(TERMINAL_KB_LAYOUTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../qmltermwidget/lib/kb-layouts")

# The emulator and the session alone: no toolkit view library, so no window can come in.
add_executable(terminal-logic-tests
    logic/main.cpp
    logic/emulation_test.cpp
    logic/session_test.cpp
)
target_link_libraries(terminal-logic-tests PRIVATE terminal-core GTest::gtest)
target_compile_definitions(terminal-logic-tests PRIVATE KB_LAYOUTS_DIR="${TERMINAL_KB_LAYOUTS_DIR}")
gtest_discover_tests(terminal-logic-tests PROPERTIES LABELS logic)

# The windows and tabs on the headless platform.
set(TERMINAL_WINDOW_SOURCES
    ../src/findbar.cpp
    ../src/processhelper.cpp
    ../src/settingsdialog.cpp
    ../src/terminalpage.cpp
    ../src/terminalsettings.cpp
    ../src/terminalwindow.cpp
    ../src/windows.cpp
)

add_executable(terminal-scenario-tests
    scenarios/main.cpp
    scenarios/input_test.cpp
    scenarios/menu_test.cpp
    scenarios/tabs_test.cpp
    ${TERMINAL_WINDOW_SOURCES}
)
target_include_directories(terminal-scenario-tests PRIVATE ../src)
target_link_libraries(terminal-scenario-tests PRIVATE terminal-view GTest::gtest)
target_compile_definitions(terminal-scenario-tests PRIVATE KB_LAYOUTS_DIR="${TERMINAL_KB_LAYOUTS_DIR}")
gtest_discover_tests(terminal-scenario-tests PROPERTIES LABELS scenario)

add_executable(terminal-layout-tests
    layout/layout_test.cpp
    ${TERMINAL_WINDOW_SOURCES}
)
target_include_directories(terminal-layout-tests PRIVATE ../src)
target_link_libraries(terminal-layout-tests PRIVATE terminal-view GTest::gtest)
target_compile_definitions(terminal-layout-tests PRIVATE KB_LAYOUTS_DIR="${TERMINAL_KB_LAYOUTS_DIR}")
gtest_discover_tests(terminal-layout-tests PROPERTIES LABELS layout)

# Emulator comparison and throughput, against the earlier builds' tools.
add_executable(termdiff termdiff.cpp)
target_link_libraries(termdiff PRIVATE terminal-core)
add_executable(termbench termbench.cpp)
target_link_libraries(termbench PRIVATE terminal-core)
