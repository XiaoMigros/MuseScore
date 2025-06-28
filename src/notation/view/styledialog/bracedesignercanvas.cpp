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

#include "bracedesignercanvas.h"

#include <cmath>
// #include <algorithm>

#include <QPainterPath>

// #include "engraving/dom/utils.h"

#include "uicomponents/view/itemmultiselectionmodel.h"

#include "translation.h"
#include "log.h"

using namespace mu::notation;
using namespace muse::ui;
using namespace muse::accessibility;

static constexpr int GRIP_RADIUS = 6;
static constexpr int GRIP_CENTER_RADIUS = GRIP_RADIUS - 2;
static constexpr int GRIP_SELECTED_RADIUS = GRIP_RADIUS + 2;
static constexpr int GRIP_FOCUS_RADIUS = GRIP_SELECTED_RADIUS + 2;

static constexpr int GRID_LINE_WIDTH = 1;
static constexpr int PATH_LINE_WIDTH = 2;

static constexpr int INVALID_INDEX = -1;

static constexpr int STEP_SIZE = 10;

BraceDesignerCanvas::BraceDesignerCanvas(QQuickItem* parent)
    : muse::uicomponents::QuickPaintedView(parent)
{
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);

    setKeepMouseGrab(true);

    uiConfig()->currentThemeChanged().onNotify(this, [this]() {
        update();
    });

    // uiConfig()->fontChanged().onNotify(this, [this]() { // needed?
        // update();
    // });

    // connect(this, &BraceDesignerCanvas::enabledChanged, [this](){
        // update();
    // });

    connect(this, &BraceDesignerCanvas::showOutlineChanged, [this](){
        update();
    });

    setScale(QSizeF(1.0, 1.0)); // for now

    connect(this, &BraceDesignerCanvas::scaleChanged, [this](){
        update();
    });

    qApp->installEventFilter(this);


    // m_partsNotifyReceiver = std::make_shared<muse::async::Asyncable>();

    m_selectionModel = new ItemMultiSelectionModel(this);
    m_selectionModel->setAllowedModifiers(Qt::ShiftModifier);

    connect(m_selectionModel, &ItemMultiSelectionModel::selectionChanged,
            this, [this](const QItemSelection& selected, const QItemSelection& deselected) {
        setItemsSelected(deselected.indexes(), false);
        setItemsSelected(selected.indexes(), true);
        // do something with the selected/deselected items
        // emit selectionChanged
    });

    connect(this, &BraceDesignerCanvas::rowsInserted, this, [this]() {
        updateRemovingAvailability();
    });

    onNotationChanged();
     context()->currentNotationChanged().onNotify(this, [this]() {
        onNotationChanged();
    });
}

BraceDesignerCanvas::~BraceDesignerCanvas()
{
    muse::DeleteAll(m_pointsAccessibleItems);
    deleteItems(); //needed?
}

QVariant BraceDesignerCanvas::pointList() const
{
    return bracePointsToQVariant(m_points);
}

QSizeF BraceDesignerCanvas::scale() const
{
    return m_scale;
}

void BraceDesignerCanvas::onNotationChanged()
{
    // m_partsNotifyReceiver->disconnectAll();

    if (m_isLoadingBlocked) {
        m_notationChangedWhileLoadingWasBlocked = true;
        return;
    }

    onBeforeChangeNotation();
    m_notation = context()->currentNotation();

    if (m_notation) {
        load();
    } else {
        clear();
    }

    m_notationChangedWhileLoadingWasBlocked = false;
}

void BraceDesignerCanvas::onBeforeChangeNotation()
{
    if (!m_notation) {
        return;
    }
	QSizeF newScale = QSizeF(m_notation->style()->styleValue(StyleId::akkoladeWidth).toDouble(), m_scale.height());
	setScale(newScale);
}

bool BraceDesignerCanvas::showOutline() const
{
    return m_showOutline;
}

void BraceDesignerCanvas::zoomIn()
{
    QSizeF newScale = QSizeF(m_scale.height() * 2, m_scale.width());
    setScale(newScale);
}

void BraceDesignerCanvas::zoomOut()
{
    QSizeF newScale = QSizeF(m_scale.height() / 2, m_scale.width());
    setScale(newScale);
}

bool BraceDesignerCanvas::focusOnFirstPoint()
{
    if (m_focusedPointIndex.has_value()) {
        return true;
    }

    if (!isPointIndexValid(0)) {
        return false;
    }

    setFocusedPointIndex(0);

    update();

    return true;
}

bool BraceDesignerCanvas::resetFocus()
{
    if (!m_focusedPointIndex.has_value()) {
        return false;
    }

    setFocusedPointIndex(INVALID_INDEX);

    update();

    return true;
}

bool BraceDesignerCanvas::moveFocusedPointToLeft()
{
    if (!m_focusedPointIndex.has_value()) {
        return false;
    }

    int index = m_focusedPointIndex.value();
    BracePoint focusedPoint = m_points.at(index);

    // add limits here

    focusedPoint.relative.rx() -= frameRect().height() / (STEP_SIZE * m_scale.width());

    if (movePoint(index, focusedPoint)) {
        update();
        emit canvasChanged();
    }

    return true;
}

bool BraceDesignerCanvas::moveFocusedPointToRight()
{
    if (!m_focusedPointIndex.has_value()) {
        return false;
    }

    int index = m_focusedPointIndex.value();
    BracePoint focusedPoint = m_points.at(index);

    // add limits here

    focusedPoint.relative.rx() += frameRect().height() / (STEP_SIZE * m_scale.width());

    if (movePoint(index, focusedPoint)) {
        update();
        emit canvasChanged();
    }

    return true;
}

bool BraceDesignerCanvas::moveFocusedPointToUp()
{
    if (!m_focusedPointIndex.has_value()) {
        return false;
    }

    int index = m_focusedPointIndex.value();
    BracePoint focusedPoint = m_points.at(index);

    focusedPoint.relative.ry() -= frameRect().height() / (STEP_SIZE * m_scale.height());

    if (movePoint(index, focusedPoint)) {
        update();
        emit canvasChanged();
    }

    return true;
}

bool BraceDesignerCanvas::moveFocusedPointToDown()
{
    if (!m_focusedPointIndex.has_value()) {
        return false;
    }

    int index = m_focusedPointIndex.value();
    BracePoint focusedPoint = m_points.at(index);

    focusedPoint.relative.ry() += frameRect().height() / (STEP_SIZE * m_scale.height());

    if (movePoint(index, focusedPoint)) {
        update();
        emit canvasChanged();
    }

    return true;
}

void BraceDesignerCanvas::setScale(QSizeF scale)
{
    scale.rwidth() == std::min(scale.width(), 0.1);
    scale.rheight() == std::min(scale.height(), 0.1);
    if (m_scale == scale) {
        return;
    }
    m_scale = scale;
    emit scaleChanged(m_scale);
}

void BraceDesignerCanvas::setShowOutline(bool show)
{
    if (m_showOutline == show) {
        return;
    }
    m_showOutline = show;
    emit showOutlineChanged(m_showOutline);
}

void BraceDesignerCanvas::setPointList(QVariant points)
{
    BracePoints newPointList = bracePointsFromQVariant(points);
    setPointListInternal(newPointList);
}

void BraceDesignerCanvas::setPointListInternal(const BracePoints& newPointList)
{
    if (m_points == newPointList) {
        return;
    }

    m_points = newPointList;

    m_pointsAccessibleItems.clear();
    for (const BracePoint& point : m_points) {
        muse::ui::AccessibleItem* item = new muse::ui::AccessibleItem(this);
        item->setName(pointAccessibleName(point));
        item->setAccessibleParent(m_accessibleParent);
        item->setRole(MUAccessible::Role::Information);
        item->componentComplete();

        m_pointsAccessibleItems << item;
    }

    update();
    emit pointListChanged(points);
}

void BraceDesignerCanvas::paint(QPainter* painter)
{
    QRectF frameRect = this->frameRect();

    drawBackground(painter, frameRect);

    if (isEnabled()) {
        drawCurve(painter, frameRect);
    }
}

void BraceDesignerCanvas::mousePressEvent(QMouseEvent* event)
{
    QRectF frameRect = this->frameRect();
    QPointF coord = this->frameCoord(frameRect, event->pos().x(), event->pos().y());

    m_currentPointIndex = this->pointIndex(coord);
    m_canvasWasChanged = false;

    update();
}

void BraceDesignerCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_currentPointIndex.has_value()) {
        return;
    }

    QRectF frameRect = this->frameRect();
    QPointF coord = this->frameCoord(frameRect, event->pos().x(), event->pos().y());
    coord.rx() /= m_scale.width();
    coord.ry() /= m_scale.height();
    BracePoint bp = BracePoint(m_points.at(m_currentPointIndex.value()).type, coord, QPointF());

    if (movePoint(m_currentPointIndex.value(), bp)) {
        m_canvasWasChanged = true;
        update();
    }
}

void BraceDesignerCanvas::mouseReleaseEvent(QMouseEvent*)
{
    m_currentPointIndex = std::nullopt;

    if (m_canvasWasChanged) {
        emit canvasChanged();
    }

    m_canvasWasChanged = false;
}

void BraceDesignerCanvas::hoverEnterEvent(QHoverEvent*)
{
    m_hoverPointIndex = std::nullopt;
}

void BraceDesignerCanvas::hoverMoveEvent(QHoverEvent* event)
{
    auto oldPointIndex = m_hoverPointIndex;

    QRectF frameRect = this->frameRect();
    QPointF pos = event->position();
    QPointF coord = this->frameCoord(frameRect, pos.x(), pos.y());

    m_hoverPointIndex = this->pointIndex(coord);

    if (oldPointIndex != m_hoverPointIndex) {
        update();
    }
}

void BraceDesignerCanvas::hoverLeaveEvent(QHoverEvent*)
{
    m_hoverPointIndex = std::nullopt;
}

bool BraceDesignerCanvas::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Type::ShortcutOverride) {
        return shortcutOverride(dynamic_cast<QKeyEvent*>(event));
    }

    return QQuickPaintedItem::eventFilter(watched, event);
}

bool BraceDesignerCanvas::shortcutOverride(QKeyEvent* event)
{
    if (!m_focusedPointIndex.has_value()) {
        return false;
    }

    if (!(event->modifiers() & Qt::KeyboardModifier::AltModifier)) {
        return false;
    }

    int index = m_focusedPointIndex.value();
    switch (event->key()) {
    case Qt::Key_Left:
        index--;

        if (!isPointIndexValid(index)) {
            return false;
        }
        break;
    case Qt::Key_Right:
        index++;

        if (!isPointIndexValid(index)) {
            return false;
        }
        break;
    default:
        return false;
    }

    setFocusedPointIndex(index);

    update();

    event->accept();
    return true;
}

QRectF BraceDesignerCanvas::frameRect() const
{
    // not qreal here, even though elsewhere yes,
    // because width and height return a number of pixels,
    // hence integers.
    const int w = width();
    const int h = height();

    // let half a column of margin around
    const qreal margin = 12.0;
    const qreal leftPos = margin * 3.0;   // also left margin
    const qreal topPos = margin;      // also top margin
    const qreal rightPos = qreal(w) - margin;   // right end position of graph
    const qreal bottomPos = qreal(h) - margin;   // bottom end position of graph

    return QRectF(QPointF(leftPos, topPos), QPointF(rightPos, bottomPos));
}

QPointF BraceDesignerCanvas::frameCoord(const QRectF& frameRect, double x, double y) const
{
    // restrict to clickable area
    if (x > frameRect.right()) {
        x = frameRect.right();
    } else if (x < frameRect.left()) {
        x = frameRect.left();
    }
    if (y > frameRect.bottom()) {
        y = frameRect.bottom();
    } else if (y < frameRect.top()) {
        y = frameRect.top();
    }

    return QPointF(x, y);
}

void BraceDesignerCanvas::drawBackground(QPainter* painter, const QRectF& frameRect)
{
    const ThemeInfo& currentTheme = uiConfig()->currentTheme();
    QColor primaryLinesColor(isEnabled() ? (currentTheme.codeKey == DARK_THEME_CODE ? Qt::white : Qt::black) : Qt::gray);
    QColor backgroundColor(Qt::white);
    QColor outlineColor(Qt::gray);

    painter->setRenderHint(QPainter::Antialiasing, true);

    // QColor backgroundColor(currentTheme.values[BACKGROUND_PRIMARY_COLOR].toString());
    painter->fillRect(QRect(0, 0, width(), height()), backgroundColor);

    QPen pen = painter->pen();

    if (showOutline()) {
        pen.setWidth(GRID_LINE_WIDTH);
        pen.setColor(outlineColor);

        // draw outline (todo)
        //painter->drawLine(xpos, frameRect.top(), xpos, frameRect.bottom()); qreals or 2 points
    }

    // draw a frame
    QPainterPath path;
    path.addRoundedRect(frameRect, 3, 3);

    pen.setColor(primaryLinesColor);
    pen.setWidth(GRID_LINE_WIDTH);
    pen.setStyle(Qt::PenStyle::SolidLine);
    painter->setPen(pen);

    painter->fillPath(path, Qt::transparent);
    painter->drawPath(path);
}

void BraceDesignerCanvas::drawCurve(QPainter* painter, const QRectF& frameRect)
{
    const ThemeInfo& currentTheme = uiConfig()->currentTheme();
    QColor backgroundColor(currentTheme.values[BACKGROUND_PRIMARY_COLOR].toString());
    QColor accentColor(currentTheme.values[ACCENT_COLOR].toString());

    // draw lines to and from selected point
    if (m_currentPointIndex.has_value()) {
        QPen pen = painter->pen();
        pen.setWidth(PATH_LINE_WIDTH);
        pen.setColor(accentColor);
        QPainterPath selectedPath;
        if (isPointIndexValid(m_currentPointIndex.value() - 1)) {
            selectedPath.moveTo(pointCoord(m_points.at(m_currentPointIndex.value() - 1)));
            selectedPath.lineTo(pointCoord(m_points.at(m_currentPointIndex.value())));
        }
        if (isPointIndexValid(m_currentPointIndex.value() + 1)) {
            selectedPath.moveTo(pointCoord(m_points.at(m_currentPointIndex.value())));
            selectedPath.lineTo(pointCoord(m_points.at(m_currentPointIndex.value() + 1)));
        }
        painter->strokePath(selectedPath, pen);
    }

    // draw brace
    QPainterPath path;
    for (int i = 0; i < m_points.size(); ++i) {
        const BracePoint& point = m_points.at(i);
        switch (point.type) {
        case BracePoint::PointType::Move:
            path.moveTo(pointCoord(point));
            break;
        case BracePoint::PointType::Line:
            path.lineTo(pointCoord(point));
            break;
        case BracePoint::PointType::Quad: {
            if (isPointIndexValid(i + 1)) {
                path.quadTo(pointCoord(point), pointCoord(m_points.at(i + 1)));
            }
            i = i + 1;
        }
        break;
        case BracePoint::PointType::Cubic: {
            if (isPointIndexValid(i + 2)) {
                path.cubicTo(pointCoord(point), pointCoord(m_points.at(i + 1)), pointCoord(m_points.at(i + 2)));
            }
            i = i + 2;
        }
        break;
        case BracePoint::PointType::Arc:
            //unsupported
            break;
        }
    }
    painter->setPen(Qt::NoPen);
    painter->fillPath(path, Qt::black);

    // draw points
    QBrush backgroundBrush(backgroundColor, Qt::SolidPattern);
    QBrush activeBrush(accentColor, Qt::SolidPattern);

    QColor hoverColor(accentColor);
    hoverColor.setAlpha(150);
    QBrush hoverBrush(hoverColor, Qt::SolidPattern);

    for (int i = 0; i < m_points.size(); ++i) {
        QPointF pos = pointCoord(m_points.at(i));

        bool focused = m_focusedPointIndex.has_value() && m_focusedPointIndex.value() == i;
        bool selected = m_currentPointIndex.has_value() && m_currentPointIndex.value() == i;
        bool hovered = m_hoverPointIndex.has_value() && m_hoverPointIndex.value() == i;

        if (!focused && !selected && !hovered) {
            painter->setBrush(activeBrush);
            painter->drawEllipse(pos, GRIP_RADIUS, GRIP_RADIUS);

            painter->setBrush(backgroundBrush);
            painter->drawEllipse(pos, GRIP_CENTER_RADIUS, GRIP_CENTER_RADIUS);
        } else if (focused) {
            QColor fontPrimaryColor(currentTheme.values[FONT_PRIMARY_COLOR].toString());
            QBrush fontPrimaryBrush(fontPrimaryColor, Qt::SolidPattern);
            painter->setBrush(fontPrimaryBrush);
            painter->drawEllipse(pos, GRIP_FOCUS_RADIUS, GRIP_FOCUS_RADIUS);

            painter->setBrush(backgroundBrush);
            painter->drawEllipse(pos, GRIP_SELECTED_RADIUS, GRIP_SELECTED_RADIUS);

            painter->setBrush(activeBrush);
            painter->drawEllipse(pos, GRIP_RADIUS, GRIP_RADIUS);
        } else if (selected) {
            painter->setBrush(backgroundBrush);
            painter->drawEllipse(pos, GRIP_SELECTED_RADIUS, GRIP_SELECTED_RADIUS);

            painter->setBrush(activeBrush);
            painter->drawEllipse(pos, GRIP_RADIUS, GRIP_RADIUS);
        } else if (hovered) {
            painter->setBrush(activeBrush);
            painter->drawEllipse(pos, GRIP_RADIUS, GRIP_RADIUS);

            painter->setBrush(backgroundBrush);
            painter->drawEllipse(pos, GRIP_CENTER_RADIUS, GRIP_CENTER_RADIUS);

            painter->setBrush(hoverBrush);
            painter->drawEllipse(pos, GRIP_CENTER_RADIUS, GRIP_CENTER_RADIUS);
        }
    }
}

bool BraceDesignerCanvas::isPointIndexValid(int index) const
{
    return index >= 0 && index < m_points.size();
}

std::optional<int> BraceDesignerCanvas::pointIndex(const QPointF& point) const
{
    const int numberOfPoints = m_points.size();

    for (int i = 0; i < numberOfPoints; ++i) {
        QPointF _point = pointCoord(m_points.at(i));

        if (std::pow((point.x() - _point.x()), 2) + std::pow((point.y() - _point.y()), 2)
            < std::pow(GRIP_CENTER_RADIUS, 2)) {
            return i;
        }
    }

    return std::nullopt;
}

QPointF BraceDesignerCanvas::pointCoord(const BracePoint& point) const
{
    qreal x = point.relative.x() * m_scale.width() + point.absolute.x();
    qreal y = point.relative.y() * m_scale.height() + point.absolute.y();
    return QPointF(x, y);
}

QString BraceDesignerCanvas::pointAccessibleName(const BracePoint& point)
{
    auto formatAxis = [](qreal rel, qreal abs) -> QString {
        int relative = std::lrint(rel * 100);
        return QString("%1%, %2sp").arg(QString::number(relative), QString::number(abs));
    };
    QString x = formatAxis(point.relative.x(), point.absolute.x()); // we want the ratio not the scaled relative value
    QString y = formatAxis(point.relative.y(), point.absolute.y());
    return muse::qtrc("notation", "Vertical: %1, Horizontal: %2").arg(x, y);
}

void BraceDesignerCanvas::updatePointAccessibleName(int index)
{
    if (!isPointIndexValid(index)) {
        return;
    }

    muse::ui::AccessibleItem* accItem = m_pointsAccessibleItems[index];
    if (accItem) {
        accItem->setName(pointAccessibleName(m_points.at(index)));
        accItem->accessiblePropertyChanged().send(IAccessible::Property::Name, muse::Val());
    }

    m_needVoicePointName = false;
}

bool BraceDesignerCanvas::movePoint(int pointIndex, BracePoint toPoint)
{
    if (!isPointIndexValid(pointIndex)) {
        return false;
    }

    BracePoint& currentPoint = m_points[pointIndex];

    // lock start and end point to the vertical center.
    // This is so we get a closed bath (upon mirroring)
    if (isFirstOrLastPoint(pointIndex)) {
        toPoint.relative.ry() = 0.0;
        toPoint.absolute.ry() = 0.0;
    }

    if (currentPoint == toPoint) {
        return false;
    }

    updatePointAccessibleName(pointIndex);
    return true;
}

void BraceDesignerCanvas::setFocusedPointIndex(int index)
{
    if (m_focusedPointIndex.has_value()) {
        m_pointsAccessibleItems[m_focusedPointIndex.value()]->setState(muse::ui::AccessibleItem::State::Focused, false);
    }

    bool isIndexValid = isPointIndexValid(index);
    m_focusedPointIndex = isIndexValid ? std::make_optional(index) : std::nullopt;

    if (!isIndexValid) {
        return;
    }

    m_needVoicePointName = true;
    updatePointAccessibleName(index);

    m_pointsAccessibleItems[index]->setState(muse::ui::AccessibleItem::State::Focused, true);
}

muse::ui::AccessibleItem* BraceDesignerCanvas::accessibleParent() const
{
    return m_accessibleParent;
}

void BraceDesignerCanvas::setAccessibleParent(muse::ui::AccessibleItem* parent)
{
    if (m_accessibleParent == parent) {
        return;
    }

    m_accessibleParent = parent;

    for (muse::ui::AccessibleItem* item : m_pointsAccessibleItems) {
        item->setAccessibleParent(m_accessibleParent);
    }

    emit accessibleParentChanged();
}

bool BraceDesignerCanvas::isFirstOrLastPoint(int index)
{
    return index == 0 || index == m_points.size() - 1;
}

bool BraceDesignerCanvas::removeRows(int row, int count, const QModelIndex& parent)
{
    if (!m_isRemovingAvailable) {
        return false;
    }

    // BraceDesignerCanvas* parentItem = modelIndexToItem(parent);

    // if (!parentItem) {
        // parentItem = m_rootItem;
    // }

    setLoadingBlocked(true);
    beginRemoveRows(parent, row, row + count - 1);

    // parentItem->removeChildren(row, count, true);

    endRemoveRows();
    setLoadingBlocked(false);

    emit isEmptyChanged();

    return true;
}

void BraceDesignerCanvas::clear()
{
    // TRACEFUNC;

    // beginResetModel();
    // deleteItems();
    // endResetModel();

    // emit isEmptyChanged();
    // emit isAddingAvailableChanged(false);
}

void BraceDesignerCanvas::deleteItems()
{
    clearSelection();
    setPointListInternal(BracePoints());
}

void BraceDesignerCanvas::load()
{
    if (m_isLoadingBlocked) {
        return;
    }

    TRACEFUNC;

    beginResetModel();

    clearSelection(); // since points are already bound and we don't need to collect from engraving, this should be fine???

    endResetModel();


    emit isEmptyChanged();
    // emit isAddingAvailableChanged(true);
}

QVariant BraceDesignerCanvas::data(const QModelIndex& index, int role) const
{
    int row = index.row();

    if (!isPointIndexValid(row) && role != ItemRole) {
        return QVariant();
    }

    return m_points.at(row).toMap(); // is this a qvariant?
}

bool BraceDesignerCanvas::setData(const QModelIndex& index, const QVariant& value, int role)
{
    int row = index.row();

    if (!isPointIndexValid(row) && role != ItemRole) {
        return false;
    }

    BracePoints points = m_points;
    BracePoint& point = points.at(row);
    point = BracePoint::fromMap(value);
    setPointListInternal(points);
    clearSelection();
    m_selection->select(index);
    emit dataChanged(index, index, { ItemRole }); // needed?
    emit selectionChanged();
    return true;
}

int BraceDesignerCanvas::rowCount(const QModelIndex&) const
{
    return m_points.size();
}

void BraceDesignerCanvas::selectRow(const QModelIndex& rowIndex)
{
    m_selectionModel->select(rowIndex);
}

void BraceDesignerCanvas::clearSelection()
{
    m_selectionModel->clear();
}

void BraceDesignerCanvas::moveSelectedRowsUp()
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

void BraceDesignerCanvas::moveSelectedRowsDown()
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

void BraceDesignerCanvas::removeSelectedRows()
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

void BraceDesignerCanvas::insertNewItem(int index)
{
    // todo: when there's only one point, we should append instead of inserting before it
    BracePoint p = isPointIndexValid(index) ? m_points.at(index) : BracePoint(BracePoint::PointType::Move, QPointF(), QPointF());
    if (isPointIndexValid(index + 1)) {
        p.relative.rx() = (p.relative.x() + m_points.at(index + 1).relative.x()) / 2;
        p.relative.ry() = (p.relative.y() + m_points.at(index + 1).relative.y()) / 2;
        p.absolute.rx() = (p.absolute.x() + m_points.at(index + 1).absolute.x()) / 2;
        p.absolute.ry() = (p.absolute.y() + m_points.at(index + 1).absolute.y()) / 2;
    }
    BracePoints list = m_points;
    list.insert(index, p);
    setPointListInternal(list);
}

int BraceDesignerCanvas::indexOf(const BracePoint& point) const
{
    return m_points.indexOf(point);
}

bool BraceDesignerCanvas::moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent,
                                    int destinationChild)
{
    setLoadingBlocked(true);
    DEFER {
        setLoadingBlocked(false);
    };

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

QVariantMap BraceDesignerCanvas::modelIndexToItem(const QModelIndex& index) const
{
    return data(index, ItemRole);
}

void BraceDesignerCanvas::startActiveDrag()
{
    m_dragInProgress = true;
}

void BraceDesignerCanvas::endActiveDrag()
{
    setLoadingBlocked(true);

    // here we would commit the change, but since we're only working with the qt model we can do nothing here instead

    // m_dragSourceParentItem = nullptr;
    // m_activeDragMoveParams = MoveParams();
    m_dragInProgress = false;

    setLoadingBlocked(false);

    // updateRearrangementAvailability(); //perhaps needed? not in the original code.
}

QItemSelectionModel* BraceDesignerCanvas::selectionModel() const
{
    return m_selectionModel;
}

/*QModelIndex BraceDesignerCanvas::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    const BraceDesignerCanvas* parentItem = nullptr;

    if (!parent.isValid()) {
        parentItem = m_rootItem;
    } else {
        parentItem = modelIndexToItem(parent);
    }

    if (!parentItem) {
        return QModelIndex();
    }

    BraceDesignerCanvas* childItem = parentItem->childAtRow(row);

    if (childItem) {
        return createIndex(row, column, childItem);
    }

    return QModelIndex();
}*/

int BraceDesignerCanvas::columnCount(const QModelIndex&) const
{
    return 1;
}

void BraceDesignerCanvas::setIsMovingUpAvailable(bool isMovingUpAvailable)
{
    if (m_isMovingUpAvailable == isMovingUpAvailable) {
        return;
    }

    m_isMovingUpAvailable = isMovingUpAvailable;
    emit isMovingUpAvailableChanged(m_isMovingUpAvailable);
}

void BraceDesignerCanvas::setIsMovingDownAvailable(bool isMovingDownAvailable)
{
    if (m_isMovingDownAvailable == isMovingDownAvailable) {
        return;
    }

    m_isMovingDownAvailable = isMovingDownAvailable;
    emit isMovingDownAvailableChanged(m_isMovingDownAvailable);
}

bool BraceDesignerCanvas::isMovingUpAvailable() const
{
    return m_isMovingUpAvailable;
}

bool BraceDesignerCanvas::isMovingDownAvailable() const
{
    return m_isMovingDownAvailable;
}

bool BraceDesignerCanvas::isRemovingAvailable() const
{
    return m_isRemovingAvailable;
}

bool BraceDesignerCanvas::isEmpty() const
{
    return m_rootItem ? m_rootItem->isEmpty() : true;
}

void BraceDesignerCanvas::updateRearrangementAvailability()
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

void BraceDesignerCanvas::updateMovingUpAvailability(bool isSelectionMovable, const QModelIndex& firstSelectedRowIndex)
{
    bool isRowInBoundaries = firstSelectedRowIndex.isValid() && firstSelectedRowIndex.row() > 0;
    setIsMovingUpAvailable(isSelectionMovable && isRowInBoundaries);
}

void BraceDesignerCanvas::updateMovingDownAvailability(bool isSelectionMovable, const QModelIndex& lastSelectedRowIndex)
{
    bool isRowInBoundaries = lastSelectedRowIndex.isValid() && lastSelectedRowIndex.row() + 1 < m_points.size();
    setIsMovingDownAvailable(isSelectionMovable && isRowInBoundaries);
}

void BraceDesignerCanvas::updateRemovingAvailability()
{
    const QModelIndexList selectedIndexes = m_selectionModel->selectedIndexes();

    bool isRemovingAvailable = !selectedIndexes.empty();

    for (const QModelIndex& index : selectedIndexes) {
        isRemovingAvailable = isRemovingAvailable && !isFirstOrLastPoint(index.row());

        if (!isRemovingAvailable) {
            break;
        }
    }

    setIsRemovingAvailable(isRemovingAvailable);
}

void BraceDesignerCanvas::setItemsSelected(const QModelIndexList& indexes, bool selected)
{
    for (const QModelIndex& index : indexes) {
        m_selection->select(modelIndex, selected ? QItemSelectionModel::Select : QItemSelectionModel::Deselect);
    }
}

QModelIndex BraceDesignerCanvas::index(int row, int column, const QModelIndex& model) const
{
    if (hasIndex(row, column, model && isPointIndexValid(row)) {
        return createIndex(row, column, m_points.at(row).toMap());
    }
    return QModelIndex();
}

void BraceDesignerCanvas::setLoadingBlocked(bool blocked)
{
    m_isLoadingBlocked = blocked;

    if (!m_isLoadingBlocked && m_notationChangedWhileLoadingWasBlocked) {
        onNotationChanged();
    }
}
