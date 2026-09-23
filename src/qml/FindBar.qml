import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import FishUI 1.0 as FishUI

// Floats over a terminal while searching its output and history. Enter goes to
// the next older match, Shift+Enter to the next newer one. Drawn in the terminal's
// colors; the field itself has no frame, only the bar does.
Rectangle {
    id: control

    property var terminal
    property bool notFound: false

    // The color scheme's own colors.
    property color backgroundColor: "#1A1B26"
    property color textColor: "#C0CAF5"
    property color errorColor: "#F7768E"
    property color accentColor: "#7AA2F7"
    property bool darkMode: true

    readonly property color tint: darkMode ? "#FFFFFF" : "#323238"

    signal closed()

    width: 320
    height: 40
    radius: FishUI.Theme.bigRadius
    color: Qt.tint(backgroundColor, Qt.rgba(textColor.r, textColor.g, textColor.b, darkMode ? 0.1 : 0.06))
    border.width: 0.5
    border.pixelAligned: false
    border.color: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.12)
    visible: false

    function open(text) {
        if (text)
            _field.text = text

        visible = true
        _field.selectAll()
        _field.forceActiveFocus()
    }

    function close() {
        visible = false
        notFound = false
        terminal.clearFind()
        closed()
    }

    function search(forwards, fromSelection) {
        const text = _field.text
        if (text === "")
            notFound = false

        // Smart case: a capital letter in the query makes it case-sensitive.
        terminal.find(text, forwards, fromSelection, text !== text.toLowerCase())
    }

    Connections {
        target: control.terminal

        function onFindResult(found) {
            control.notFound = !found
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: FishUI.Units.largeSpacing
        anchors.rightMargin: 4
        spacing: 2

        T.TextField {
            id: _field
            Layout.fillWidth: true
            Layout.fillHeight: true
            verticalAlignment: TextInput.AlignVCenter
            renderType: FishUI.Theme.renderType
            selectByMouse: true
            color: control.notFound ? control.errorColor : control.textColor
            selectionColor: Qt.rgba(control.accentColor.r, control.accentColor.g, control.accentColor.b, 0.4)
            selectedTextColor: control.textColor
            placeholderText: qsTr("Find")

            Text {
                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                text: _field.placeholderText
                font: _field.font
                renderType: _field.renderType
                color: Qt.rgba(control.textColor.r, control.textColor.g, control.textColor.b, 0.45)
                visible: !_field.length && !_field.preeditText
            }

            onTextChanged: control.search(false, false)

            Keys.onReturnPressed: (event) => control.search(event.modifiers & Qt.ShiftModifier, true)
            Keys.onEnterPressed: (event) => control.search(event.modifiers & Qt.ShiftModifier, true)
            Keys.onEscapePressed: control.close()
        }

        FishUI.RoundImageButton {
            size: 28
            iconName: "chevron-up"
            iconSize: 16
            iconColor: control.textColor
            hoveredColor: Qt.rgba(control.tint.r, control.tint.g, control.tint.b, control.darkMode ? 0.12 : 0.11)
            pressedColor: Qt.rgba(control.tint.r, control.tint.g, control.tint.b, control.darkMode ? 0.06 : 0.21)
            onClicked: control.search(false, true)
        }

        FishUI.RoundImageButton {
            size: 28
            iconName: "chevron-down"
            iconSize: 16
            iconColor: control.textColor
            hoveredColor: Qt.rgba(control.tint.r, control.tint.g, control.tint.b, control.darkMode ? 0.12 : 0.11)
            pressedColor: Qt.rgba(control.tint.r, control.tint.g, control.tint.b, control.darkMode ? 0.06 : 0.21)
            onClicked: control.search(true, true)
        }

        FishUI.RoundImageButton {
            size: 28
            source: "qrc:/fishui/kit/images/" + (control.darkMode ? "dark/" : "light/") + "close.svg"
            hoveredColor: Qt.rgba(control.tint.r, control.tint.g, control.tint.b, control.darkMode ? 0.12 : 0.11)
            pressedColor: Qt.rgba(control.tint.r, control.tint.g, control.tint.b, control.darkMode ? 0.06 : 0.21)
            // The window buttons' artwork, at its own 24px grid.
            iconSize: 24
            image.smooth: false
            image.antialiasing: true
            onClicked: control.close()
        }
    }
}
