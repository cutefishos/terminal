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

import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

import FishUI 1.0 as FishUI
import Cutefish.TermWidget 1.0

Page {
    id: control

    height: _view.height
    width: _view.width
    focus: true

    // Drop effect
    opacity: _dropArea.containsDrag ? 0.8 : 1

    signal urlsDropped(var urls)
    signal keyPressed(var event)
    signal terminalClosed()

    property string path
    property alias terminal: _terminal
    readonly property QMLTermSession session: _session

    title: _session.title

    onUrlsDropped: {
        for (var i in urls)
            _session.sendText(urls[i].replace("file://", "") + " ")
    }

    onKeyPressed: {
        if ((event.key === Qt.Key_C)
                && (event.modifiers & Qt.ControlModifier)
                && (event.modifiers & Qt.ShiftModifier)) {
            copyAction.triggered()
        }

        if ((event.key === Qt.Key_V)
                && (event.modifiers & Qt.ControlModifier)
                && (event.modifiers & Qt.ShiftModifier)) {
            pasteAction.triggered()
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
        }

        if ((event.key === Qt.Key_W)
                && (event.modifiers & Qt.ControlModifier)
                && (event.modifiers & Qt.ShiftModifier)) {
            root.closeCurrentTab()
        }

        if (event.key === Qt.Key_Tab && event.modifiers & Qt.ControlModifier) {
            root.toggleTab()
        }
    }

    QMLTermWidget {
        id: _terminal
        anchors.fill: parent
        colorScheme: "GreenOnBlack"
        font.family: "Noto Sans Mono"
        font.pointSize: settings.fontPointSize
        blinkingCursor: settings.blinkingCursor
        fullCursorHeight: true

        Keys.enabled: true
        Keys.onPressed: control.keyPressed(event)

        onUsesMouseChanged: {
            console.log(_terminal.getUsesMouse)
        }

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

            onDoubleClicked: {
                 var coord = correctDistortion(mouse.x, mouse.y)
                 _terminal.simulateMouseDoubleClick(coord.x, coord.y, mouse.button, mouse.buttons, mouse.modifiers)
            }

            onPressed: {
                if ((!_terminal.terminalUsesMouse || mouse.modifiers & Qt.ShiftModifier)
                        && mouse.button == Qt.RightButton) {
                    terminalMenu.open()
                } else {
                    var coord = correctDistortion(mouse.x, mouse.y)
                    _terminal.simulateMousePress(coord.x, coord.y, mouse.button, mouse.buttons, mouse.modifiers)
                }
            }

            onReleased: {
                var coord = correctDistortion(mouse.x, mouse.y)
                _terminal.simulateMouseRelease(coord.x, coord.y, mouse.button, mouse.buttons, mouse.modifiers)
            }

            onPositionChanged: {
                var coord = correctDistortion(mouse.x, mouse.y)
                _terminal.simulateMouseMove(coord.x, coord.y, mouse.button, mouse.buttons, mouse.modifiers)
            }

            onClicked: {
                if (mouse.button === Qt.RightButton) {
                    terminalMenu.open()

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

    FishUI.DesktopMenu {
        id: terminalMenu

        MenuItem {
            id: copyAction
            text: qsTr("Copy")
            onTriggered: _terminal.copyClipboard()
        }

        MenuItem {
            id: pasteAction
            text: qsTr("Paste")
            onTriggered: _terminal.pasteClipboard()
        }

        MenuItem {
            text: qsTr("Open File Manager")
            onTriggered: Process.startDetached("gio", ["open", _session.currentDir])
        }
    }

    ScrollBar {
        id: _scrollbar
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        hoverEnabled: true
        active: hovered || pressed
        orientation: Qt.Vertical
        size: (_terminal.lines / (_terminal.lines + _terminal.scrollbarMaximum - _terminal.scrollbarMinimum))
        position: _terminal.scrollbarCurrentValue / (_terminal.lines + _terminal.scrollbarMaximum)
    }

    DropArea {
        id: _dropArea
        anchors.fill: parent
        onDropped: {
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
}
