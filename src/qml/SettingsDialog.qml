import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import FishUI 1.0 as FishUI

FishUI.Window {
    id: control

    title: qsTr("Settings")

    readonly property int dialogWidth: 440
    readonly property int dialogHeight: _mainLayout.implicitHeight + header.height + FishUI.Units.largeSpacing * 1.5

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
    header.height: 44
    windowButtonsAlignment: Qt.AlignVCenter
    windowButtonsTopMargin: 0
    windowButtonsRightMargin: FishUI.Units.smallSpacing

    background.color: FishUI.Theme.backgroundColor

    readonly property color separatorColor: Qt.rgba(FishUI.Theme.textColor.r, FishUI.Theme.textColor.g,
                                                     FishUI.Theme.textColor.b, 0.08)

    component Card: FishUI.RoundedRectangle {
        default property alias rows: _rows.data

        Layout.fillWidth: true
        implicitHeight: _rows.implicitHeight
        radius: FishUI.Theme.bigRadius
        color: FishUI.Theme.secondBackgroundColor

        ColumnLayout {
            id: _rows
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 0
        }
    }

    // A label on the left, its control on the right, a hairline below unless last.
    component SettingRow: Item {
        property alias text: _label.text
        property bool last: false
        default property alias content: _content.data

        Layout.fillWidth: true
        implicitHeight: 44

        FishUI.Label {
            id: _label
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
        }

        Row {
            id: _content
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: FishUI.Units.largeSpacing
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            height: 1
            color: control.separatorColor
            visible: !parent.last
        }
    }

    // A value shown beside its slider, in a fixed width so the slider does not shift.
    component ValueLabel: FishUI.Label {
        anchors.verticalCenter: parent.verticalCenter
        width: 40
        horizontalAlignment: Text.AlignRight
        color: FishUI.Theme.disabledTextColor
        font.features: { "tnum": 1 }
    }

    headerItem: Item {
        FishUI.Label {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: (control.width - parent.width) / 2
            text: control.title
            font.pixelSize: 13
            font.weight: Font.Medium
        }
    }

    ColumnLayout {
        id: _mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: FishUI.Units.largeSpacing * 1.5
        anchors.rightMargin: FishUI.Units.largeSpacing * 1.5
        spacing: FishUI.Units.largeSpacing

        // The chosen font on the terminal's own colours.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 76
            radius: FishUI.Theme.bigRadius
            color: root.terminalBackground
            border.width: 1
            border.color: control.separatorColor
            clip: true

            Text {
                anchors.fill: parent
                anchors.margins: 14
                verticalAlignment: Text.AlignVCenter
                textFormat: Text.StyledText
                font.family: settings.fontName
                font.pointSize: settings.fontPointSize
                color: FishUI.Theme.darkMode ? "#E5E5E7" : "#2B2B30"
                text: {
                    const green = FishUI.Theme.darkMode ? "#5FD38D" : "#1F8A4C"
                    const blue = FishUI.Theme.darkMode ? "#5AA2F5" : "#1F6FD6"
                    return "<font color=\"" + green + "\">user@cutefish</font> "
                            + "<font color=\"" + blue + "\">~</font> $ ls<br>"
                            + "<font color=\"" + blue + "\">Documents&nbsp;&nbsp;Pictures</font>&nbsp;&nbsp;notes.txt"
                }
            }
        }

        Card {
            SettingRow {
                text: qsTr("Font")

                FishUI.ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 200
                    model: Fonts.families
                    currentIndex: Math.max(0, Fonts.families.indexOf(settings.fontName))

                    onActivated: settings.fontName = currentText
                }
            }

            SettingRow {
                text: qsTr("Font Size")

                FishUI.Slider {
                    id: fontSizeSlider
                    anchors.verticalCenter: parent.verticalCenter
                    width: 160
                    from: 5
                    to: 30
                    stepSize: 1
                    value: settings.fontPointSize

                    onMoved: settings.fontPointSize = fontSizeSlider.value
                }

                ValueLabel {
                    text: settings.fontPointSize + " pt"
                }
            }

            SettingRow {
                text: qsTr("Blinking Cursor")
                last: true

                FishUI.Switch {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: settings.blinkingCursor
                    onToggled: settings.blinkingCursor = checked
                }
            }
        }

        Card {
            SettingRow {
                text: qsTr("Opacity")

                FishUI.Slider {
                    id: transparencySlider
                    anchors.verticalCenter: parent.verticalCenter
                    width: 160
                    from: 0.1
                    to: 1.0
                    stepSize: 0.05
                    value: settings.opacity

                    onMoved: settings.opacity = transparencySlider.value
                }

                ValueLabel {
                    text: Math.round(settings.opacity * 100) + "%"
                }
            }

            SettingRow {
                text: qsTr("Window Blur")
                last: true

                FishUI.Switch {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: settings.blur
                    onToggled: settings.blur = checked
                }
            }
        }
    }
}
