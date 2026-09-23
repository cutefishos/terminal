/*
 *   Copyright 2021 Reion Wong <reionwong@gmail.com>
 *   Copyright 2018 Camilo Higuita <milo.h@aol.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it  be useful,
 *   but WITHOUT ANY WARRANTY;  even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Window

import FishUI 1.0 as FishUI
import Cutefish.TermWidget

Item {
    id: control

    height: _tabView.height
    width: _tabView.width
    focus: true

    signal urlsDropped(var urls)
    signal keyPressed(var event)
    signal terminalClosed()

    property string path: "$PWD"
    property alias terminal: _terminal
    readonly property QMLTermSession session: _session
    readonly property string title: _session.title

    onUrlsDropped: (urls) => {
        for (var i in urls)
            _session.sendText(urls[i].toString().replace("file://", "") + " ")
    }

    onKeyPressed: (event) => {
        if ((event.key === Qt.Key_A)
                && (event.modifiers & Qt.ControlModifier)
                && (event.modifiers & Qt.ShiftModifier)) {
            _terminal.selectAll()
            event.accepted = true
        }

        if ((event.key === Qt.Key_C)
                && (event.modifiers & Qt.ControlModifier)
                && (event.modifiers & Qt.ShiftModifier)) {
            _copyAction.trigger()
            event.accepted = true
        }

        if ((event.key === Qt.Key_V)
                && (event.modifiers & Qt.ControlModifier)
                && (event.modifiers & Qt.ShiftModifier)) {
            _pasteAction.trigger()
            event.accepted = true
        }

        if ((event.key === Qt.Key_Q)
                && (event.modifiers & Qt.ControlModifier)
                && (event.modifiers & Qt.ShiftModifier)) {
            Qt.quit()
        }

        if ((event.key === Qt.Key_T)
                && (event.modifiers & Qt.ControlModifier)
                && (event.modifiers & Qt.ShiftModifier)) {
            root.openNewTab()
            event.accepted = true
        }

        if ((event.key === Qt.Key_W)
                && (event.modifiers & Qt.ControlModifier)
                && (event.modifiers & Qt.ShiftModifier)) {
            root.closeCurrentTab()
            event.accepted = true
        }

        if (event.key === Qt.Key_Tab && event.modifiers & Qt.ControlModifier) {
            root.toggleTab()
            event.accepted = true
        }

        if (event.key === Qt.Key_F11) {
            root.visibility = root.isFullScreen ? Window.Windowed : Window.FullScreen
            event.accepted = true
        }
    }

    QMLTermWidget {
        id: _terminal
        anchors.fill: parent
        colorScheme: FishUI.Theme.darkMode ? "Cutefish-Dark" : "Cutefish-Light"
        font.family: settings.fontName
        font.pointSize: settings.fontPointSize
        blinkingCursor: settings.blinkingCursor
        fullCursorHeight: true
        backgroundOpacity: 0

        Keys.enabled: true
        Keys.onPressed: (event) => control.keyPressed(event)

        session: QMLTermSession {
            id: _session
            onFinished: control.terminalClosed()
            initialWorkingDirectory: control.path
        }

        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: true
            cursorShape: _terminal.terminalUsesMouse ? Qt.ArrowCursor : Qt.IBeamCursor
            acceptedButtons:  Qt.RightButton | Qt.LeftButton

            onDoubleClicked: (mouse) => {
                 var coord = correctDistortion(mouse.x, mouse.y)
                 _terminal.simulateMouseDoubleClick(coord.x, coord.y, mouse.button, mouse.buttons, mouse.modifiers)
            }

            onPressed: (mouse) => {
                if ((!_terminal.terminalUsesMouse || mouse.modifiers & Qt.ShiftModifier)
                        && mouse.button == Qt.RightButton) {
                    updateMenu()
                    terminalMenu.popup()
                } else {
                    var coord = correctDistortion(mouse.x, mouse.y)
                    _terminal.simulateMousePress(coord.x, coord.y, mouse.button, mouse.buttons, mouse.modifiers)
                }
            }

            onReleased: (mouse) => {
                var coord = correctDistortion(mouse.x, mouse.y)
                _terminal.simulateMouseRelease(coord.x, coord.y, mouse.button, mouse.buttons, mouse.modifiers)
            }

            onPositionChanged: (mouse) => {
                var coord = correctDistortion(mouse.x, mouse.y)
                _terminal.simulateMouseMove(coord.x, coord.y, mouse.button, mouse.buttons, mouse.modifiers)
            }

            onClicked: (mouse) => {
                if (mouse.button === Qt.RightButton) {
                    updateMenu()
                    terminalMenu.popup()
                } else if(mouse.button === Qt.LeftButton) {
                    _terminal.forceActiveFocus()
                }

                // control.clicked()
            }
        }

        Component.onCompleted: {
            _session.startShellProgram()
            _terminal.forceActiveFocus()
        }
    }

    Action {
        id: _copyAction
        text: qsTr("Copy")
        onTriggered: _terminal.copyClipboard()
    }

    Action {
        id: _pasteAction
        text: qsTr("Paste")
        onTriggered: _terminal.pasteClipboard()
    }

    FishUI.DesktopMenu {
        id: terminalMenu

        FishUI.MenuItem {
            id: copyMenuItem
            action: _copyAction
            visible: _terminal.hasSelection
        }

        FishUI.MenuItem {
            id: pasteMenuItem
            text: qsTr("Paste")
            action: _pasteAction
        }

        FishUI.MenuItem {
            id: selectAllItem
            text: qsTr("Select All")
            onTriggered: _terminal.selectAll()
        }

        FishUI.MenuItem {
            text: qsTr("Open File Manager")
            onTriggered: Process.openFileManager(_session.currentDir)
        }

        FishUI.MenuItem {
            text: root.isFullScreen ? qsTr("Exit full screen") : qsTr("Full screen")
            onTriggered: {
                root.visibility = root.isFullScreen ? Window.Windowed : Window.FullScreen
            }
        }

        FishUI.MenuItem {
            text: qsTr("Settings")
            onTriggered: {
                settingsDialog.show()
                settingsDialog.raise()
            }
        }
    }

    FishUI.ScrollBar {
        id: _scrollbar
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.bottomMargin: FishUI.Units.smallSpacing * 1.5
        hoverEnabled: true
        active: hovered || pressed
        orientation: Qt.Vertical
        size: (_terminal.lines / (_terminal.lines + _terminal.scrollbarMaximum - _terminal.scrollbarMinimum))
        position: _terminal.scrollbarCurrentValue / (_terminal.lines + _terminal.scrollbarMaximum)
    }

    DropArea {
        id: _dropArea
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls) {
                control.urlsDropped(drop.urls)
            } else if (drop.hasText) {
                _session.sendText(drop.text)
            }
        }
    }

    function forceActiveFocus() {
        _terminal.forceActiveFocus()
    }

    function correctDistortion(x, y) {
        x = x / width
        y = y / height

        var cc = Qt.size(0.5 - x, 0.5 - y)
        var distortion = 0

        return Qt.point((x - cc.width  * (1 + distortion) * distortion) * _terminal.width,
                        (y - cc.height * (1 + distortion) * distortion) * _terminal.height)
    }

    function updateMenu() {
        pasteMenuItem.visible = Utils.text() !== ""
    }
}
