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

#pragma once

#include <optional>

#include <QPainter>
#include <QVariant>
#include <QAbstractItemModel>

#include "bracedesignercanvas.h"

#include "async/asyncable.h"

#include "modularity/ioc.h"
#include "ui/iuiconfiguration.h"

#include "context/iglobalcontext.h"

Q_MOC_INCLUDE(< QItemSelectionModel >)

namespace muse::uicomponents {
class ItemMultiSelectionModel;
}

class QItemSelectionModel;

namespace mu::notation {
class BraceDesignerPointsModel : public QAbstractItemModel, public muse::async::Asyncable
{
    Q_OBJECT // Inject here if needed

    // Q_PROPERTY(QVariant pointList READ pointList WRITE setPointList NOTIFY pointListChanged)
    Q_PROPERTY(bool isMovingUpAvailable READ isMovingUpAvailable NOTIFY isMovingUpAvailableChanged)
    Q_PROPERTY(bool isMovingDownAvailable READ isMovingDownAvailable NOTIFY isMovingDownAvailableChanged)
    Q_PROPERTY(bool isRemovingAvailable READ isRemovingAvailable NOTIFY isRemovingAvailableChanged)
    Q_PROPERTY(bool isEmpty READ isEmpty NOTIFY isEmptyChanged)

public:
    explicit BraceDesignerPointsModel(QObject* parent = nullptr);
    ~BraceDesignerPointsModel() override;

    QVariant pointList;

    QModelIndex index(int row, int column, const QModelIndex& model = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    Q_INVOKABLE void load();
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int) override;
    bool isEmpty() const;
    bool isMovingUpAvailable() const;
    bool isMovingDownAvailable() const;
    bool isRemovingAvailable() const;
    Q_INVOKABLE void selectRow(const QModelIndex& rowIndex);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent,
                              int destinationChild) override;
    Q_INVOKABLE QItemSelectionModel* selectionModel() const;

    Q_INVOKABLE void startActiveDrag();
    Q_INVOKABLE void endActiveDrag();
    Q_INVOKABLE void moveSelectedRowsUp();
    Q_INVOKABLE void moveSelectedRowsDown();
    Q_INVOKABLE void removeSelectedRows();
    // Q_INVOKABLE void insertNewItem(int index);
    void setSelectedIndex(int row);
    void clear();

// public slots:
    // void setPointList(QVariant pointList);

signals:
    void isMovingUpAvailableChanged(bool isMovingUpAvailable);
    void isMovingDownAvailableChanged(bool isMovingDownAvailable);
    void isRemovingAvailableChanged(bool isRemovingAvailable);
    void isEmptyChanged();

    void selectedPointsChanged(int index);
    void pointListChanged(QVariant pointList);
    void insertNewPoint(int index);

private:
    bool removeRows(int row, int count, const QModelIndex& parent) override;
    void deleteItems();

    void setIsMovingUpAvailable(bool isMovingUpAvailable);
    void setIsMovingDownAvailable(bool isMovingDownAvailable);
    void setIsRemovingAvailable(bool isRemovingAvailable);
    void setItemsSelected(const QModelIndexList& indexes, bool selected);
    QVariantMap modelIndexToItem(const QModelIndex& index) const;

    bool m_isMovingUpAvailable = false;
    bool m_isMovingDownAvailable = false;
    bool m_isRemovingAvailable = false;

    muse::uicomponents::ItemMultiSelectionModel* m_selectionModel = nullptr;
    bool m_dragInProgress = false;

    enum RoleNames {
        ItemRole = Qt::UserRole + 1
    };

    // BracePoints m_points;
    // bool isFirstOrLastPoint(int index);
    // void setPointListInternal(const BracePoints& newPointList);

private slots:
    void updateRearrangementAvailability();
    void updateMovingUpAvailability(bool isSelectionMovable, const QModelIndex& firstSelectedRowIndex = QModelIndex());
    void updateMovingDownAvailability(bool isSelectionMovable, const QModelIndex& lastSelectedRowIndex = QModelIndex());
    void updateRemovingAvailability();
};
}
