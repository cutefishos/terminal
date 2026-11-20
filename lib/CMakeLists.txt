pkg_check_modules(GHOSTTY_VT REQUIRED IMPORTED_TARGET libghostty-vt)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)

# The terminal itself: libghostty-vt's state, the pty, the session.
add_library(terminal-emulator STATIC
    colorschemes.cpp
    links.cpp
    proxyenvironment.cpp
    pty.cpp
    search.cpp
    session.cpp
    terminal.cpp
    terminalinput.cpp
)
target_include_directories(terminal-emulator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(terminal-emulator
    PUBLIC
        Cutefish::ToolkitBase
        PkgConfig::GHOSTTY_VT
    PRIVATE
        PkgConfig::XKBCOMMON
)

# The display, on the toolkit.
add_library(terminal-display STATIC
    boxdrawing.cpp
    cellfont.cpp
    terminalview.cpp
    terminalviewinput.cpp
    terminalviewpaint.cpp
)
target_link_libraries(terminal-display
    PUBLIC
        terminal-emulator
        Cutefish::Toolkit
    PRIVATE
        PkgConfig::XKBCOMMON
)
