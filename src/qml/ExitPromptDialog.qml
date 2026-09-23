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

import QtQuick
import FishUI 1.0 as FishUI

FishUI.ConfirmDialog {
    id: control

    // What to do once confirmed: close a tab, the other tabs, or the window.
    property var pendingAction: null

    onAccepted: {
        if (pendingAction)
            pendingAction()
        pendingAction = null
    }
    onRejected: pendingAction = null

    showOnCompleted: false
    title: qsTr("Process is running, are you sure you want to quit?")
    cancelText: qsTr("Cancel")
    confirmText: qsTr("OK")
    destructive: true
}
