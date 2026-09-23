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

        if ((event.key === Qt.Key_F)
                && (event.modifiers & Qt.ControlModifier)
                && (event.modifiers & Qt.ShiftModifier)) {
            control.openFind()
            event.accepted = true
        }

        // Zoom: Ctrl+= (or Ctrl++), Ctrl+-, Ctrl+0.
        if ((event.modifiers & Qt.ControlModifier) && !(event.modifiers & Qt.AltModifier)) {
            if (event.key === Qt.Key_Equal || event.key === Qt.Key_Plus) {
                root.zoom(1)
                event.accepted = true
            } else if (event.key === Qt.Key_Minus && !(event.modifiers & Qt.ShiftModifier)) {
                root.zoom(-1)
                event.accepted = true
            } else if (event.key === Qt.Key_0 && !(event.modifiers & Qt.ShiftModifier)) {
                root.zoom(0)
                event.accepted = true
            }
        }

        if (event.key === Qt.Key_F11) {
            root.visibility = root.isFullScreen ? Window.Windowed : Window.FullScreen
            event.accepted = true
        }
    }

    QMLTermWidget {
        id: _terminal
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.topMargin: 2
        anchors.bottomMargin: 6
        colorScheme: root.colorScheme
        font.family: settings.fontName
        font.pointSize: root.fontPointSize
        blinkingCursor: settings.blinkingCursor
        confirmMultilinePaste: true
        fullCursorHeight: true
        backgroundOpacity: 0

        Keys.enabled: true
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Control)
                _linkHover.ctrlHeld = true
            control.keyPressed(event)
        }
        Keys.onReleased: (event) => {
            if (event.key === Qt.Key_Control)
                _linkHover.ctrlHeld = false
        }

        onScrollbarParamsChanged: _linkHover.refresh()
        onMultilinePasteRequested: (text) => root.confirmPaste(text, () => _terminal.confirmPaste())

        // The link under the pointer: underlined on hover, opened with Ctrl+click.
        QtObject {
            id: _linkHover

            property var segments: []
            property bool ctrlHeld: false
            property real posX: -1
            property real posY: -1
            property string cell: ""

            function update(x, y, modifiers) {
                ctrlHeld = (modifiers & Qt.ControlModifier) !== 0
                posX = x
                posY = y

                // Only a move to another cell can change the link under the pointer.
                const key = Math.floor(x / Math.max(1, _terminal.fontMetrics.width)) + ":"
                          + Math.floor(y / Math.max(1, _terminal.fontMetrics.height))
                if (key === cell)
                    return

                cell = key
                segments = _terminal.linkUnderlineAt(x, y)
            }

            function refresh() {
                if (posX >= 0)
                    segments = _terminal.linkUnderlineAt(posX, posY)
            }

            function clear() {
                posX = -1
                cell = ""
                segments = []
            }
        }

        Repeater {
            model: _linkHover.segments

            Rectangle {
                x: modelData.x
                y: modelData.y
                width: modelData.width
                height: 1
                color: root.chromeForeground
            }
        }

        session: QMLTermSession {
            id: _session
            onFinished: control.terminalClosed()
            initialWorkingDirectory: control.path
        }

        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: true
            hoverEnabled: true
            cursorShape: _linkHover.segments.length && _linkHover.ctrlHeld ? Qt.PointingHandCursor
                         : _terminal.terminalUsesMouse ? Qt.ArrowCursor : Qt.IBeamCursor
            acceptedButtons:  Qt.RightButton | Qt.LeftButton

            // The menu opens on release: shown while the button is still down, it
            // takes the release, leaving this area pressed and its cursor stale.
            property bool menuPress: false
            // A Ctrl+click on a link opens it and must not start a selection.
            property bool linkPress: false

            onDoubleClicked: (mouse) => {
                 _terminal.simulateMouseDoubleClick(mouse.x, mouse.y, mouse.button, mouse.buttons, mouse.modifiers)
            }

            onPressed: (mouse) => {
                menuPress = mouse.button === Qt.RightButton
                        && (!_terminal.terminalUsesMouse || mouse.modifiers & Qt.ShiftModifier)

                linkPress = false
                if (mouse.button === Qt.LeftButton && (mouse.modifiers & Qt.ControlModifier)) {
                    const link = _terminal.linkAt(mouse.x, mouse.y)
                    if (link !== "") {
                        linkPress = true
                        Qt.openUrlExternally(link)
                        return
                    }
                }

                if (!menuPress)
                    _terminal.simulateMousePress(mouse.x, mouse.y, mouse.button, mouse.buttons, mouse.modifiers)
            }

            onReleased: (mouse) => {
                if (linkPress)
                    return

                if (menuPress) {
                    menuPress = false
                    control.openMenu(mouse.x, mouse.y)
                    return
                }

                _terminal.simulateMouseRelease(mouse.x, mouse.y, mouse.button, mouse.buttons, mouse.modifiers)
            }

            onPositionChanged: (mouse) => {
                if (!pressed) {
                    _linkHover.update(mouse.x, mouse.y, mouse.modifiers)
                    return
                }

                if (!menuPress && !linkPress)
                    _terminal.simulateMouseMove(mouse.x, mouse.y, mouse.button, mouse.buttons, mouse.modifiers)
            }

            onExited: _linkHover.clear()

            // A right click the program took stays its own.
            onClicked: (mouse) => {
                if (mouse.button === Qt.LeftButton && !linkPress)
                    _terminal.forceActiveFocus()
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

    // What the menu offers depends on what it was opened over; captured on open.
    QtObject {
        id: _menuContext

        property string link
        property string path
        property bool canPaste: false

        readonly property string pathName: {
            const name = path.substring(path.lastIndexOf("/") + 1) || path
            return name.length > 30 ? name.substring(0, 29) + "…" : name
        }
    }

    // The menu is a window of its own; it is made on the first right click.
    property QtObject _terminalMenu: null

    Component {
        id: _terminalMenuComponent

        FishUI.DesktopMenu {
            FishUI.MenuItem {
                text: qsTr("Open Link")
                visible: _menuContext.link !== ""
                onTriggered: Qt.openUrlExternally(_menuContext.link)
            }

            FishUI.MenuItem {
                text: qsTr("Copy Link")
                visible: _menuContext.link !== ""
                onTriggered: Utils.setText(_menuContext.link)
            }

            FishUI.MenuSeparator {
                visible: _menuContext.link !== ""
            }

            FishUI.MenuItem {
                text: qsTr("Open “%1”").arg(_menuContext.pathName)
                visible: _menuContext.path !== ""
                onTriggered: Process.openUrl(_menuContext.path)
            }

            FishUI.MenuItem {
                text: qsTr("Show in File Manager")
                visible: _menuContext.path !== ""
                onTriggered: Process.showInFileManager(_menuContext.path)
            }

            FishUI.MenuSeparator {
                visible: _menuContext.path !== ""
            }

            FishUI.MenuItem {
                action: _copyAction
                visible: _terminal.hasSelection
            }

            FishUI.MenuItem {
                action: _pasteAction
                visible: _menuContext.canPaste
            }

            FishUI.MenuItem {
                text: qsTr("Select All")
                onTriggered: _terminal.selectAll()
            }

            FishUI.MenuItem {
                text: qsTr("Find…")
                onTriggered: control.openFind()
            }

            FishUI.MenuItem {
                text: qsTr("Clear Scrollback")
                visible: _terminal.scrollbarMaximum > 0
                onTriggered: _session.clearScrollback()
            }

            FishUI.MenuSeparator {}

            FishUI.MenuItem {
                text: qsTr("New Tab")
                onTriggered: root.openNewTab()
            }

            FishUI.MenuItem {
                text: qsTr("Open File Manager")
                onTriggered: Process.openFileManager(_session.currentDir)
            }

            FishUI.MenuSeparator {}

            FishUI.MenuItem {
                text: root.isFullScreen ? qsTr("Exit full screen") : qsTr("Full screen")
                onTriggered: {
                    root.visibility = root.isFullScreen ? Window.Windowed : Window.FullScreen
                }
            }

            FishUI.MenuItem {
                text: qsTr("Settings")
                onTriggered: root.showSettings()
            }
        }
    }

    // Overlays the right margin; shown while scrolling or under the pointer.
    FishUI.ScrollBar {
        id: _scrollbar

        readonly property int totalLines: _terminal.lines + _terminal.scrollbarMaximum - _terminal.scrollbarMinimum

        anchors.right: parent.right
        anchors.top: _terminal.top
        anchors.bottom: _terminal.bottom
        orientation: Qt.Vertical
        darkMode: root.chromeDark
        active: hovered || pressed || _scrollActivity.running
        size: totalLines > 0 ? _terminal.lines / totalLines : 1
        position: totalLines > 0 ? (_terminal.scrollbarCurrentValue - _terminal.scrollbarMinimum) / totalLines : 0

        onPositionChanged: {
            if (pressed)
                _terminal.scrollbarCurrentValue = Math.round(position * totalLines) + _terminal.scrollbarMinimum
        }

        Timer {
            id: _scrollActivity
            interval: 800
        }

        Connections {
            target: _terminal
            function onScrollbarValueChanged() { _scrollActivity.restart() }
        }
    }

    Loader {
        id: _findBar
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: FishUI.Units.smallSpacing
        anchors.rightMargin: FishUI.Units.largeSpacing
        z: 1
        active: false

        sourceComponent: FindBar {
            terminal: _terminal
            backgroundColor: root.terminalBackground
            textColor: root.chromeForeground
            darkMode: root.chromeDark
            errorColor: root.colorSchemeInfo.colors ? root.colorSchemeInfo.colors[1] : FishUI.Theme.redColor
            accentColor: root.colorSchemeInfo.colors ? root.colorSchemeInfo.colors[4] : FishUI.Theme.highlightColor
            onClosed: _terminal.forceActiveFocus()
        }
    }

    DropArea {
        id: _dropArea
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls) {
                control.urlsDropped(drop.urls)
            } else if (drop.hasText) {
                const text = drop.text
                if (text.indexOf("\n") !== -1)
                    root.confirmPaste(text, () => _session.sendText(text))
                else
                    _session.sendText(text)
            }
        }
    }

    function forceActiveFocus() {
        _terminal.forceActiveFocus()
    }

    // Starts from the selection when it is a single line, as a search term would be.
    function openFind() {
        const selection = _terminal.hasSelection ? _terminal.selectedText().trim() : ""
        _findBar.active = true
        _findBar.item.open(selection.indexOf("\n") === -1 ? selection : "")
    }

    function openMenu(x, y) {
        // A right click on a word outside the selection selects that word first,
        // so Copy and the file and link items act on what was clicked.
        if (!_terminal.isSelectedAt(x, y))
            _terminal.selectWordAt(x, y)

        const selection = _terminal.hasSelection ? _terminal.selectedText().trim() : ""
        const selectionIsLink = /^[a-z][a-z0-9+.-]*:\/\/\S+$/i.test(selection)

        _menuContext.link = _terminal.linkAt(x, y) || (selectionIsLink ? selection : "")
        _menuContext.path = _menuContext.link === "" ? Process.existingPath(selection, _session.currentDir) : ""
        _menuContext.canPaste = Utils.text() !== ""

        if (!_terminalMenu)
            _terminalMenu = _terminalMenuComponent.createObject(control)
        _terminalMenu.popup()
    }
}
