import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import FishUI 1.0 as FishUI

Window {
    id: control

    title: qsTr("Settings")

    width: 400
    height: _mainLayout.implicitHeight + FishUI.Units.largeSpacing * 4

    maximumHeight: _mainLayout.implicitHeight + FishUI.Units.largeSpacing * 4
    maximumWidth: 400
    minimumWidth: 400
    minimumHeight: _mainLayout.implicitHeight + FishUI.Units.largeSpacing * 4

    flags: Qt.Dialog
    modality: Qt.WindowModal

    visible: false

    Rectangle {
        anchors.fill: parent
        color: FishUI.Theme.secondBackgroundColor
    }

    GridLayout {
        id: _mainLayout
        anchors.fill: parent
        anchors.margins: FishUI.Units.largeSpacing
        columns: 2
        columnSpacing: FishUI.Units.largeSpacing * 2
        rowSpacing: FishUI.Units.largeSpacing * 2

        Label {
            text: qsTr("Font")
        }

        ComboBox {
            id: fontsCombobox
            model: Fonts.families
            // Layout.fillHeight: true
            Layout.fillWidth: true

            onCurrentTextChanged: {
                settings.fontName = currentText
            }

            Component.onCompleted: {
                for (var i = 0; i <= fontsCombobox.model.length; ++i) {
                    if (fontsCombobox.model[i] === settings.fontName) {
                        fontsCombobox.currentIndex = i
                        break
                    }
                }
            }
        }

        Label {
            text: qsTr("Font Size")
        }

        Slider {
            id: fontSizeSlider
            Layout.fillHeight: true
            Layout.fillWidth: true
            from: 5
            to: 30
            stepSize: 1

            Component.onCompleted: {
                fontSizeSlider.value = settings.fontPointSize
            }

            onValueChanged: settings.fontPointSize = fontSizeSlider.value
        }

        Label {
            text: qsTr("Transparency")
        }

        Slider {
            id: transparencySlider
            Layout.fillHeight: true
            Layout.fillWidth: true
            from: 0.1
            to: 1.0
            stepSize: 0.05

            Component.onCompleted: {
                transparencySlider.value = settings.opacity
            }

            onValueChanged: settings.opacity = transparencySlider.value
        }

        Label {
            text: qsTr("Window Blur")
        }

        Switch {
            Layout.alignment: Qt.AlignRight
            Layout.fillHeight: true
            checked: settings.blur
            onCheckedChanged: settings.blur = checked
        }
    }
}
