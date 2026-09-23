/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Reion Wong <reionwong@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick
import QtCore

Settings {
    // The default location follows the binary name, cutefish-terminal.
    location: StandardPaths.writableLocation(StandardPaths.GenericConfigLocation)
              + "/cutefish/terminal.conf"

    property int width: 750
    property int height: 500
    property int fontPointSize: 10
    property string fontName: "Noto Mono"
    property bool blinkingCursor: true
    // 0 block, 1 underline, 2 I-beam.
    property int cursorShape: 0
    // Lines kept above the screen; -1 keeps everything.
    property int scrollbackLines: 10000
    property bool visualBell: true
    property bool confirmMultilinePaste: true
    // A bundled color scheme, by file name.
    property string colorScheme: "TokyoNight"

    property double opacity: 0.85
    property bool blur: true
}
