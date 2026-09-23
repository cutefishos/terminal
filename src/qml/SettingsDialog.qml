import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import FishUI 1.0 as FishUI

FishUI.Window {
    id: control

    title: qsTr("Settings")

    readonly property int dialogWidth: 400
    readonly property int dialogHeight: _mainLayout.implicitHeight + header.height + FishUI.Units.largeSpacing

    width: dialogWidth
    height: dialogHeight
    minimumWidth: dialogWidth
    maximumWidth: dialogWidth
    minimumHeight: dialogHeight
    maximumHeight: dialogHeight

    flags: Qt.Dialog | Qt.FramelessWindowHint
    modality: Qt.WindowModal
    visible: false
    minimizeButtonVisible: false

    background.color: FishUI.Theme.secondBackgroundColor

    headerItem: Item {
        FishUI.Label {
            anchors.left: parent.left
            anchors.leftMargin: FishUI.Units.largeSpacing
            anchors.verticalCenter: parent.verticalCenter
            text: control.title
            font.weight: Font.DemiBold
        }
    }

    GridLayout {
        id: _mainLayout
        anchors.fill: parent
        anchors.margins: FishUI.Units.largeSpacing
        anchors.topMargin: 0
        columns: 2
        columnSpacing: FishUI.Units.largeSpacing * 2
        rowSpacing: FishUI.Units.largeSpacing * 2

        FishUI.Label {
            text: qsTr("Font")
        }

        FishUI.ComboBox {
            id: fontsCombobox
            model: Fonts.families
            currentIndex: Math.max(0, Fonts.families.indexOf(settings.fontName))
            Layout.fillWidth: true

            onActivated: settings.fontName = currentText
        }

        FishUI.Label {
            text: qsTr("Font Size")
        }

        FishUI.Slider {
            id: fontSizeSlider
            Layout.fillWidth: true
            from: 5
            to: 30
            stepSize: 1
            value: settings.fontPointSize

            onMoved: settings.fontPointSize = fontSizeSlider.value
        }

        FishUI.Label {
            text: qsTr("Transparency")
        }

        FishUI.Slider {
            id: transparencySlider
            Layout.fillWidth: true
            from: 0.1
            to: 1.0
            stepSize: 0.05
            value: settings.opacity

            onMoved: settings.opacity = transparencySlider.value
        }

        FishUI.Label {
            text: qsTr("Window Blur")
        }

        FishUI.Switch {
            Layout.alignment: Qt.AlignRight
            checked: settings.blur
            onToggled: settings.blur = checked
        }
    }
}
