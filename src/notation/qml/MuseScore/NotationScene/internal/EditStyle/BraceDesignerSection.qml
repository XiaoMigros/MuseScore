/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited
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
import QtQuick.Layouts 1.15

import MuseScore.NotationScene 1.0
import Muse.UiComponents 1.0
import Muse.Ui 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: ui.theme.backgroundPrimaryColor

    property alias model: braceModel

    property NavigationPanel navigationPanel: null
    property int navigationRowStart: 0
    property int buttonNavigationRow: navigationRowStart
    property int navigationRowEnd: navigationRowStart + 1

    BraceDesignerSectionModel {
        id: braceModel
    }

    CheckBox {
        id: useCustomPathBox
        width: parent.width
        text: qsTrc("notation", "Use custom path for curly braces") // use smufl, negate?
        checked: braceModel.useCustomPath.value
        onClicked: {
            braceModel.useCustomPath.value = !checked
        }
    }

    QtObject {
        id: prv

        property string currentItemNavigationName: ""
    }

    StyledGroupBox {
        id: braceDesignerBox
        anchors.top: useCustomPathBox.bottom
        anchors.topMargin: 12
        width: parent.width
        height: 360 //Math.max(360, implicitHeight)
        visible: braceModel.useCustomPath.value

        title: qsTrc("notation", "Custom Path Designer")

        Item {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.horizontalCenter

            NavigationControl {
                id: navCtrl
                name: "BraceDesignerCanvas"
                enabled: braceDesignerCanvas.visible

                panel: root.navigationPanel
                row: root.navigationRowEnd + 1

                accessible.role: MUAccessible.Information
                accessible.name: braceDesignerBox.title + "; " + qsTrc("notation", "Press Enter to start editing")

                onActiveChanged: {
                    if (navCtrl.active) {
                        braceDesignerCanvas.forceActiveFocus()
                    } else {
                        braceDesignerCanvas.resetFocus()
                    }
                }

                onNavigationEvent: function(event) {
                    if (!root.model) {
                        return
                    }

                    var accepted = false
                    switch(event.type) {
                    case NavigationEvent.Trigger:
                        accepted = braceDesignerCanvas.focusOnFirstPoint()

                        if (accepted) {
                            navCtrl.accessible.focused = false
                        }

                        break
                    case NavigationEvent.Escape:
                        accepted = braceDesignerCanvas.resetFocus()

                        if (accepted) {
                            navCtrl.accessible.focused = true
                        }

                        break
                    case NavigationEvent.Left:
                        accepted = braceDesignerCanvas.moveFocusedPointToLeft()
                        break
                    case NavigationEvent.Right:
                        accepted = braceDesignerCanvas.moveFocusedPointToRight()
                        break
                    case NavigationEvent.Up:
                        accepted = braceDesignerCanvas.moveFocusedPointToUp()
                        break
                    case NavigationEvent.Down:
                        accepted = braceDesignerCanvas.moveFocusedPointToDown()
                        break
                    }

                    event.accepted = accepted
                }
            }

            NavigationFocusBorder {
                navigationCtrl: navCtrl
            }

            BraceDesignerCanvas {
                id: braceDesignerCanvas

                focus: true

                pointList: root.model ? root.model.customPath : null

                accessibleParent: navCtrl.accessible

                onCanvasChanged: {
                    if (root.model) {
                        root.model.customPath = pointList
                    }
                }
            }
        }
        // add zoom in zoom out buttons at the bottom here

        ColumnLayout {
            id: controlsColumn
            anchors.top: parent.top
            anchors.left: parent.horizontalCenter
            anchors.right: parent.right
            anchors.leftMargin: 12

            RowLayout {
                width: parent.width
                spacing: 8
                anchors.margins: 8

                FlatButton {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTrc("notation", "Clear bracket")

                    onClicked: {
                        braceModel.resetPath()
                    }
                }

                StyledDropdown {
                    id: presetSelector
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    model: [
                        { value: BracketDesignerTypes.CUSTOM_BRACE, title: qsTrc("inspector", "Custom") },
                        { value: BracketDesignerTypes.FINALE_BRACE, title: qsTrc("inspector", "Finale-style") },
                        { value: BracketDesignerTypes.MUSE_BRACE,   title: qsTrc("inspector", "MuseScore-style") }
                    ]

                    onActivated: function(index, value) {
                        braceModel.loadPreset(value)
                    }
                }
            }

            CheckBox {
                id: showGridLinesBox
                width: parent.width
                checked: braceDesignerCanvas.showOutline //.value is only for style types
                text: qsTrc("notation", "Show gridlines")
                onClicked: {
                    braceDesignerCanvas.showOutline = !checked
                }
            }
            // values: % (rel) + sp (abs)
        }
        StyledFlickable {
            id: flickable

            anchors.top: controlsColumn.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.horizontalCenter
            anchors.right: parent.right
            anchors.leftMargin: 12
            anchors.topMargin: 12

            contentWidth: width
            contentHeight: bracesDesignerPointTree.implicitHeight

            TreeView {
                id: bracesDesignerPointTree

                readonly property real delegateHeight: 38

                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right

                implicitHeight: flickableItem.contentHeight
                flickableItem.interactive: false

                visible: !model.isEmpty

                model: braceDesignerCanvas.pointsModel()

                selection: braceDesignerCanvas.pointsModel() ? braceDesignerCanvas.pointsModel().selectionModel() : null

                alternatingRowColors: true // false
                backgroundVisible: false // maybe negates row colors? dont think so
                headerVisible: false
                frameVisible: false // if you want a frame, add it to the flickable

                function scrollToFocusedItem(focusedIndex) {
                    let targetScrollPosition = focusedIndex * bracesDesignerPointTree.delegateHeight
                    let visibleAreaEnd = flickable.contentY + flickable.height

                    if (targetScrollPosition + bracesDesignerPointTree.delegateHeight > visibleAreaEnd) {
                        flickable.contentY = Math.min(targetScrollPosition + bracesDesignerPointTree.delegateHeight - flickable.height, flickable.contentHeight - flickable.height)
                    } else if (targetScrollPosition < flickable.contentY) {
                        flickable.contentY = Math.max(targetScrollPosition, 0)
                    }
                }

                property NavigationPanel navigationTreePanel : NavigationPanel {
                    name: "BraceDesignerPointTree"
                    section: root.navigationSection
                    direction: NavigationPanel.Both
                    enabled: bracesDesignerPointTree.enabled && bracesDesignerPointTree.visible
                    order: showGridLinesBox.navigation.order + 1

                    onNavigationEvent: function(event) {
                        if (event.type === NavigationEvent.AboutActive) {
                            event.setData("controlName", prv.currentItemNavigationName)
                        }
                    }
                }

                TableViewColumn {
                    role: "itemRole"
                }

                style: TreeViewStyle {
                    indentation: 0

                    frame: Item {}
                    incrementControl: Item {}
                    decrementControl: Item {}
                    handle: Item {}
                    scrollBarBackground: Item {}
                    branchDelegate: Item {}

                    backgroundColor: "transparent"

                    rowDelegate: Item {
                        height: bracesDesignerPointTree.delegateHeight
                        width: parent.width
                    }
                }

                itemDelegate: DropArea {
                    id: dropArea

                    Loader {
                        id: PointDelegateLoader

                        height: parent.height
                        width: parent.width

                        sourceComponent: BracePointItem {
                            id: pointDelegate

                            treeView: bracesDesignerPointTree
                            item: model ? model.itemRole : null
                            originalParent: PointDelegateLoader

                            sideMargin: contentColumn.sideMargin
                            popupAnchorItem: root

                            navigation.name: model ? model.itemRole.title : "LayoutPanelItemDelegate"
                            navigation.panel: bracesDesignerPointTree.navigationTreePanel
                            navigation.row: model ? model.index : 0
                            navigation.onActiveChanged: {
                                if (navigation.active) {
                                    prv.currentItemNavigationName = navigation.name
                                    bracesDesignerPointTree.scrollToFocusedItem(model.index)
                                }
                            }

                            onClicked: {
                                if (pointDelegate.isSelectable) {
                                    braceDesignerCanvas.pointsModel().selectRow(styleData.index)
                                }
                            }

                            onDoubleClicked: {
                                if (!isExpandable) {
                                    return
                                }

                                if (!styleData.isExpanded) {
                                    bracesDesignerPointTree.expand(styleData.index)
                                } else {
                                    bracesDesignerPointTree.collapse(styleData.index)
                                }
                            }

                            onRemoveSelectionRequested: {
                                braceDesignerCanvas.pointsModel().removeSelectedRows()
                            }

                            property real contentYBackup: 0

                            onPopupOpened: function(popupX, popupY, popupHeight) {
                                contentYBackup = flickable.contentY
                                var mappedPopupY = mapToItem(flickable, popupX, popupY).y

                                if (mappedPopupY + popupHeight < flickable.height - contentColumn.sideMargin) {
                                    return
                                }

                                var hiddenPopupPartHeight = Math.abs(flickable.height - (mappedPopupY + popupHeight))
                                flickable.contentY += hiddenPopupPartHeight + contentColumn.sideMargin
                            }

                            onPopupClosed: {
                                flickable.contentY = contentYBackup
                            }

                            onChangeVisibilityOfSelectedRowsRequested: function(visible) {
                                braceDesignerCanvas.pointsModel().changeVisibilityOfSelectedRows(visible);
                            }

                            onChangeVisibilityRequested: function(index, visible) {
                                braceDesignerCanvas.pointsModel().changeVisibility(index, visible)
                            }

                            onDragStarted: {
                                braceDesignerCanvas.pointsModel().startActiveDrag()
                            }

                            onDropped: {
                                braceDesignerCanvas.pointsModel().endActiveDrag()
                            }
                        }
                    }

                    onEntered: function(drag) {
                        if (styleData.index === drag.source.index || !styleData.value.canAcceptDrop(drag.source.item)) {
                            return
                        }

                        braceDesignerCanvas.pointsModel().moveRows(drag.source.index.parent,
                                                     drag.source.index.row,
                                                     1,
                                                     styleData.index.parent,
                                                     styleData.index.row)
                    }
                }
            }
        }
        Item {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.leftMargin: 12

            RowLayout {
                anchors.centerIn: parent
                spacing: 4

                FlatButton {
                    id: addButton

                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24

                    icon: IconCode.PLUS
                    onClicked: {
                        for (let row = braceDesignerCanvas.pointsModel().rowCount() - 1; row >= 0 ; row--) {
                            const index = braceDesignerCanvas.pointsModel().index(row, 0);
                            if (index.isSelected) {
                                return braceDesignerCanvas.pointsModel().insertNewItem(row);
                            }
                        }
                        return braceDesignerCanvas.pointsModel().insertNewItem(braceDesignerCanvas.pointsModel().rowCount() - 1);
                    }
                }

                StyledTextLabel {
                    id: titleLabel
                    width: implicitWidth

                    text: qsTrc("notation", "Add point")
                    horizontalAlignment: Text.AlignLeft
                }
            }
        }
    }
}
