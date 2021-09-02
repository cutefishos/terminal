import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import FishUI 1.0 as FishUI

FishUI.Window {
    id: control

    width: 400
    height: 400

    maximumHeight: 400
    maximumWidth: 400
    minimumWidth: 400
    minimumHeight: 400

    visible: false

    ColumnLayout {
        id: _mainLayout
        anchors.fill: parent
        anchors.leftMargin: FishUI.Units.largeSpacing
        anchors.rightMargin: FishUI.Units.largeSpacing
        spacing: FishUI.Units.largeSpacing

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 45

            Rectangle {
                anchors.fill: parent
                color: FishUI.Theme.secondBackgroundColor
                radius: FishUI.Theme.smallRadius
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: FishUI.Units.largeSpacing
                anchors.rightMargin: FishUI.Units.largeSpacing

                Label {
                    text: qsTr("Font")
                }

                Item {
                    width: FishUI.Units.largeSpacing
                }

                ComboBox {
                    id: fontsCombobox
                    model: Fonts.families
                    Layout.fillHeight: true
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
            }
        }

        // Font size
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 45

            Rectangle {
                anchors.fill: parent
                color: FishUI.Theme.secondBackgroundColor
                radius: FishUI.Theme.smallRadius
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: FishUI.Units.largeSpacing
                anchors.rightMargin: FishUI.Units.largeSpacing

                Label {
                    text: qsTr("Font Size")
                }

                Item {
                    width: FishUI.Units.largeSpacing
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
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 45

            Rectangle {
                anchors.fill: parent
                color: FishUI.Theme.secondBackgroundColor
                radius: FishUI.Theme.smallRadius
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: FishUI.Units.largeSpacing
                anchors.rightMargin: FishUI.Units.largeSpacing

                Label {
                    text: qsTr("Transparency")
                }

                Item {
                    width: FishUI.Units.largeSpacing
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
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 45

            Rectangle {
                anchors.fill: parent
                color: FishUI.Theme.secondBackgroundColor
                radius: FishUI.Theme.smallRadius
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: FishUI.Units.largeSpacing
                anchors.rightMargin: FishUI.Units.smallSpacing

                Label {
                    text: qsTr("Window Blur")
                }

                Item {
                    Layout.fillWidth: true
                }

                Switch {
                    Layout.fillHeight: true
                    checked: settings.blur
                    onCheckedChanged: settings.blur = checked
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}