import QtQuick
import FishUI 1.0 as FishUI

// Tabs laid out on a pill-shaped track in the title bar; the current tab is
// raised on the track and only it, or the hovered tab, shows a close button.
// Tabs are dragged to reorder them, dragged off the bar into a window of their
// own, and renamed from their context menu.
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
    // A tab dragged in from another window takes a slot of its own; one
    // pulled out gives its slot up before it leaves the view.
    readonly property int slotCount: count + (incoming ? 1 : 0) - (leavingPage ? 1 : 0)
    readonly property real tabWidth: slotCount > 0 ? Math.max(minimumTabWidth, Math.min(maximumTabWidth, width / slotCount)) : 0
    // What the tabs are drawn at: tabWidth, eased while they slide.
    property real slotWidth: tabWidth

    Behavior on slotWidth {
        enabled: control.sliding
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }

    // A tab dragged over the bar from another window: drawn under the pointer
    // at `incomingX`, with the others making room where it would drop.
    property bool incoming: false
    property string incomingTitle: ""
    property real incomingX: 0
    readonly property int incomingIndex: incoming
            ? Math.max(0, Math.min(count, Math.round((incomingX + _flick.contentX) / Math.max(1, tabWidth) - 0.5)))
            : -1

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
    // The page of a tab pulled off the bar, hidden until the view lets it go.
    property var leavingPage: null
    // Where a tab dragged in was drawn when it was dropped, in slots; the tab
    // made for it glides from there.
    property real arrivalSlot: NaN
    // Tabs glide to their slots while a tab is dragged, arrives or leaves;
    // opening or closing a tab places the rest at once.
    readonly property bool sliding: dragIndex >= 0 || incoming || _settle.running
    // The tab whose title is being edited.
    property int editingIndex: -1
    // How far above or below the bar a dragged tab comes off it.
    readonly property real detachDistance: 16

    signal closeRequested(int index)
    signal closeOthersRequested(int index)
    signal moveRequested(int from, int to)
    // `position` is the pointer, in the window.
    signal detachRequested(int index, point position)
    signal renameRequested(int index, string title)

    implicitHeight: 30

    // Where a tab sits while another is dragged: the dragged one is taken out
    // of the order and put back at dropIndex, so the rest slide aside.
    function slotFor(index) {
        if (incoming)
            return index + (index >= incomingIndex ? 1 : 0)
        if (leavingPage) {
            const leaving = _tabs.indexOf(leavingPage)
            return index - (leaving >= 0 && index > leaving ? 1 : 0)
        }
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

    // Every edge of a drag restarts the settle timer first: bindings that
    // follow the edge would otherwise still see `sliding` false and jump.
    function beginIncoming(title, x) {
        _settle.restart()
        incomingTitle = title
        incomingX = x
        incoming = true
    }

    // Dropped, the tab glides from under the pointer into its slot; gone,
    // the others slide back.
    function endIncoming(dropped) {
        _settle.restart()
        arrivalSlot = dropped ? _ghost.x / slotWidth : NaN
        incoming = false
    }

    Timer {
        id: _settle
        interval: 150
    }

    // The view's pages, one element each. The Repeater keeps a tab's delegate
    // while others come, go or move, so each tab can glide from where it is.
    ListModel {
        id: _tabs

        function indexOf(page) {
            for (let i = 0; i < count; ++i) {
                if (get(i).page === page)
                    return i
            }

            return -1
        }
    }

    function syncTabs() {
        const pages = view ? view.contentChildren : []
        for (let i = 0; i < pages.length; ++i) {
            const at = _tabs.indexOf(pages[i])
            if (at < 0)
                _tabs.insert(i, { page: pages[i] })
            else if (at !== i)
                _tabs.move(at, i, 1)
        }

        if (_tabs.count > pages.length)
            _tabs.remove(pages.length, _tabs.count - pages.length)
        if (leavingPage && _tabs.indexOf(leavingPage) < 0)
            leavingPage = null
    }

    onViewChanged: syncTabs()
    Component.onCompleted: syncTabs()

    function rename(index) {
        editingIndex = index
        ensureVisible(index)
    }

    Connections {
        target: control.view
        function onCurrentIndexChanged() { control.ensureVisible(control.view.currentIndex) }
        function onContentChildrenChanged() { control.syncTabs() }
    }

    Flickable {
        id: _flick
        anchors.fill: parent
        clip: true
        contentWidth: control.tabWidth * control.slotCount
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentWidth > width && control.dragIndex < 0

        Rectangle {
            width: _flick.contentWidth
            height: _flick.height
            radius: height / 2
            color: control.trackColor

            Behavior on width {
                enabled: control.sliding
                NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
            }
        }

        Repeater {
            model: _tabs

            delegate: Item {
                id: tab

                readonly property bool selected: control.view.currentIndex === index
                readonly property bool hovered: _mouseArea.containsMouse || _closeArea.containsMouse
                readonly property bool dragged: control.dragIndex === index
                readonly property var page: model.page
                readonly property real slot: control.slotFor(index)
                // Eases to zero: the tab is drawn at slot + slotShift, so it
                // glides from where it was drawn when its slot changes.
                property real slotShift: 0
                property real lastSlot: 0
                // Let go, or dropped in from another window, and not in its slot yet.
                property bool landing: false

                x: dragged ? control.dragX : (slot + slotShift) * control.slotWidth
                z: dragged || landing ? 2 : selected ? 1 : 0
                width: control.slotWidth
                height: control.height
                // Not `visible`: hiding the item would cancel the press the window drag goes on with.
                opacity: page !== control.leavingPage ? 1 : 0

                function glideFrom(drawnSlot) {
                    if (control.sliding) {
                        slotShift = drawnSlot - slot
                        _glide.restart()
                    } else {
                        _glide.stop()
                        slotShift = 0
                        landing = false
                    }
                    lastSlot = slot
                }

                onSlotChanged: glideFrom(lastSlot + slotShift)
                onDraggedChanged: {
                    if (!dragged) {
                        landing = true
                        glideFrom(control.dragX / control.slotWidth)
                    }
                }

                Component.onCompleted: {
                    const arrival = control.arrivalSlot
                    control.arrivalSlot = NaN
                    landing = !isNaN(arrival)
                    glideFrom(landing ? arrival : slot)
                }

                NumberAnimation {
                    id: _glide
                    target: tab
                    property: "slotShift"
                    to: 0
                    duration: 150
                    easing.type: Easing.OutCubic
                    onFinished: tab.landing = false
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: height / 2
                    color: tab.selected || tab.dragged || tab.landing ? control.selectedColor
                                                                      : tab.hovered ? control.hoverColor : "transparent"
                    // Light mode raises a white pill on a pale track; a hairline keeps its edge.
                    border.width: (tab.selected || tab.dragged || tab.landing) && !control.darkMode ? 1 : 0
                    border.color: Qt.rgba(0, 0, 0, 0.06)
                }

                // Between two resting tabs only; a raised or hovered pill has its own edge.
                Rectangle {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: 14
                    color: control.separatorColor
                    visible: !control.sliding
                             && index < control.count - 1
                             && !tab.selected && !tab.hovered
                             && control.view.currentIndex !== index + 1
                }

                MouseArea {
                    id: _mouseArea

                    property real pressX: 0
                    property real pressY: 0
                    property bool didDrag: false
                    // Off the bar: the rest of the press belongs to the window drag.
                    property bool detached: false

                    anchors.fill: parent
                    hoverEnabled: true
                    // The title bar would otherwise take a sideways drag to move the window.
                    preventStealing: true
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton

                    onPressed: (mouse) => {
                        didDrag = false
                        detached = false
                        pressX = mapToItem(_flick.contentItem, mouse.x, mouse.y).x
                        pressY = mapToItem(control, mouse.x, mouse.y).y
                    }

                    onPositionChanged: (mouse) => {
                        if (!(pressedButtons & Qt.LeftButton) || control.editingIndex === index || detached)
                            return

                        const x = mapToItem(_flick.contentItem, mouse.x, mouse.y).x
                        const y = mapToItem(control, mouse.x, mouse.y).y
                        if (!didDrag && (Math.abs(x - pressX) > 8 || Math.abs(y - pressY) > 8)) {
                            didDrag = true
                            _settle.restart()
                            control.dragIndex = index
                            control.dropIndex = index
                        }

                        if (didDrag && (y < -control.detachDistance
                                        || y > control.height + control.detachDistance)) {
                            didDrag = false
                            detached = true
                            // The rest close up behind it before the view lets it go.
                            _settle.restart()
                            control.leavingPage = tab.page
                            control.dragIndex = -1
                            control.dropIndex = -1
                            control.detachRequested(index, mapToItem(null, mouse.x, mouse.y))
                            return
                        }

                        if (didDrag) {
                            const limit = Math.max(0, _flick.contentWidth - control.tabWidth)
                            control.dragX = Math.max(0, Math.min(limit, index * control.tabWidth + x - pressX))
                            control.dropIndex = Math.max(0, Math.min(control.count - 1,
                                                                     Math.round(control.dragX / control.tabWidth)))
                        }
                    }

                    onReleased: {
                        // The view kept the tab after all.
                        if (detached)
                            control.leavingPage = null
                        if (!didDrag)
                            return

                        const from = control.dragIndex
                        const to = control.dropIndex
                        // The tab glides from under the pointer into its slot.
                        _settle.restart()
                        control.dragIndex = -1
                        control.dropIndex = -1
                        control.moveRequested(from, to)
                    }

                    onCanceled: {
                        if (detached)
                            control.leavingPage = null
                        _settle.restart()
                        control.dragIndex = -1
                        control.dropIndex = -1
                    }

                    onClicked: (mouse) => {
                        if (didDrag || detached)
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
                    width: 16
                    height: 16
                    anchors.right: parent.right
                    anchors.rightMargin: 8
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

                    FishUI.Icon {
                        anchors.fill: parent
                        name: "close"
                        color: control.textColor
                        opacity: _closeArea.containsMouse ? 1 : 0.6
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

        Item {
            id: _ghost
            visible: control.incoming
            x: Math.max(0, Math.min(_flick.contentWidth - width,
                                    control.incomingX + _flick.contentX - width / 2))
            z: 2
            width: control.slotWidth
            height: control.height

            FishUI.RoundedRectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: height / 2
                continuousCorners: false
                color: control.selectedColor
                border.width: 1
                border.color: control.separatorColor
            }

            FishUI.Label {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: control.incomingTitle
                elide: Text.ElideMiddle
                font.pixelSize: 12
                font.weight: Font.Medium
                color: control.textColor
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
