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

#include "async/asyncable.h"

#include "modularity/ioc.h"
#include "ui/iuiconfiguration.h"

#include "uicomponents/view/quickpaintedview.h"
#include "ui/view/qmlaccessible.h"
#include "context/iglobalcontext.h"

Q_MOC_INCLUDE(< QItemSelectionModel >)

namespace muse::uicomponents {
class ItemMultiSelectionModel;
}

class QItemSelectionModel;

namespace mu::notation {
class BracketDesignerTypes
{
    Q_GADGET

public:
    enum class BracePresets {
        CUSTOM_BRACE = 0,
        FINALE_BRACE  = 1,
        MUSE_BRACE = 2,
    };

    Q_ENUM(BracePresets)
};

// todo: declare this type in engraving so we can use it for layout there
struct BracePoint
{
    enum class PointType {
        Move,
        Line,
        Quad,
        Cubic,
        Arc,
    };

    PointType type;
    QPointF relative;
    QPointF absolute;

    BracePoint() = default;
    BracePoint(PointType type, QPointF relative, QPointF absolute)
        : type(type), relative(relative), absolute(absolute) {}

    inline bool operator==(const BracePoint& o) const
    {
        return o.relative == relative && o.absolute == absolute && o.type == type;
    }

    inline bool operator!=(const BracePoint& o) const { return !operator==(o); }

    QVariantMap toMap() const
    {
        QVariantMap map;
        map["type"]     = static_cast<int>(type);
        map["relative"] = relative;
        map["absolute"] = absolute;

        return map;
    }

    static BracePoint fromMap(const QVariant& obj)
    {
        QVariantMap map = obj.toMap();

        BracePoint point;
        point.type     = static_cast<PointType>(map["type"].toInt());
        point.relative = map["relative"].toPointF();
        point.relative = map["relative"].toPointF();
        return point;
    }
};

using BracePoints = QList<BracePoint>;

inline QVariant bracePointsToQVariant(const BracePoints& points)
{
    QVariantList list;
    for (const BracePoint& value : points) {
        list << value.toMap();
    }

    return list;
}

inline BracePoints bracePointsFromQVariant(const QVariant& var)
{
    BracePoints points;
    for (const QVariant& obj : var.toList()) {
        points.push_back(BracePoint::fromMap(obj));
    }

    return points;
}

class BraceDesignerCanvas : public muse::uicomponents::QuickPaintedView, public muse::async::Asyncable
{
    Q_OBJECT

    INJECT(muse::ui::IUiConfiguration, uiConfig)
    INJECT(context::IGlobalContext, context)

    Q_PROPERTY(QVariant pointList READ pointList WRITE setPointList NOTIFY pointListChanged)
    Q_PROPERTY(QSizeF scale READ scale WRITE setScale NOTIFY scaleChanged)

    Q_PROPERTY(bool showOutline READ showOutline WRITE setShowOutline NOTIFY showOutlineChanged)

    Q_PROPERTY(muse::ui::AccessibleItem
               * accessibleParent READ accessibleParent WRITE setAccessibleParent NOTIFY accessibleParentChanged)

public:
    explicit BraceDesignerCanvas(QQuickItem* parent = nullptr);
    ~BraceDesignerCanvas() override;

    QModelIndex index(int row, int column, const QModelIndex& model) const override;
    QVariant pointList() const;
    QSizeF scale() const;

    bool showOutline() const;

    Q_INVOKABLE bool focusOnFirstPoint();
    Q_INVOKABLE bool resetFocus();

    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();

    Q_INVOKABLE bool moveFocusedPointToLeft();
    Q_INVOKABLE bool moveFocusedPointToRight();
    Q_INVOKABLE bool moveFocusedPointToUp();
    Q_INVOKABLE bool moveFocusedPointToDown();

    muse::ui::AccessibleItem* accessibleParent() const;
    void setAccessibleParent(muse::ui::AccessibleItem* parent);

    Q_INVOKABLE void load();
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
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
	Q_INVOKABLE void insertNewItem(int index);

public slots:
    void setPointList(QVariant pointList);
    void setScale(QSizeF scale);
    void setShowOutline(bool show);

signals:
    void canvasChanged();

    void pointListChanged(QVariant pointList);
    void scaleChanged(QSizeF scale);

    void showOutlineChanged(bool show);

    void accessibleParentChanged();
    void isMovingUpAvailableChanged(bool isMovingUpAvailable);
    void isMovingDownAvailableChanged(bool isMovingDownAvailable);
    void isRemovingAvailableChanged(bool isRemovingAvailable);
    void isEmptyChanged();

private:
    void paint(QPainter* painter) override;

    void setPointListInternal(const BracePoints& newPointList);

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void hoverEnterEvent(QHoverEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;

    bool eventFilter(QObject* watched, QEvent* event) override;
    bool shortcutOverride(QKeyEvent* event);

    QRectF frameRect() const;
    QPointF frameCoord(const QRectF& frameRect, double x, double y) const;

    void drawBackground(QPainter* painter, const QRectF& frameRect);
    void drawCurve(QPainter* painter, const QRectF& frameRect);

    bool isPointIndexValid(int index) const;

    std::optional<int> pointIndex(const QPointF& point) const;
    QPointF pointCoord(const BracePoint& point) const;

    QString pointAccessibleName(const BracePoint& point);
    void updatePointAccessibleName(int index);

    bool movePoint(int pointIndex, BracePoint toPoint);

    void setFocusedPointIndex(int index);

    BracePoints m_points;
    bool isFirstOrLastPoint(int index);
    QList<muse::ui::AccessibleItem*> m_pointsAccessibleItems;
    muse::ui::AccessibleItem* m_accessibleParent = nullptr;
    bool m_needVoicePointName = false;
    void setLoadingBlocked(bool blocked);
    mu::notation::INotationPtr m_notation = nullptr;
    bool m_notationChangedWhileLoadingWasBlocked = false;

    QSizeF m_scale;

    bool m_showOutline;

    std::optional<int> m_currentPointIndex; //todo: make multiple
    std::optional<int> m_focusedPointIndex;
    std::optional<int> m_hoverPointIndex;

    bool m_canvasWasChanged = false;

    bool removeRows(int row, int count, const QModelIndex& parent) override;
    void clear();
    void deleteItems();

    void setIsMovingUpAvailable(bool isMovingUpAvailable);
    void setIsMovingDownAvailable(bool isMovingDownAvailable);
    void setIsRemovingAvailable(bool isRemovingAvailable);
    void setItemsSelected(const QModelIndexList& indexes, bool selected);
    QVariantMap modelIndexToItem(const QModelIndex& index) const;

    bool m_isMovingUpAvailable = false;
    bool m_isMovingDownAvailable = false;
    bool m_isRemovingAvailable = false;
    bool m_isLoadingBlocked = false;
    muse::uicomponents::ItemMultiSelectionModel* m_selectionModel = nullptr;
    bool m_dragInProgress = false;
    // we might need an enum Roles, even though we only use one role (ItemRole)!

private slots:
    // void updateRearrangementAvailability();
    void updateMovingUpAvailability(bool isSelectionMovable, const QModelIndex& firstSelectedRowIndex = QModelIndex());
    void updateMovingDownAvailability(bool isSelectionMovable, const QModelIndex& lastSelectedRowIndex = QModelIndex());
    void updateRemovingAvailability();
};
}
