find_package(Qt6 REQUIRED Test)

add_executable(vt102emulation-test vt102emulation_test.cpp)
target_link_libraries(vt102emulation-test PRIVATE qmltermwidget_core Qt6::Test)
target_compile_definitions(vt102emulation-test PRIVATE
    KB_LAYOUTS_DIR="${CMAKE_SOURCE_DIR}/qmltermwidget/lib/kb-layouts"
)
add_test(NAME vt102emulation COMMAND vt102emulation-test -platform offscreen)

# Runs /bin/sh on a pty.
add_executable(ksession-test ksession_test.cpp)
target_link_libraries(ksession-test PRIVATE qmltermwidget_core Qt6::Test)
target_compile_definitions(ksession-test PRIVATE
    KB_LAYOUTS_DIR="${CMAKE_SOURCE_DIR}/qmltermwidget/lib/kb-layouts"
)
add_test(NAME ksession COMMAND ksession-test -platform offscreen)
