/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
import QtQuick 2.15

import Muse.Ui 1.0
import Muse.UiComponents 1.0
import MuseScore.Inspector 1.0

import "../../../common"

Column {
    id: root

    property alias offset: offsets.propertyItem

    property bool isSnappedToGrid: false
    property real horizontalGridSizeSpatium: 0.0
    property real verticalGridSizeSpatium: 0.0
    property alias isVerticalOffsetAvailable: offsets.isVerticalOffsetAvailable

    property NavigationPanel navigationPanel: null
    property int navigationRowStart: 0
    property int navigationRowEnd: verticalGridSizeControl.navigation.row

    signal snapToGridToggled(var snap)
    signal horizontalGridSizeSpatiumEdited(var spatium)
    signal verticalGridSizeSpatiumEdited(var spatium)

    height: implicitHeight
    width: parent.width

    spacing: 12

    OffsetSection {
        id: offsets

        navigationPanel: root.navigationPanel
        navigationRowStart: root.navigationRowStart
    }

    CheckBox {
        id: snapToGridCheckbox

        width: parent.width

        navigation.name: "Snap to grid"
        navigation.panel: root.navigationPanel
        navigation.row: offsets.navigationRowEnd + 1

        text: qsTrc("inspector", "Snap to grid")

        checked: root.isSnappedToGrid

        onClicked: {
            root.snapToGridToggled(!checked)
        }
    }

    StyledTextLabel {
        text: qsTrc("notation", "Edit grid")
    }

    Row {
        id: row

        height: childrenRect.height
        width: parent.width

        spacing: 8

        IncrementalPropertyControl {
            id: horizontalGridSizeControl

            width: parent.width / 2 - row.spacing / 2

			navigation.name: "Horizontal grid size"
			navigation.panel: root.navigationPanel
			navigation.row: snapToGridCheckbox.navigationRowEnd + 1

            currentValue: root.horizontalGridSizeSpatium
            step: 0.05
            decimals: 2
            maxValue: 5
            minValue: 0.01

            icon: IconCode.HORIZONTAL

            onValueEditingFinished: function(newValue) {
                root.horizontalGridSizeSpatiumEdited(newValue)
            }
        }

        IncrementalPropertyControl {
            id: verticalGridSizeControl

            width: parent.width / 2 - row.spacing / 2

			navigation.name: "Vertical grid size"
			navigation.panel: root.navigationPanel
			navigation.row: horizontalGridSizeControl.navigationRowEnd + 1

            currentValue: root.verticalGridSizeSpatium
            step: 0.05
            decimals: 2
            maxValue: 5
            minValue: 0.01

            icon: IconCode.VERTICAL

            onValueEditingFinished: function(newValue) {
                root.verticalGridSizeSpatiumEdited(newValue)
            }
        }
    }
}
