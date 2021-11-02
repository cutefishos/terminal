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

import QtQuick 2.12
import QtQml.Models 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.0

import Cutefish.TermWidget 1.0
import FishUI 1.0 as FishUI

FishUI.Window {
    id: root
    minimumWidth: 400
    minimumHeight: 300
    width: settings.width
    height: settings.height
    title: currentItem && currentItem.terminal ? currentItem.terminal.session.title : ""

    background.color: FishUI.Theme.backgroundColor
    background.opacity: root.compositing ? settings.opacity : 1
    header.height: 40

    property int currentIndex: -1
    property alias currentItem: _tabView.currentItem
    readonly property QMLTermWidget currentTerminal: currentItem ? currentItem.terminal : null

    GlobalSettings { id: settings }
    ObjectModel { id: tabsModel }

    ExitPromptDialog {
        id: exitPrompt

        property var index: -1

        onOkBtnClicked: {
            if (index != -1) {
                closeTab(index)
            } else {
                onOkBtnClicked: Qt.quit()
            }
        }
    }

    SettingsDialog {
        id: settingsDialog
    }

    FishUI.WindowBlur {
        view: root
        geometry: Qt.rect(root.x, root.y, root.width, root.height)
        windowRadius: root.background.radius
        enabled: settings.blur
    }

    onClosing: {
        if (!root.isMaximized && !root.isFullScreen) {
            settings.width = root.width
            settings.height = root.height
        }

        // Exit prompt.
        for (var i = 0; i < tabsModel.count; ++i) {
            var obj = tabsModel.get(i)
            if (obj.session.hasActiveProcess) {
                exitPrompt.index = -1
                exitPrompt.visible = true
                close.accepted = false
                break
            }
        }
    }

    headerItem: Item {
        FishUI.TabBar {
            id: _tabbar
            anchors.fill: parent
            anchors.margins: FishUI.Units.smallSpacing / 2
            anchors.rightMargin: FishUI.Units.largeSpacing * 2

            currentIndex : _tabView.currentIndex

            onNewTabClicked: openNewTab()

            Repeater {
                id: _repeater
                model: _tabView.count

                FishUI.TabButton {
                    text: _tabView.contentModel.get(index).title
                    implicitHeight: parent.height
                    implicitWidth: _repeater.count === 1 ? 200
                                                         : parent.width / _repeater.count

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000

                    checked: _tabView.currentIndex === index

                    font.pointSize: 9
                    font.family: "Noto Sans Mono"

                    ToolTip.visible: hovered
                    ToolTip.text: _tabView.contentModel.get(index).title

                    onClicked: {
                        _tabView.currentIndex = index
                        _tabView.currentItem.forceActiveFocus()
                    }

                    onCloseClicked: {
                        _tabView.closeTab(index)
                    }
                }
            }
        }
    }


    FishUI.TabView {
        id: _tabView
        anchors.fill: parent
    }

    Component.onCompleted: {
        openTab("$PWD")
    }

    function openNewTab() {
        if (currentTerminal) {
            openTab(currentTerminal.session.currentDir)
        } else {
            openTab("$HOME")
        }
    }

    function openTab(path) {
        if (tabsModel.count > 7)
            return

        const component = Qt.createComponent("Terminal.qml");
        if (component.status === Component.Ready) {
//            const object = component.createObject(tabsModel, {'path': path})
//            tabsModel.append(object)
//            const index = tabsModel.count - 1
//            _view.currentIndex = index
//            object.terminalClosed.connect(() => closeTab(index))

            _tabView.addTab(component, {})
        }
    }

    function closeProtection(index) {
        var obj = tabsModel.get(index)
        if (obj.session.hasActiveProcess) {
            exitPrompt.index = index
            exitPrompt.visible = true
            return
        }

        closeTab(index)
    }

    function closeTab(index) {
        tabsModel.remove(index)

        if (index === tabsModel.count) {
            _tabView.currentIndex = tabsModel.count - 1
        } else if (index === _tabView.currentIndex) {
            // Reion: Need to reset index.
            _tabView.currentIndex = -1
            _tabView.currentIndex = index
        }

        if (tabsModel.count == 0)
            Qt.quit()
    }

    function closeCurrentTab() {
        closeProtection(_tabView.currentIndex)
    }

    function toggleTab() {
        var nextIndex = _tabView.currentIndex
        ++nextIndex
        if (nextIndex > tabsModel.count - 1)
            nextIndex = 0

        _tabView.currentIndex = nextIndex
    }
}
