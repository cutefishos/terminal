# libghostty-vt as a static library under a name of Cutefish's own, so it is
# linked in whole and nothing of it is needed at run time.
pkg_check_modules(GHOSTTY_VT REQUIRED IMPORTED_TARGET cutefish-ghostty-vt)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(PCRE2 REQUIRED IMPORTED_TARGET libpcre2-8)

# The terminal itself: libghostty-vt's state, the pty, the session.
add_library(terminal-emulator STATIC
    colorschemes.cpp
    links.cpp
    proxyenvironment.cpp
    pty.cpp
    search.cpp
    session.cpp
    shellintegration.cpp
    terminal.cpp
    terminalinput.cpp
)
target_include_directories(terminal-emulator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(terminal-emulator PRIVATE
    SHELL_INTEGRATION_DIR="${CMAKE_INSTALL_FULL_DATADIR}/cutefish-terminal/shell-integration")
target_link_libraries(terminal-emulator
    PUBLIC
        Cutefish::ToolkitBase
        PkgConfig::GHOSTTY_VT
    PRIVATE
        PkgConfig::PCRE2
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
