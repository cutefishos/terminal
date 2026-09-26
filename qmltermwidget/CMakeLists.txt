include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR} lib src
)
add_definitions(-DHAVE_POSIX_OPENPT -DHAVE_SYS_TIME_H)

# Mirrors lib.pri from upstream; the QTermWidget widget and search bar are not built.
set(TERMWIDGET_SRCS
    lib/BlockArray.h
    lib/CharacterColor.h
    lib/Character.h
    lib/ColorScheme.h
    lib/ColorTables.h
    lib/DefaultTranslatorText.h
    lib/Emulation.h
    lib/ExtendedDefaultTranslator.h
    lib/Filter.h
    lib/History.h
    lib/HistorySearch.h
    lib/KeyboardTranslator.h
    lib/konsole_wcwidth.h
    lib/kprocess.h
    lib/kptydevice.h
    lib/kpty.h
    lib/kpty_p.h
    lib/kptyprocess.h
    lib/ksession.h
    lib/LineChars.h
    lib/LineFont.h
    lib/Pty.h
    lib/ProcessInfo.h
    lib/Screen.h
    lib/ScreenWindow.h
    lib/Session.h
    lib/ShellCommand.h
    lib/TerminalCharacterDecoder.h
    lib/TerminalDisplay.h
    lib/TerminalGpuNode.h
    lib/tools.h
    lib/Vt102Emulation.h
    lib/mac-vkcode.h
    lib/BlockArray.cpp
    lib/ColorScheme.cpp
    lib/Emulation.cpp
    lib/Filter.cpp
    lib/History.cpp
    lib/HistorySearch.cpp
    lib/KeyboardTranslator.cpp
    lib/konsole_wcwidth.cpp
    lib/kprocess.cpp
    lib/kpty.cpp
    lib/kptydevice.cpp
    lib/kptyprocess.cpp
    lib/ksession.cpp
    lib/ProcessInfo.cpp
    lib/Pty.cpp
    lib/Screen.cpp
    lib/ScreenWindow.cpp
    lib/Session.cpp
    lib/ShellCommand.cpp
    lib/TerminalCharacterDecoder.cpp
    lib/TerminalDisplay.cpp
    lib/TerminalGpuNode.cpp
    lib/tools.cpp
    lib/Vt102Emulation.cpp
)

# The plugin's objects, which the tests link as well.
add_library(qmltermwidget_core OBJECT
    ${TERMWIDGET_SRCS}
)

set_target_properties(qmltermwidget_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(qmltermwidget_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib)

target_link_libraries(qmltermwidget_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::Widgets
    Cutefish::Application
)

add_library(cutefishqmltermwidget MODULE
    src/qmltermwidget_plugin.h
    src/qmltermwidget_plugin.cpp
)

target_link_libraries(cutefishqmltermwidget
    qmltermwidget_core
)

# BATCHABLE gives the vertex stage the variant the scene graph batches with.
qt_add_shaders(cutefishqmltermwidget "qmltermwidget_shaders"
    BATCHABLE
    PREFIX "/qmltermwidget/shaders"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    FILES
    shaders/terminalglyph.vert
    shaders/terminalglyph.frag
)

install(TARGETS cutefishqmltermwidget DESTINATION ${INSTALL_QMLDIR}/Cutefish/TermWidget)
install(FILES src/qmldir DESTINATION ${INSTALL_QMLDIR}/Cutefish/TermWidget)
install(FILES src/QMLTermScrollbar.qml DESTINATION ${INSTALL_QMLDIR}/Cutefish/TermWidget)
install(DIRECTORY lib/color-schemes DESTINATION ${INSTALL_QMLDIR}/Cutefish/TermWidget)
install(DIRECTORY lib/kb-layouts DESTINATION ${INSTALL_QMLDIR}/Cutefish/TermWidget)
