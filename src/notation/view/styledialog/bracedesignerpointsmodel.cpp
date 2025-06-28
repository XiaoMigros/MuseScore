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

#include "bracedesignerpointsmodel.h"

#include <cmath>
// #include <algorithm>

// #include "engraving/dom/utils.h"

#include "uicomponents/view/itemmultiselectionmodel.h"

#include "translation.h"
#include "defer.h"
#include "log.h"

using namespace mu::notation;
using namespace muse::ui;
using namespace muse::accessibility;
using namespace muse::uicomponents;

BraceDesignerPointsModel::BraceDesignerPointsModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    // m_partsNotifyReceiver = std::make_shared<muse::async::Asyncable>();

    m_selectionModel = new ItemMultiSelectionModel(this);
    m_selectionModel->setAllowedModifiers(Qt::ShiftModifier);

    connect(m_selectionModel, &ItemMultiSelectionModel::selectionChanged,
            this, [this](const QItemSelection& selected, const QItemSelection& deselected) {
        setItemsSelected(deselected.indexes(), false);
        setItemsSelected(selected.indexes(), true);
        // do something with the selected/deselected items
        if (selected.indexes().size() == 1) {
            emit selectedPointsChanged(selected.indexes().front().row()); // todo add
        } else {
            emit selectedPointsChanged(-1);
        }
    });

    connect(this, &BraceDesignerPointsModel::rowsInserted, this, [this]() {
        updateRemovingAvailability();
    });

    /*connect(this, &BraceDesignerCanvas::pointListChanged, [this](QVariant points, bool fromCanvas) {
        if (!fromCanvas) {
            return;
        }
        update();
    });*/

}

BraceDesignerPointsModel::~BraceDesignerPointsModel()
{
    deleteItems(); //needed?
}

bool BraceDesignerPointsModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (!m_isRemovingAvailable) {
        return false;
    }

    // BraceDesignerPointsModel* parentItem = modelIndexToItem(parent);

    // if (!parentItem) {
        // parentItem = m_rootItem;
    // }

    // setLoadingBlocked(true);
    beginRemoveRows(parent, row, row + count - 1);

    // parentItem->removeChildren(row, count, true);

    endRemoveRows();
    // setLoadingBlocked(false);

    emit isEmptyChanged();
    emit pointListChanged(pointList);

    return true;
}

void BraceDesignerPointsModel::clear()
{
    // TRACEFUNC;

    // beginResetModel();
    // deleteItems();
    // endResetModel();

    // emit isEmptyChanged();
    // emit isAddingAvailableChanged(false);
}

void BraceDesignerPointsModel::deleteItems()
{
    clearSelection();
    // setPointListInternal(BracePoints());
}

void BraceDesignerPointsModel::load()
{
    // if (m_isLoadingBlocked) {
        // return;
    // }

    TRACEFUNC;

    beginResetModel();

    clearSelection(); // since points are already bound and we don't need to collect from engraving, this should be fine???

    endResetModel();


    emit isEmptyChanged();
    // emit isAddingAvailableChanged(true);
}

QVariant BraceDesignerPointsModel::data(const QModelIndex& index, int role) const
{
    int row = index.row();
    if (row < 0 || row > pointList.toList().size()) {
        return QVariant();
    }

    return pointList.toList().at(row);
}

bool BraceDesignerPointsModel::setData(const QModelIndex& index, const QVariant& value, int)
{
    int row = index.row();

    if (row < 0 || row > pointList.toList().size()) {
        return false;
    }

    pointList.toList().replace(row, value);
    clearSelection();
    m_selectionModel->select(index);
    emit dataChanged(index, index, { ItemRole }); // needed?
    // emit selectionChanged();
    emit selectedPointsChanged(row);
    emit pointListChanged(pointList);
    return true;
}

int BraceDesignerPointsModel::rowCount(const QModelIndex&) const
{
    return pointList.toList().size();
}

void BraceDesignerPointsModel::selectRow(const QModelIndex& rowIndex)
{
    m_selectionModel->select(rowIndex);
}

void BraceDesignerPointsModel::clearSelection()
{
    m_selectionModel->clear();
}

void BraceDesignerPointsModel::moveSelectedRowsUp()
{
    if (!m_isMovingUpAvailable) {
        return;
    }

    QModelIndexList selectedIndexList = m_selectionModel->selectedIndexes();
    if (selectedIndexList.isEmpty()) {
        return;
    }

    std::sort(selectedIndexList.begin(), selectedIndexList.end(), [](QModelIndex f, QModelIndex s) -> bool {
        return f.row() < s.row();
    });

    const QModelIndex& sourceRowFirst = selectedIndexList.first();

    moveRows(sourceRowFirst.parent(), sourceRowFirst.row(), selectedIndexList.count(), sourceRowFirst.parent(), sourceRowFirst.row() - 1);
}

void BraceDesignerPointsModel::moveSelectedRowsDown()
{
    if (!m_isMovingDownAvailable) {
        return;
    }

    QModelIndexList selectedIndexList = m_selectionModel->selectedIndexes();
    if (selectedIndexList.isEmpty()) {
        return;
    }

    std::sort(selectedIndexList.begin(), selectedIndexList.end(), [](const QModelIndex& f, const QModelIndex& s) -> bool {
        return f.row() < s.row();
    });

    const QModelIndex& sourceRowFirst = selectedIndexList.first();
    const QModelIndex& sourceRowLast = selectedIndexList.last();

    moveRows(sourceRowFirst.parent(), sourceRowFirst.row(), selectedIndexList.count(), sourceRowFirst.parent(), sourceRowLast.row() + 1);
}

void BraceDesignerPointsModel::removeSelectedRows()
{
    if (!m_isRemovingAvailable) {
        return;
    }

    QModelIndexList selectedIndexList = m_selectionModel->selectedIndexes();
    if (selectedIndexList.empty()) {
        return;
    }

    QModelIndex firstIndex = *std::min_element(selectedIndexList.cbegin(), selectedIndexList.cend(),
                                               [](const QModelIndex& f, const QModelIndex& s) {
        return f.row() < s.row();
    });

    removeRows(firstIndex.row(), selectedIndexList.size(), firstIndex.parent());
}

// int BraceDesignerPointsModel::indexOf(const BracePoint& point) const
// {
    // return m_points.indexOf(point);
// }

bool BraceDesignerPointsModel::moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent,
                                    int destinationChild)
{
    // setLoadingBlocked(true);
    // DEFER {
        // setLoadingBlocked(false);
    // };

    // QVariantMap sourceItem = modelIndexToItem(sourceParent);
    // QVariantMap sourceItem = modelIndexToItem(destinationParent);

    int sourceFirstRow = sourceRow;
    int sourceLastRow = sourceRow + count - 1;
    int destinationRow = /*(*/sourceLastRow > destinationChild /*|| sourceParentItem != destinationParentItem)*/
                         ? destinationChild : destinationChild + 1;

    /* if (m_dragInProgress) {
        MoveParams moveParams;
        moveParams.sourceRow = sourceRow;
        moveParams.count = count;
        moveParams.destinationRow = destinationRow;

        if (!params.isValid()) {
            return false;
        }

        // m_dragSourceParentItem = sourceParentItem;
        m_activeDragMoveParams = params;
    } */

    beginMoveRows(sourceParent, sourceFirstRow, sourceLastRow, destinationParent, destinationRow);
    // sourceParentItem->moveChildren(sourceFirstRow, count, destinationParentItem, destinationRow, !m_dragInProgress);
    endMoveRows(); // if nothing needed, replace with moverows
    updateRearrangementAvailability();

    return true;
}

QVariantMap BraceDesignerPointsModel::modelIndexToItem(const QModelIndex& index) const
{
    return data(index, ItemRole).toMap();
}

void BraceDesignerPointsModel::startActiveDrag()
{
    m_dragInProgress = true;
}

void BraceDesignerPointsModel::endActiveDrag()
{
    // setLoadingBlocked(true);

    // here we would commit the change, but since we're only working with the qt model we can do nothing here instead

    // m_dragSourceParentItem = nullptr;
    // m_activeDragMoveParams = MoveParams();
    m_dragInProgress = false;

    // setLoadingBlocked(false);

    // updateRearrangementAvailability(); //perhaps needed? not in the original code.
}

QItemSelectionModel* BraceDesignerPointsModel::selectionModel() const
{
    return m_selectionModel;
}

int BraceDesignerPointsModel::columnCount(const QModelIndex&) const
{
    return 1;
}

void BraceDesignerPointsModel::setIsMovingUpAvailable(bool isMovingUpAvailable)
{
    if (m_isMovingUpAvailable == isMovingUpAvailable) {
        return;
    }

    m_isMovingUpAvailable = isMovingUpAvailable;
    emit isMovingUpAvailableChanged(m_isMovingUpAvailable);
}

void BraceDesignerPointsModel::setIsMovingDownAvailable(bool isMovingDownAvailable)
{
    if (m_isMovingDownAvailable == isMovingDownAvailable) {
        return;
    }

    m_isMovingDownAvailable = isMovingDownAvailable;
    emit isMovingDownAvailableChanged(m_isMovingDownAvailable);
}

bool BraceDesignerPointsModel::isMovingUpAvailable() const
{
    return m_isMovingUpAvailable;
}

bool BraceDesignerPointsModel::isMovingDownAvailable() const
{
    return m_isMovingDownAvailable;
}

bool BraceDesignerPointsModel::isRemovingAvailable() const
{
    return m_isRemovingAvailable;
}

bool BraceDesignerPointsModel::isEmpty() const
{
    return !pointList.isNull() ? pointList.toList().isEmpty() : true;
}

void BraceDesignerPointsModel::updateRearrangementAvailability()
{
    QModelIndexList selectedIndexList = m_selectionModel->selectedIndexes();

    if (selectedIndexList.isEmpty()) {
        updateMovingUpAvailability(false);
        updateMovingDownAvailability(false);
        return;
    }

    std::sort(selectedIndexList.begin(), selectedIndexList.end(), [](const QModelIndex& f, const QModelIndex& s) -> bool {
        return f.row() < s.row();
    });

    bool isRearrangementAvailable = true;

    QMutableListIterator<QModelIndex> it(selectedIndexList);

    while (it.hasNext() && selectedIndexList.count() > 1) {
        int nextRow = it.next().row();
        int previousRow = it.peekPrevious().row();

        isRearrangementAvailable = (nextRow - previousRow == 1);
        if (!isRearrangementAvailable) {
            updateMovingUpAvailability(isRearrangementAvailable);
            updateMovingDownAvailability(isRearrangementAvailable);
            return;
        }
    }

    updateMovingUpAvailability(isRearrangementAvailable, selectedIndexList.first());
    updateMovingDownAvailability(isRearrangementAvailable, selectedIndexList.last());
}

void BraceDesignerPointsModel::updateMovingUpAvailability(bool isSelectionMovable, const QModelIndex& firstSelectedRowIndex)
{
    bool isRowInBoundaries = firstSelectedRowIndex.isValid() && firstSelectedRowIndex.row() > 0;
    setIsMovingUpAvailable(isSelectionMovable && isRowInBoundaries);
}

void BraceDesignerPointsModel::updateMovingDownAvailability(bool isSelectionMovable, const QModelIndex& lastSelectedRowIndex)
{
    bool isRowInBoundaries = lastSelectedRowIndex.isValid() && lastSelectedRowIndex.row() + 1 < pointList.toList().size();
    setIsMovingDownAvailable(isSelectionMovable && isRowInBoundaries);
}

void BraceDesignerPointsModel::updateRemovingAvailability()
{
    const QModelIndexList selectedIndexes = m_selectionModel->selectedIndexes();

    bool isRemovingAvailable = !selectedIndexes.empty();

    for (const QModelIndex& index : selectedIndexes) {
        isRemovingAvailable = isRemovingAvailable && !(index.row() == 0 || index.row() == pointList.toList().size());

        if (!isRemovingAvailable) {
            break;
        }
    }

    setIsRemovingAvailable(isRemovingAvailable);
}

void BraceDesignerPointsModel::setItemsSelected(const QModelIndexList& indexes, bool selected)
{
    for (const QModelIndex& index : indexes) {
        m_selectionModel->select(index, selected ? QItemSelectionModel::Select : QItemSelectionModel::Deselect);
    }
}

QModelIndex BraceDesignerPointsModel::index(int row, int column, const QModelIndex& model) const
{
    if (hasIndex(row, column, model) && !(row == 0 || row == pointList.toList().size())) {
        return createIndex(row, column); //, pointList.toList().at(row).toMap()
    }
    return QModelIndex();
}

void BraceDesignerPointsModel::setSelectedIndex(int row)
{
    QModelIndex index = createIndex(row, 0);
    selectRow(index);
}
