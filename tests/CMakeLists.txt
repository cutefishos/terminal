find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(terminal-core-tests
    main.cpp
    emulation_test.cpp
    session_test.cpp
)
target_link_libraries(terminal-core-tests PRIVATE terminal-core GTest::gtest)
target_compile_definitions(terminal-core-tests PRIVATE
    KB_LAYOUTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../qmltermwidget/lib/kb-layouts"
)
gtest_discover_tests(terminal-core-tests)

# Emulator comparison and throughput, against the earlier builds' tools.
add_executable(termdiff termdiff.cpp)
target_link_libraries(termdiff PRIVATE terminal-core)
add_executable(termbench termbench.cpp)
target_link_libraries(termbench PRIVATE terminal-core)
