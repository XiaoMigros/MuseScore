/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited
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

#include "noteanchoredlinesettingsmodel.h"

#include "types/commontypes.h"
#include "types/linetypes.h"

#include "translation.h"

#include "ui/view/iconcodes.h"

using namespace mu::inspector;
using namespace mu::engraving;

using IconCode = muse::ui::IconCode::Code;

NoteAnchoredLineSettingsModel::NoteAnchoredLineSettingsModel(QObject* parent, IElementRepositoryService* repository)
    : AbstractInspectorModel(parent, repository, ElementType::NOTE_ANCHORED_LINE)
{
    setTitle(muse::qtrc("inspector", "Note-anchored line"));
    setModelType(InspectorModelType::TYPE_NOTE_ANCHORED_LINE);
    // setIcon(muse::ui::IconCode::Code::NOTE_ANCHORED_LINE);

    createProperties();
}

PropertyItem* NoteAnchoredLineSettingsModel::avoidNotes() const
{
    return m_avoidNotes;
}

PropertyItem* NoteAnchoredLineSettingsModel::lineStyle() const
{
    return m_lineStyle;
}

PropertyItem* NoteAnchoredLineSettingsModel::thickness() const
{
    return m_thickness;
}

PropertyItem* NoteAnchoredLineSettingsModel::dashLineLength() const
{
    return m_dashLineLength;
}

PropertyItem* NoteAnchoredLineSettingsModel::dashGapLength() const
{
    return m_dashGapLength;
}

void NoteAnchoredLineSettingsModel::createProperties()
{
    m_avoidNotes = buildPropertyItem(Pid::LAYOUT_GLISS_STYLE);

    m_lineStyle = buildPropertyItem(Pid::LINE_STYLE, [this](const Pid pid, const QVariant& newValue) {
        onPropertyValueChanged(pid, newValue);
        onUpdateLinePropertiesAvailability();
    });

    m_thickness = buildPropertyItem(Pid::LINE_WIDTH);
    m_dashLineLength = buildPropertyItem(Pid::DASH_LINE_LEN);
    m_dashGapLength = buildPropertyItem(Pid::DASH_GAP_LEN);
}

void NoteAnchoredLineSettingsModel::loadProperties()
{
    static const PropertyIdSet propertyIdSet {
        Pid::LINE_STYLE,
        Pid::LINE_WIDTH,
        Pid::DASH_LINE_LEN,
        Pid::DASH_GAP_LEN,
        Pid::LAYOUT_GLISS_STYLE
    };

    loadProperties(propertyIdSet);
}

void NoteAnchoredLineSettingsModel::resetProperties()
{
    m_avoidNotes->resetToDefault();
    m_lineStyle->resetToDefault();
    m_thickness->resetToDefault();
    m_dashLineLength->resetToDefault();
    m_dashGapLength->resetToDefault();
}

void NoteAnchoredLineSettingsModel::onUpdateLinePropertiesAvailability()
{
    auto currentStyle = static_cast<LineTypes::LineStyle>(m_lineStyle->value().toInt());
    bool areDashPropertiesAvailable = currentStyle == LineTypes::LineStyle::LINE_STYLE_DASHED;

    m_dashLineLength->setIsEnabled(areDashPropertiesAvailable);
    m_dashGapLength->setIsEnabled(areDashPropertiesAvailable);
}

void NoteAnchoredLineSettingsModel::onNotationChanged(const PropertyIdSet& changedPropertyIdSet, const StyleIdSet&)
{
    loadProperties(changedPropertyIdSet);
}

void NoteAnchoredLineSettingsModel::loadProperties(const PropertyIdSet& propertyIdSet)
{
    if (muse::contains(propertyIdSet, Pid::LAYOUT_GLISS_STYLE)) {
        loadPropertyItem(m_avoidNotes);
    }

    if (muse::contains(propertyIdSet, Pid::LINE_STYLE)) {
        loadPropertyItem(m_lineStyle);
    }

    if (muse::contains(propertyIdSet, Pid::LINE_WIDTH)) {
        loadPropertyItem(m_thickness, formatDoubleFunc);
    }

    if (muse::contains(propertyIdSet, Pid::DASH_LINE_LEN)) {
        loadPropertyItem(m_dashLineLength, formatDoubleFunc);
    }

    if (muse::contains(propertyIdSet, Pid::DASH_GAP_LEN)) {
        loadPropertyItem(m_dashGapLength, formatDoubleFunc);
    }

    onUpdateLinePropertiesAvailability();
}
