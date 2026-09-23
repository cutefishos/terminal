import QtQuick
import FishUI 1.0 as FishUI

// Tabs laid out on a pill-shaped track in the title bar; the current tab is
// raised on the track and only it, or the hovered tab, shows a close button.
// Tabs are dragged to reorder them and renamed from their context menu.
Item {
    id: control

    property var view
    // The surface the bar sits on; set when it is painted in the app's own colours.
    property bool darkMode: FishUI.Theme.darkMode
    property color textColor: FishUI.Theme.textColor

    readonly property int count: view ? view.count : 0
    readonly property real maximumTabWidth: 220
    // Below this the bar scrolls instead of squeezing the titles away.
    readonly property real minimumTabWidth: 96
    readonly property real tabWidth: count > 0 ? Math.max(minimumTabWidth, Math.min(maximumTabWidth, width / count)) : 0

    readonly property color trackColor: darkMode ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.045)
    // Light schemes are not always white; a translucent pill lifts off any of them.
    readonly property color selectedColor: darkMode ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(1, 1, 1, 0.8)
    readonly property color hoverColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.05)
    readonly property color separatorColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.12)
    readonly property color dimTextColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.6)

    // The tab being dragged, the slot it would drop into, and where it is drawn.
    property int dragIndex: -1
    property int dropIndex: -1
    property real dragX: 0
    // The tab whose title is being edited.
    property int editingIndex: -1

    signal closeRequested(int index)
    signal closeOthersRequested(int index)
    signal moveRequested(int from, int to)
    signal renameRequested(int index, string title)

    implicitHeight: 30

    // Where a tab sits while another is dragged: the dragged one is taken out
    // of the order and put back at dropIndex, so the rest slide aside.
    function slotFor(index) {
        if (dragIndex < 0 || index === dragIndex)
            return index
        const without = index - (index > dragIndex ? 1 : 0)
        return without + (without >= dropIndex ? 1 : 0)
    }

    function ensureVisible(index) {
        if (index < 0 || index >= count)
            return
        const left = index * tabWidth
        const limit = Math.max(0, _flick.contentWidth - _flick.width)
        _flick.contentX = Math.max(0, Math.min(limit, left < _flick.contentX ? left
                                                      : left + tabWidth > _flick.contentX + _flick.width
                                                        ? left + tabWidth - _flick.width : _flick.contentX))
    }

    function rename(index) {
        editingIndex = index
        ensureVisible(index)
    }

    Connections {
        target: control.view
        function onCurrentIndexChanged() { control.ensureVisible(control.view.currentIndex) }
    }

    Flickable {
        id: _flick
        anchors.fill: parent
        clip: true
        contentWidth: control.tabWidth * control.count
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentWidth > width && control.dragIndex < 0

        Rectangle {
            width: _flick.contentWidth
            height: _flick.height
            radius: height / 2
            color: control.trackColor
        }

        Repeater {
            model: control.count

            delegate: Item {
                id: tab

                readonly property bool selected: control.view.currentIndex === index
                readonly property bool hovered: _mouseArea.containsMouse || _closeArea.containsMouse
                readonly property bool dragged: control.dragIndex === index
                readonly property var page: control.view.contentModel.get(index)

                x: dragged ? control.dragX : control.slotFor(index) * control.tabWidth
                z: dragged ? 2 : selected ? 1 : 0
                width: control.tabWidth
                height: control.height

                // Only the tabs sliding aside animate; a drop places them at once.
                Behavior on x {
                    enabled: control.dragIndex >= 0 && !tab.dragged
                    NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: height / 2
                    color: tab.selected || tab.dragged ? control.selectedColor
                                                       : tab.hovered ? control.hoverColor : "transparent"
                    // Light mode raises a white pill on a pale track; a hairline keeps its edge.
                    border.width: (tab.selected || tab.dragged) && !control.darkMode ? 1 : 0
                    border.color: Qt.rgba(0, 0, 0, 0.06)
                }

                // Between two resting tabs only; a raised or hovered pill has its own edge.
                Rectangle {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: 14
                    color: control.separatorColor
                    visible: control.dragIndex < 0
                             && index < control.count - 1
                             && !tab.selected && !tab.hovered
                             && control.view.currentIndex !== index + 1
                }

                MouseArea {
                    id: _mouseArea

                    property real pressX: 0
                    property bool didDrag: false

                    anchors.fill: parent
                    hoverEnabled: true
                    // The title bar would otherwise take a sideways drag to move the window.
                    preventStealing: true
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton

                    onPressed: (mouse) => {
                        didDrag = false
                        pressX = mapToItem(_flick.contentItem, mouse.x, mouse.y).x
                    }

                    onPositionChanged: (mouse) => {
                        if (!(pressedButtons & Qt.LeftButton) || control.editingIndex === index)
                            return

                        const x = mapToItem(_flick.contentItem, mouse.x, mouse.y).x
                        if (!didDrag && Math.abs(x - pressX) > 8) {
                            didDrag = true
                            control.dragIndex = index
                            control.dropIndex = index
                        }

                        if (didDrag) {
                            const limit = Math.max(0, _flick.contentWidth - control.tabWidth)
                            control.dragX = Math.max(0, Math.min(limit, index * control.tabWidth + x - pressX))
                            control.dropIndex = Math.max(0, Math.min(control.count - 1,
                                                                     Math.round(control.dragX / control.tabWidth)))
                        }
                    }

                    onReleased: {
                        if (!didDrag)
                            return

                        const from = control.dragIndex
                        const to = control.dropIndex
                        // Cleared first: the tabs already sit in their final slots.
                        control.dragIndex = -1
                        control.dropIndex = -1
                        control.moveRequested(from, to)
                    }

                    onCanceled: {
                        control.dragIndex = -1
                        control.dropIndex = -1
                    }

                    onClicked: (mouse) => {
                        if (didDrag)
                            return

                        if (mouse.button === Qt.MiddleButton) {
                            control.closeRequested(index)
                        } else if (mouse.button === Qt.RightButton) {
                            control.openMenu(index)
                        } else {
                            control.view.currentIndex = index
                            control.view.currentItem.forceActiveFocus()
                        }
                    }
                }

                FishUI.Label {
                    anchors.fill: parent
                    anchors.leftMargin: 28
                    anchors.rightMargin: 28
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: tab.page ? tab.page.title : ""
                    elide: Text.ElideMiddle
                    font.pixelSize: 12
                    font.weight: tab.selected ? Font.Medium : Font.Normal
                    color: tab.selected ? control.textColor : control.dimTextColor
                    visible: control.editingIndex !== index
                }

                TextInput {
                    id: _titleEditor
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: control.textColor
                    selectionColor: Qt.rgba(control.textColor.r, control.textColor.g, control.textColor.b, 0.25)
                    selectedTextColor: control.textColor
                    clip: true
                    visible: control.editingIndex === index

                    onVisibleChanged: {
                        if (visible) {
                            text = tab.page ? tab.page.title : ""
                            selectAll()
                            forceActiveFocus()
                        }
                    }

                    // An empty title goes back to following the shell.
                    onAccepted: {
                        control.editingIndex = -1
                        control.renameRequested(index, text)
                    }
                    Keys.onEscapePressed: {
                        control.editingIndex = -1
                        control.view.currentItem.forceActiveFocus()
                    }
                    onActiveFocusChanged: {
                        if (!activeFocus && control.editingIndex === index)
                            control.editingIndex = -1
                    }
                }

                Item {
                    width: 18
                    height: 18
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    opacity: (tab.selected || tab.hovered) && control.dragIndex < 0 && control.editingIndex !== index ? 1 : 0

                    Rectangle {
                        anchors.fill: parent
                        radius: height / 2
                        color: _closeArea.pressed ? Qt.rgba(control.textColor.r, control.textColor.g,
                                                            control.textColor.b, 0.16)
                                                  : control.hoverColor
                        visible: _closeArea.containsMouse
                    }

                    // Drawn at the tab's scale: the window close artwork is sized for a 32px button.
                    Repeater {
                        model: [45, -45]

                        Rectangle {
                            anchors.centerIn: parent
                            width: 8
                            height: 1.2
                            radius: height / 2
                            rotation: modelData
                            antialiasing: true
                            color: control.textColor
                            opacity: _closeArea.containsMouse ? 1 : 0.6
                        }
                    }

                    MouseArea {
                        id: _closeArea
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: parent.opacity > 0
                        onClicked: control.closeRequested(index)
                    }
                }
            }
        }
    }

    // A window of its own, so it is made on the first right click.
    property QtObject _menu: null
    property int _menuIndex: -1

    function openMenu(index) {
        _menuIndex = index
        if (!_menu)
            _menu = _menuComponent.createObject(control)
        _menu.popup()
    }

    Component {
        id: _menuComponent

        FishUI.DesktopMenu {
            FishUI.MenuItem {
                text: qsTr("Rename Tab…")
                onTriggered: control.rename(control._menuIndex)
            }

            FishUI.MenuSeparator {}

            FishUI.MenuItem {
                text: qsTr("Close Tab")
                onTriggered: control.closeRequested(control._menuIndex)
            }

            FishUI.MenuItem {
                text: qsTr("Close Other Tabs")
                visible: control.count > 1
                onTriggered: control.closeOthersRequested(control._menuIndex)
            }
        }
    }
}
