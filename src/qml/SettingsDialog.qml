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

    // Bundled schemes, by file name. The labels are fixed: a model that changes
    // after creation resets the combo box.
    readonly property var themeNames: ["Catppuccin-Latte", "Catppuccin-Mocha", "Dracula",
                                       "Gruvbox-Dark", "Gruvbox-Light", "Nord", "One-Dark", "One-Light",
                                       "Solarized", "SolarizedLight", "TokyoNight", "TokyoNight-Day",
                                       "Tomorrow-Night"]
    readonly property var themeLabels: ["Catppuccin Latte", "Catppuccin Mocha", "Dracula",
                                        "Gruvbox Dark", "Gruvbox Light", "Nord", "One Dark", "One Light",
                                        "Solarized Dark", "Solarized Light", "Tokyo Night", "Tokyo Night Day",
                                        "Tomorrow Night"]
    readonly property var schemeInfo: root.colorSchemeInfo

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
                color: control.schemeInfo.foreground ? control.schemeInfo.foreground : FishUI.Theme.textColor
                text: {
                    const colors = control.schemeInfo.colors || []
                    const green = colors.length ? colors[2] : FishUI.Theme.textColor
                    const blue = colors.length ? colors[4] : FishUI.Theme.textColor
                    return "<font color=\"" + green + "\">user@cutefish</font> "
                            + "<font color=\"" + blue + "\">~</font> $ ls<br>"
                            + "<font color=\"" + blue + "\">Documents&nbsp;&nbsp;Pictures</font>&nbsp;&nbsp;notes.txt"
                }
            }

            // The scheme's eight normal colours.
            Row {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 12
                spacing: 4

                Repeater {
                    model: control.schemeInfo.colors ? control.schemeInfo.colors.slice(0, 8) : []

                    Rectangle {
                        width: 10
                        height: 10
                        radius: 3
                        color: modelData
                        border.width: 1
                        border.color: Qt.rgba(0.5, 0.5, 0.5, 0.25)
                    }
                }
            }
        }

        Card {
            SettingRow {
                text: qsTr("Theme")

                FishUI.ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 200
                    model: control.themeLabels
                    currentIndex: Math.max(0, control.themeNames.indexOf(root.colorScheme))

                    onActivated: (index) => settings.colorScheme = control.themeNames[index]
                }
            }

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

        Card {
            SettingRow {
                text: qsTr("Cursor Shape")

                FishUI.ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 160
                    // In the order of the cursorShape values.
                    model: [qsTr("Block"), qsTr("Underline"), qsTr("I-Beam")]
                    currentIndex: settings.cursorShape

                    onActivated: (index) => settings.cursorShape = index
                }
            }

            SettingRow {
                text: qsTr("Scrollback")

                FishUI.ComboBox {
                    readonly property var lines: [1000, 10000, 100000, -1]

                    anchors.verticalCenter: parent.verticalCenter
                    width: 160
                    model: [qsTr("1,000 lines"), qsTr("10,000 lines"), qsTr("100,000 lines"), qsTr("Unlimited")]
                    currentIndex: Math.max(0, lines.indexOf(settings.scrollbackLines))

                    onActivated: (index) => settings.scrollbackLines = lines[index]
                }
            }

            SettingRow {
                text: qsTr("Flash on Bell")

                FishUI.Switch {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: settings.visualBell
                    onToggled: settings.visualBell = checked
                }
            }

            SettingRow {
                text: qsTr("Warn Before Pasting Multiple Lines")
                last: true

                FishUI.Switch {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: settings.confirmMultilinePaste
                    onToggled: settings.confirmMultilinePaste = checked
                }
            }
        }
    }
}
