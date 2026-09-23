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
import QtQuick.Layouts

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
    background.color: FishUI.Theme.backgroundColor
    background.opacity: settings.opacity
    header.height: 40

    property int currentIndex: -1
    property alias currentItem: _tabView.currentItem
    readonly property QMLTermWidget currentTerminal: currentItem ? currentItem.terminal : null

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
        FishUI.DocumentTabBar {
            id: _tabbar
            anchors.fill: parent
            anchors.margins: FishUI.Units.smallSpacing / 2
            anchors.rightMargin: FishUI.Units.largeSpacing * 4

            currentIndex: _tabView.currentIndex
            model: _tabView.count

            onNewTabClicked: openNewTab()

            delegate: FishUI.DocumentTabButton {
                id: _tabBtn
                text: _tabView.contentModel.get(index).title
                height: _tabbar.height - FishUI.Units.smallSpacing / 2
                width: Math.min(_tabbar.width / _tabbar.count,
                                _tabBtn.contentWidth)

                checked: _tabView.currentIndex === index

                font.pointSize: 9
                font.family: "Noto Sans Mono"

                FishUI.ToolTip {
                    delay: 500
                    timeout: 5000
                    visible: _tabBtn.hovered
                    text: _tabBtn.text
                }

                onClicked: {
                    _tabView.currentIndex = index
                    _tabView.currentItem.forceActiveFocus()
                }

                onCloseClicked: {
                    root.closeProtection(index)
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent

        FishUI.TabView {
            id: _tabView
            Layout.fillWidth: true
            Layout.fillHeight: true
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
            const index = _tabView.contentModel.count
            const object = _tabView.addTab(component, {path: path})
            object.terminalClosed.connect(() => closeTab(index))
        }
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
