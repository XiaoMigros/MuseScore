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

#include <QPainterPath>

#include "engraving/dom/utils.h"

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

    uiConfig()->fontChanged().onNotify(this, [this]() { // needed?
        update();
    });

    connect(this, &BraceDesignerCanvas::enabledChanged, [this](){
        update();
    });

    connect(this, &BraceDesignerCanvas::showOutlineChanged, [this](){
        update();
    });

    setScale(QSizeF(1.0, 1.0)); // for now

    qApp->installEventFilter(this);
}

BraceDesignerCanvas::~BraceDesignerCanvas()
{
    muse::DeleteAll(m_pointsAccessibleItems);
}

QVariant BraceDesignerCanvas::pointList() const
{
    return bracePointsToQVariant(m_points);
}

QSizeF BraceDesignerCanvas::scale() const
{
    return m_scale;
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
    if (pointIndex == 0 || pointIndex == m_points.size()) {
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
