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

import Cutefish.TermWidget
import FishUI 1.0 as FishUI

FishUI.Window {
    id: root
    minimumWidth: 400
    minimumHeight: 300
    width: 760
    height: 450
    title: currentItem ? currentItem.title : ""

    blurEnabled: settings.blur
    background.color: terminalBackground
    background.opacity: settings.opacity
    headerDarkMode: chromeDark
    header.height: 40
    windowButtonsAlignment: Qt.AlignVCenter
    windowButtonsTopMargin: 0
    windowButtonsRightMargin: 4

    property int currentIndex: -1
    property alias currentItem: _tabView.currentItem
    readonly property int tabCount: _tabView.count
    readonly property QMLTermWidget currentTerminal: currentItem ? currentItem.terminal : null

    // Zoom is for this window only; the size in Settings stays the default.
    property int fontZoom: 0
    readonly property int fontPointSize: Math.max(5, Math.min(40, settings.fontPointSize + fontZoom))

    function zoom(step) {
        const size = step === 0 ? settings.fontPointSize : fontPointSize + step
        fontZoom = Math.max(5, Math.min(40, size)) - settings.fontPointSize
        _zoomIndicator.show()
    }

    // Settings saved before themes existed hold an empty name.
    readonly property string colorScheme: settings.colorScheme !== "" ? settings.colorScheme : "TokyoNight"
    readonly property var colorSchemeInfo: currentTerminal ? currentTerminal.colorSchemeInfo(colorScheme) : ({})

    // The scheme's own background, so cells in the default colour and the
    // window around the text are one surface.
    readonly property color terminalBackground: colorSchemeInfo.background
                                                ? colorSchemeInfo.background
                                                : "#1A1B26"

    // The title bar is painted with the terminal, so its text and buttons follow
    // the scheme rather than the system theme.
    readonly property color chromeForeground: colorSchemeInfo.foreground ? colorSchemeInfo.foreground
                                                                         : FishUI.Theme.textColor
    readonly property bool chromeDark: 0.2126 * terminalBackground.r + 0.7152 * terminalBackground.g
                                       + 0.0722 * terminalBackground.b < 0.5

    property QtObject settings: GlobalSettings { }

    // A tab pulled out of another window, which this window opens with
    // instead of a new shell.
    property Item detachedPage: null

    // What a tab dragged between windows carries.
    readonly property string tabMimeType: "application/x-cutefish-terminal-tab"
    // A tab is being pulled out: the tab bar stays up, even with one tab
    // left, so the tab can be dragged back into it.
    property bool detaching: false

    // Each dialog is a window of its own, so it is made the first time it is needed.
    property QtObject _exitPrompt: null
    property QtObject _settingsDialog: null
    property QtObject _pasteConfirm: null
    property QtObject _linkConfirm: null

    // Asks before running action when a process is still running in one of pages.
    function confirmClose(pages, action) {
        if (!pages.some(page => page.session.hasActiveProcess)) {
            action()
            return
        }

        if (!_exitPrompt)
            _exitPrompt = _exitPromptComponent.createObject(root)
        _exitPrompt.pendingAction = action
        _exitPrompt.open()
    }

    function showSettings() {
        if (!_settingsDialog)
            _settingsDialog = _settingsDialogComponent.createObject(root)
        _settingsDialog.show()
        _settingsDialog.raise()
    }

    Component {
        id: _exitPromptComponent

        ExitPromptDialog {
            parentWindow: root
            x: root.x + Math.round((root.width - width) / 2)
            y: root.y + Math.round((root.height - height) / 2)
        }
    }

    Component {
        id: _settingsDialogComponent

        SettingsDialog {
            parentWindow: root
        }
    }

    // Line breaks in a paste run as if Enter were pressed, so a paste with any
    // waits here first.
    Component {
        id: _pasteConfirmComponent

        FishUI.ConfirmDialog {
            property var pendingAction: null
            property int lineCount: 0

            parentWindow: root
            x: root.x + Math.round((root.width - width) / 2)
            y: root.y + Math.round((root.height - height) / 2)
            showOnCompleted: false
            title: lineCount > 1 ? qsTr("Paste %1 lines?").arg(lineCount) : qsTr("Paste this line and press Enter?")
            confirmText: qsTr("Paste")

            onAccepted: {
                if (pendingAction)
                    pendingAction()
                pendingAction = null
            }
            onRejected: pendingAction = null
        }
    }

    // A link in the output can name any scheme, and opening one starts whatever
    // program claims it; only web and mail links open without asking.
    Component {
        id: _linkConfirmComponent

        FishUI.ConfirmDialog {
            property string url

            parentWindow: root
            x: root.x + Math.round((root.width - width) / 2)
            y: root.y + Math.round((root.height - height) / 2)
            showOnCompleted: false
            title: qsTr("Open this link?")
            text: url.length > 300 ? url.substring(0, 299) + "…" : url
            confirmText: qsTr("Open")

            onAccepted: Qt.openUrlExternally(url)
        }
    }

    function openLink(url) {
        if (/^(https?|mailto):/i.test(url)) {
            Qt.openUrlExternally(url)
            return
        }

        if (!_linkConfirm)
            _linkConfirm = _linkConfirmComponent.createObject(root)
        _linkConfirm.url = url
        _linkConfirm.open()
    }

    function confirmPaste(text, action) {
        const lines = text.replace(/\n+$/, "").split("\n")
        const shown = lines.slice(0, 4).map(line => line.length > 60 ? line.substring(0, 59) + "…" : line)
        if (lines.length > 4)
            shown.push("…")

        if (!_pasteConfirm)
            _pasteConfirm = _pasteConfirmComponent.createObject(root)

        _pasteConfirm.lineCount = lines.length
        _pasteConfirm.text = shown.join("\n")
        _pasteConfirm.pendingAction = action
        _pasteConfirm.open()
    }

    onClosing: (close) => {
        if (!_quitting) {
            const pages = tabs()
            if (pages.some(page => page.session.hasActiveProcess)) {
                close.accepted = false
                confirmClose(pages, () => {
                    _quitting = true
                    root.close()
                })
                return
            }
        }

        // The shells end with the window, and the application with the last window.
        for (const page of tabs())
            _tabView.closeTab(indexOfTab(page))
        if (_settingsDialog)
            _settingsDialog.close()
        Windows.release(root)
    }

    headerItem: Item {
        id: _header

        // Width taken by the window buttons, mirrored on the left so the
        // title is centred on the window rather than on this item.
        readonly property real reservedWidth: root.width - width + _newTabButton.width

        FishUI.Label {
            visible: !_tabBar.visible
            width: Math.min(implicitWidth, root.width - _header.reservedWidth * 2)
            x: (root.width - width) / 2
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            elide: Text.ElideMiddle
            font.family: settings.fontName
            font.pixelSize: 13
            font.weight: Font.Medium
            color: root.active ? root.chromeForeground
                               : Qt.rgba(root.chromeForeground.r, root.chromeForeground.g, root.chromeForeground.b, 0.5)
        }

        TabBar {
            id: _tabBar
            visible: _tabView.count > 1 || incoming || root.detaching
            view: _tabView
            darkMode: root.chromeDark
            textColor: root.chromeForeground
            anchors.left: parent.left
            anchors.right: _newTabButton.left
            anchors.leftMargin: FishUI.Units.smallSpacing
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter

            onCloseRequested: (index) => root.closeProtection(index)
            onCloseOthersRequested: (index) => root.closeOtherTabs(index)
            onMoveRequested: (from, to) => root.moveTab(from, to)
            // Later, not from inside the tab's own handler: taking the tab out
            // destroys the delegate that is running it.
            onDetachRequested: (index, position) => Qt.callLater(root.detachTab, index, position)
            onRenameRequested: (index, title) => {
                _tabView.contentModel.get(index).customTitle = title.trim()
                _tabView.currentItem.forceActiveFocus()
            }
        }

        // Takes a tab dragged out of another window of this process; one from
        // another terminal process cannot bring its shell along. While it is
        // over the title bar the tab bar shows it where it would drop, and the
        // compositor stops drawing the window it is carried in.
        DropArea {
            anchors.fill: parent
            keys: [root.tabMimeType]

            onEntered: (drag) => {
                drag.accepted = drag.source !== root && drag.source
                        && drag.source.tabMimeType === root.tabMimeType
                if (!drag.accepted)
                    return
                _tabBar.beginIncoming(drag.source.title, mapToItem(_tabBar, drag.x, drag.y).x)
            }
            onPositionChanged: (drag) => {
                if (_tabBar.incoming)
                    _tabBar.incomingX = mapToItem(_tabBar, drag.x, drag.y).x
            }
            onExited: _tabBar.endIncoming(false)
            onDropped: (drop) => {
                const index = _tabBar.incoming ? _tabBar.incomingIndex : _tabView.count
                _tabBar.endIncoming(true)
                root.insertTab(drop.source.takePage(), index)
                root.requestActivate()
                drop.accept(Qt.MoveAction)
            }
        }

        FishUI.HeaderButton {
            id: _newTabButton
            anchors.right: parent.right
            anchors.rightMargin: root.windowButtonsSpacing
            anchors.verticalCenter: parent.verticalCenter
            size: 32
            artwork: "add"
            onClicked: root.openNewTab()
        }
    }

    FishUI.TabView {
        id: _tabView
        anchors.fill: parent
    }

    // Shows the size briefly while zooming.
    Rectangle {
        id: _zoomIndicator
        anchors.centerIn: parent
        width: _zoomLabel.implicitWidth + FishUI.Units.largeSpacing * 2
        height: 36
        radius: height / 2
        color: FishUI.Theme.secondBackgroundColor
        border.width: 1
        border.color: Qt.rgba(FishUI.Theme.textColor.r, FishUI.Theme.textColor.g, FishUI.Theme.textColor.b, 0.1)
        opacity: 0
        visible: opacity > 0

        function show() {
            _zoomFade.stop()
            opacity = 1
            _zoomHide.restart()
        }

        FishUI.Label {
            id: _zoomLabel
            anchors.centerIn: parent
            text: qsTr("%1 pt").arg(root.fontPointSize)
            font.pixelSize: 13
            font.weight: Font.Medium
        }

        Timer {
            id: _zoomHide
            interval: 900
            onTriggered: _zoomFade.start()
        }

        NumberAnimation {
            id: _zoomFade
            target: _zoomIndicator
            property: "opacity"
            to: 0
            duration: 200
        }
    }

    // The visual bell: the whole window, title bar included, briefly lightens,
    // or darkens on a light scheme.
    function flashBell() {
        _bellFade.restart()
    }

    Rectangle {
        id: _bellFlash
        parent: root.contentItem
        anchors.fill: parent
        z: 1000
        color: root.chromeForeground
        opacity: 0
        visible: opacity > 0

        SequentialAnimation {
            id: _bellFade
            PropertyAction { target: _bellFlash; property: "opacity"; value: 0.12 }
            NumberAnimation { target: _bellFlash; property: "opacity"; to: 0; duration: 180 }
        }
    }

    // Set once the close has been confirmed, so closing does not ask again.
    property bool _quitting: false

    Component.onCompleted: {
        if (detachedPage) {
            insertTab(detachedPage, 0)
            detachedPage = null
            return
        }

        // Given on the command line: cutefish-terminal --workdir <dir> -e <command>.
        const directory = typeof StartupDirectory === "string" && StartupDirectory !== "" ? StartupDirectory : "$PWD"
        const command = typeof StartupCommand === "object" ? StartupCommand : []
        openTab(directory, command)
    }

    function openNewTab() {
        if (_tabView.currentItem) {
            openTab(_tabView.currentItem.session.currentDir, [], _tabView.currentIndex + 1)
        } else {
            openTab("$HOME")
        }
    }

    function openTab(path, command, index) {
        const component = Qt.createComponent("Terminal.qml");
        if (component.status === Component.Ready) {
            const object = _tabView.addTab(component, { path: path, command: command || [], root: root }, index)
            // Looked up when the shell exits: closing other tabs shifts the
            // index, and a tab dragged into another window closes there.
            object.terminalClosed.connect(() => object.root.closeTab(object.root.indexOfTab(object)))
        }
    }

    function insertTab(page, index) {
        _tabView.insertTab(page, index)
        page.forceActiveFocus()
    }

    // The only tab of a window pulled out of another, for a window it is
    // dropped on. The empty window hides at once and goes when the drag ends.
    function takePage() {
        const page = _tabView.takeTab(0)
        root.hide()
        return page
    }

    // Pulls a tab out into a window of its own, which follows the pointer
    // until it is dropped. Escape puts the tab back. `position` is where the
    // pointer is in this window.
    function detachTab(index, position) {
        if (index < 0 || index >= _tabView.count || _tabView.count < 2)
            return

        detaching = true
        const page = _tabView.takeTab(index)
        const window = Windows.create({
            detachedPage: page,
            width: root.width,
            height: root.height,
            fontZoom: root.fontZoom,
            restorationBehavior: FishUI.Window.Disabled
        })
        if (!window) {
            detaching = false
            insertTab(page, index)
            return
        }

        // The pointer stays over the new window's title bar, as far across
        // as it is over this one.
        const offset = Qt.point(Math.min(position.x, window.width - 1), window.header.height / 2)
        const action = FishUI.TabTransfer.drag(window, offset, tabMimeType)
        detaching = false

        if (action === Qt.IgnoreAction && window.tabCount === 1)
            insertTab(window.takePage(), index)

        if (window.tabCount === 0)
            Windows.release(window)
        else
            window.requestActivate()
    }

    function indexOfTab(object) {
        for (var i = 0; i < _tabView.contentModel.count; ++i) {
            if (_tabView.contentModel.get(i) === object)
                return i
        }

        return -1
    }

    function tabs() {
        const pages = []
        for (let i = 0; i < _tabView.contentModel.count; ++i)
            pages.push(_tabView.contentModel.get(i))
        return pages
    }

    function closeProtection(index) {
        const page = _tabView.contentModel.get(index)
        if (page)
            confirmClose([page], () => closeTab(indexOfTab(page)))
    }

    function closeOtherTabs(index) {
        const keep = _tabView.contentModel.get(index)
        const others = tabs().filter(page => page !== keep)
        confirmClose(others, () => {
            for (const page of others)
                closeTab(indexOfTab(page))
            _tabView.currentIndex = indexOfTab(keep)
        })
    }

    function selectTab(index) {
        if (index < 0 || index >= _tabView.count)
            return
        _tabView.currentIndex = index
        _tabView.currentItem.forceActiveFocus()
    }

    // Moves the current tab by step, keeping it current.
    function moveCurrentTab(step) {
        moveTab(_tabView.currentIndex, _tabView.currentIndex + step)
    }

    function moveTab(from, to) {
        if (from === to || from < 0 || to < 0 || from >= _tabView.count || to >= _tabView.count)
            return
        const page = _tabView.contentModel.get(from)
        _tabView.moveItem(from, to)
        _tabView.currentIndex = indexOfTab(page)
        page.forceActiveFocus()
    }

    function newWindow() {
        Process.newWindow(currentItem ? currentItem.session.currentDir : "")
    }

    function closeTab(index) {
        if (index < 0)
            return

        _tabView.closeTab(index)

        if (_tabView.contentModel.count === 0) {
            _quitting = true
            root.close()
        }
    }

    function closeCurrentTab() {
        closeProtection(_tabView.currentIndex)
    }

    // Next (step 1) or previous (step -1) tab, wrapping around.
    function cycleTab(step) {
        const count = _tabView.count
        if (count > 1)
            selectTab((_tabView.currentIndex + step + count) % count)
    }
}
