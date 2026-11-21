find_package(GTest REQUIRED)
include(GoogleTest)

# The emulator and the session alone: no toolkit view library, so no window can come in.
add_executable(terminal-logic-tests
    logic/main.cpp
    logic/links_test.cpp
    logic/proxy_test.cpp
    logic/session_test.cpp
    logic/shellintegration_test.cpp
    logic/terminal_test.cpp
)
target_link_libraries(terminal-logic-tests PRIVATE terminal-emulator GTest::gtest)
target_compile_definitions(terminal-logic-tests PRIVATE TERMINAL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
gtest_discover_tests(terminal-logic-tests PROPERTIES LABELS logic)

# The windows and tabs on the headless platform.
set(TERMINAL_WINDOW_SOURCES
    ../src/findbar.cpp
    ../src/processhelper.cpp
    ../src/settingsdialog.cpp
    ../src/terminalpage.cpp
    ../src/terminalsettings.cpp
    ../src/terminalwindow.cpp
    ../src/terminalwindowdialogs.cpp
    ../src/windows.cpp
)

add_executable(terminal-scenario-tests
    scenarios/main.cpp
    scenarios/input_test.cpp
    scenarios/interaction_test.cpp
    scenarios/menu_test.cpp
    scenarios/tabs_test.cpp
    scenarios/view_test.cpp
    ${TERMINAL_WINDOW_SOURCES}
)
target_include_directories(terminal-scenario-tests PRIVATE ../src)
target_link_libraries(terminal-scenario-tests PRIVATE terminal-display GTest::gtest)
gtest_discover_tests(terminal-scenario-tests PROPERTIES LABELS scenario)

add_executable(terminal-layout-tests
    layout/display_test.cpp
    layout/layout_test.cpp
    layout/scrolling_test.cpp
    ${TERMINAL_WINDOW_SOURCES}
)
target_include_directories(terminal-layout-tests PRIVATE ../src)
target_link_libraries(terminal-layout-tests PRIVATE terminal-display GTest::gtest)
gtest_discover_tests(terminal-layout-tests PROPERTIES LABELS layout)

# Throughput, measured as in the terminal's performance notes.
add_executable(termbench termbench.cpp)
target_link_libraries(termbench PRIVATE terminal-emulator)
