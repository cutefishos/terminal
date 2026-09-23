import QtQuick
import FishUI 1.0 as FishUI

// Tabs laid out on a pill-shaped track in the title bar; the current tab is
// raised on the track and only it, or the hovered tab, shows a close button.
Item {
    id: control

    property var view
    // The surface the bar sits on; set when it is painted in the app's own colours.
    property bool darkMode: FishUI.Theme.darkMode
    property color textColor: FishUI.Theme.textColor

    readonly property int count: view ? view.count : 0
    readonly property real maximumTabWidth: 220
    readonly property real tabWidth: count > 0 ? Math.min(maximumTabWidth, width / count) : 0

    readonly property color trackColor: darkMode ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.045)
    // Light schemes are not always white; a translucent pill lifts off any of them.
    readonly property color selectedColor: darkMode ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(1, 1, 1, 0.8)
    readonly property color hoverColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.05)
    readonly property color separatorColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.12)
    readonly property color dimTextColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.6)

    signal closeRequested(int index)

    implicitHeight: 30

    Rectangle {
        width: control.tabWidth * control.count
        height: parent.height
        radius: height / 2
        color: control.trackColor
    }

    Repeater {
        model: control.count

        delegate: Item {
            id: tab

            readonly property bool selected: control.view.currentIndex === index
            readonly property bool hovered: _mouseArea.containsMouse || _closeArea.containsMouse
            readonly property var page: control.view.contentModel.get(index)

            x: index * control.tabWidth
            width: control.tabWidth
            height: control.height

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: height / 2
                color: tab.selected ? control.selectedColor
                                    : tab.hovered ? control.hoverColor : "transparent"
                // Light mode raises a white pill on a pale track; a hairline keeps its edge.
                border.width: tab.selected && !control.darkMode ? 1 : 0
                border.color: Qt.rgba(0, 0, 0, 0.06)
            }

            // Between two resting tabs only; a raised or hovered pill has its own edge.
            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: 14
                color: control.separatorColor
                visible: index < control.count - 1
                         && !tab.selected && !tab.hovered
                         && control.view.currentIndex !== index + 1
            }

            MouseArea {
                id: _mouseArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton

                onClicked: (mouse) => {
                    if (mouse.button === Qt.MiddleButton) {
                        control.closeRequested(index)
                        return
                    }

                    control.view.currentIndex = index
                    control.view.currentItem.forceActiveFocus()
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
            }

            Item {
                width: 24
                height: 24
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                opacity: tab.selected || tab.hovered ? 1 : 0

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: height / 2
                    color: _closeArea.pressed ? Qt.rgba(control.textColor.r, control.textColor.g,
                                                        control.textColor.b, 0.16)
                                              : control.hoverColor
                    visible: _closeArea.containsMouse
                }

                // The window close artwork, at its own 24px grid so it stays crisp.
                Image {
                    anchors.fill: parent
                    source: "qrc:/fishui/kit/images/" + (control.darkMode ? "dark/" : "light/") + "close.svg"
                    sourceSize: Qt.size(width, height)
                    smooth: false
                    antialiasing: true
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
}
