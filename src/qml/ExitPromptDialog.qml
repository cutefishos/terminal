// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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
