// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore

Settings {
    // The default location follows the binary name, cutefish-terminal.
    location: StandardPaths.writableLocation(StandardPaths.GenericConfigLocation)
              + "/cutefish/terminal.conf"

    property int fontPointSize: 10
    property string fontName: "Noto Mono"
    property bool blinkingCursor: true
    // 0 block, 1 underline, 2 I-beam.
    property int cursorShape: 0
    // Lines kept above the screen; -1 keeps everything.
    property int scrollbackLines: 10000
    property bool visualBell: false
    property bool confirmMultilinePaste: true
    // Off paints the text on the CPU, as before.
    property bool gpuRendering: true
    // A bundled color scheme, by file name.
    property string colorScheme: "TokyoNight"

    property double opacity: 0.85
    property bool blur: true
}
