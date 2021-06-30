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
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import FishUI 1.0 as FishUI

Dialog {
    id: control

    modal: true

    width: _mainLayout.implicitWidth + FishUI.Units.largeSpacing * 4
    height: _mainLayout.implicitHeight + FishUI.Units.largeSpacing * 4

    x: (parent.width - control.width) / 2
    y: (parent.height - control.height) / 2

    signal okBtnClicked

    Rectangle {
        anchors.fill: parent
        color: FishUI.Theme.backgroundColor
    }

    ColumnLayout {
        id: _mainLayout
        anchors.fill: parent

        Label {
            text: qsTr("Process is running, are you sure you want to quit?")
            Layout.alignment: Qt.AlignHCenter
        }

        DialogButtonBox {
            Layout.alignment: Qt.AlignHCenter

            Button {
                text: qsTr("Cancel")
                onClicked: control.visible = false
            }

            Button {
                text: qsTr("OK")
                onClicked: {
                    control.visible = false
                    control.okBtnClicked()
                }
            }
        }
    }
}
