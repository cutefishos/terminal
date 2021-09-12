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
    background.opacity: settings.opacity
    header.height: 45

    property int currentIndex: -1
    property alias currentItem: _view.currentItem
    readonly property QMLTermWidget currentTerminal: currentItem ? currentItem.terminal : null

    GlobalSettings { id: settings }
    ObjectModel { id: tabsModel }

    ExitPromptDialog {
        id: exitPrompt
        onOkBtnClicked: Qt.quit()
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
        if (!root.isMaximized) {
            settings.width = root.width
            settings.height = root.height
        }

        // Exit prompt.
        for (var i = 0; i < tabsModel.count; ++i) {
            var obj = tabsModel.get(i)
            if (obj.session.hasActiveProcess) {
                exitPrompt.visible = true
                close.accepted = false
                break
            }
        }
    }

    headerItem: Item {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: FishUI.Units.smallSpacing
            anchors.rightMargin: FishUI.Units.smallSpacing
            anchors.topMargin: FishUI.Units.smallSpacing
            anchors.bottomMargin: FishUI.Units.smallSpacing
            spacing: FishUI.Units.smallSpacing

            ListView {
                id: _tabView
                model: tabsModel.count
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: ListView.Horizontal
                spacing: FishUI.Units.smallSpacing
                currentIndex: _view.currentIndex
                highlightFollowsCurrentItem: true
                maximumFlickVelocity: 900
                highlightMoveDuration: 0
                highlightResizeDuration: 0
                interactive: false
                clip: true

                highlight: Rectangle {
                    color: FishUI.Theme.highlightColor
                    opacity: 1
                    border.width: 0
                    radius: FishUI.Theme.smallRadius
                }

                delegate: Item {
                    id: _tabItem
                    height: root.header.height - FishUI.Units.largeSpacing
                    width: Math.min(_layout.implicitWidth + FishUI.Units.largeSpacing,
                                    _tabView.width / _tabView.count - FishUI.Units.smallSpacing)

                    property bool isCurrent: _tabView.currentIndex === index
                    property var text: tabsModel.get(index).title

                    MouseArea {
                        id: _mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: _view.currentIndex = index
                    }

                    Rectangle {
                        id: hoveredRect
                        anchors.fill: parent
                        color: _mouseArea.containsMouse ? FishUI.Theme.textColor : "transparent"
                        opacity: _mouseArea.pressed ? 0.1 : 0.05
                        border.width: 0
                        radius: FishUI.Theme.smallRadius
                    }

                    RowLayout {
                        id: _layout
                        anchors.fill: parent
                        anchors.leftMargin: FishUI.Units.smallSpacing
                        anchors.rightMargin: FishUI.Units.smallSpacing
                        spacing: 0

                        Label {
                            id: _tabName
                            Layout.fillWidth: true
                            text: _tabItem.text ? _tabItem.text : `Tab #${index + 1}`
                            elide: Label.ElideRight
                            font.pointSize: 9
                            font.family: "Noto Sans Mono"
                            color: isCurrent ? FishUI.Theme.highlightedTextColor : FishUI.Theme.textColor
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        FishUI.RoundImageButton {
                            Layout.preferredHeight: 24
                            Layout.preferredWidth: 24
                            size: 24
                            source: "qrc:/images/" + (FishUI.Theme.darkMode || isCurrent ? "dark/" : "light/") + "close.svg"
                            onClicked: closeTab(index)
                        }
                    }
                }
            }

            FishUI.RoundImageButton {
                source: "qrc:/images/" + (FishUI.Theme.darkMode ? "dark/" : "light/") + "add.svg"
                onClicked: root.openNewTab()
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }


    ListView {
        id: _view
        anchors.fill: parent
        clip: true
        focus: true
        orientation: ListView.Horizontal
        model: tabsModel
        snapMode: ListView.SnapOneItem
        spacing: 0
        highlightFollowsCurrentItem: true
        highlightMoveDuration: 0
        highlightResizeDuration: 0
        highlightRangeMode: ListView.StrictlyEnforceRange
        preferredHighlightBegin: 0
        preferredHighlightEnd: width
        highlight: Item {}
        highlightMoveVelocity: -1
        highlightResizeVelocity: -1
        onMovementEnded: _view.currentIndex = indexAt(contentX, contentY)
        onCurrentItemChanged: {
            if (currentItem)
                currentItem.forceActiveFocus()
        }
        interactive: false
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
            const object = component.createObject(tabsModel, {'path': path})
            tabsModel.append(object)
            const index = tabsModel.count - 1
            _view.currentIndex = index
            object.terminalClosed.connect(() => closeTab(index))
        }
    }

    function closeTab(index) {
        tabsModel.remove(index)

        if (index === tabsModel.count) {
            _view.currentIndex = tabsModel.count - 1
        } else if (index === _view.currentIndex) {
            // Reion: Need to reset index.
            _view.currentIndex = -1
            _view.currentIndex = index
        }

        if (tabsModel.count == 0)
            Qt.quit()
    }

    function closeCurrentTab() {
        closeTab(_view.currentIndex)
    }

    function toggleTab() {
        var nextIndex = _view.currentIndex
        ++nextIndex
        if (nextIndex > tabsModel.count - 1)
            nextIndex = 0

        _view.currentIndex = nextIndex
    }
}
