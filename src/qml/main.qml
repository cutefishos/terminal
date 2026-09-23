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
    width: settings.width
    height: settings.height
    title: currentItem && currentItem.terminal ? currentItem.terminal.session.title : ""

    blurEnabled: settings.blur
    background.color: terminalBackground
    background.opacity: settings.opacity
    headerDarkMode: chromeDark
    header.height: 44
    windowButtonsAlignment: Qt.AlignVCenter
    windowButtonsTopMargin: 0
    windowButtonsRightMargin: FishUI.Units.smallSpacing

    property int currentIndex: -1
    property alias currentItem: _tabView.currentItem
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
    readonly property color chromeTint: chromeDark ? "#FFFFFF" : "#323238"

    GlobalSettings { id: settings }

    ExitPromptDialog {
        id: exitPrompt
        parentWindow: root
        x: root.x + Math.round((root.width - width) / 2)
        y: root.y + Math.round((root.height - height) / 2)

        onAccepted: {
            if (index != -1) {
                closeTab(index)
            } else {
                Qt.quit()
            }
        }
    }

    SettingsDialog {
        id: settingsDialog
        parentWindow: root
    }

    onClosing: (close) => {
        if (!root.isMaximized && !root.isFullScreen) {
            settings.width = root.width
            settings.height = root.height
        }

        // Exit prompt.
        for (var i = 0; i < _tabView.contentModel.count; ++i) {
            var obj = _tabView.contentModel.get(i)
            if (obj.session.hasActiveProcess) {
                exitPrompt.index = -1
                exitPrompt.open()
                close.accepted = false
                break
            }
        }
    }

    headerItem: Item {
        id: _header

        // Width taken by the window buttons, mirrored on the left so the
        // title is centred on the window rather than on this item.
        readonly property real reservedWidth: root.width - width + _newTabButton.width

        FishUI.Label {
            visible: _tabView.count < 2
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
            visible: _tabView.count > 1
            view: _tabView
            darkMode: root.chromeDark
            textColor: root.chromeForeground
            anchors.left: parent.left
            anchors.right: _newTabButton.left
            anchors.leftMargin: FishUI.Units.largeSpacing
            anchors.rightMargin: FishUI.Units.smallSpacing
            anchors.verticalCenter: parent.verticalCenter

            onCloseRequested: (index) => root.closeProtection(index)
        }

        FishUI.RoundImageButton {
            id: _newTabButton
            anchors.right: parent.right
            anchors.rightMargin: root.windowButtonsSpacing
            anchors.verticalCenter: parent.verticalCenter
            size: 32
            source: "qrc:/fishui/kit/images/" + (root.chromeDark ? "dark/" : "light/") + "add.svg"
            hoveredColor: Qt.rgba(root.chromeTint.r, root.chromeTint.g, root.chromeTint.b, root.chromeDark ? 0.12 : 0.11)
            pressedColor: Qt.rgba(root.chromeTint.r, root.chromeTint.g, root.chromeTint.b, root.chromeDark ? 0.06 : 0.21)
            // Same artwork grid and sampling as the window buttons beside it.
            iconSize: 24
            image.smooth: false
            image.antialiasing: true
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

    Component.onCompleted: {
        openTab("$PWD")
    }

    function openNewTab() {
        if (_tabView.currentItem) {
            openTab(_tabView.currentItem.session.currentDir)
        } else {
            openTab("$HOME")
        }
    }

    function openTab(path) {
        if (_tabView.contentModel.count > 7)
            return

        const component = Qt.createComponent("Terminal.qml");
        if (component.status === Component.Ready) {
            const object = _tabView.addTab(component, {path: path})
            // Looked up when the shell exits: closing other tabs shifts the index.
            object.terminalClosed.connect(() => closeTab(indexOfTab(object)))
        }
    }

    function indexOfTab(object) {
        for (var i = 0; i < _tabView.contentModel.count; ++i) {
            if (_tabView.contentModel.get(i) === object)
                return i
        }

        return -1
    }

    function closeProtection(index) {
        var obj = _tabView.contentModel.get(index)
        if (obj.session.hasActiveProcess) {
            exitPrompt.index = index
            exitPrompt.open()
            return
        }

        closeTab(index)
    }

    function closeTab(index) {
        if (index < 0)
            return

        _tabView.closeTab(index)

        if (_tabView.contentModel.count === 0)
            Qt.quit()
    }

    function closeCurrentTab() {
        closeProtection(_tabView.currentIndex)
    }

    function toggleTab() {
        var nextIndex = _tabView.currentIndex
        ++nextIndex
        if (nextIndex > _tabView.contentModel.count - 1)
            nextIndex = 0

        _tabView.currentIndex = nextIndex
    }
}
