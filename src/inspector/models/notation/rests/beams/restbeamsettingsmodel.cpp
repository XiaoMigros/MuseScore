/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited
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
#include "restbeamsettingsmodel.h"

#include "translation.h"

using namespace mu::inspector;
using namespace mu::engraving;

RestBeamSettingsModel::RestBeamSettingsModel(QObject* parent, IElementRepositoryService* repository)
    : AbstractInspectorModel(parent, repository)
{
    setModelType(InspectorModelType::TYPE_BEAM);
    setTitle(muse::qtrc("inspector", "Beam"));

    m_beamModesModel = new BeamModesModel(this, repository);
    m_beamModesModel->init();

    connect(m_beamModesModel->mode(), &PropertyItem::propertyModified, this, &AbstractInspectorModel::requestReloadPropertyItems);
}

QObject* RestBeamSettingsModel::beamModesModel() const
{
    return m_beamModesModel;
}

void RestBeamSettingsModel::createProperties()
{
    m_quarterSymbol = buildPropertyItem(mu::engraving::Pid::REST_QUARTER_SYMBOL);
    m_restVisualDuration = buildPropertyItem(mu::engraving::Pid::REST_VISUAL_DURATION,
                                             [this](const mu::engraving::Pid pid, const QVariant& newValue) {
        onPropertyValueChanged(pid, newValue);
        updateQuarterSymbolVisible();
    });
}

void RestBeamSettingsModel::requestElements()
{
    m_elementList = m_repository->findElementsByType(mu::engraving::ElementType::REST);
}

void RestBeamSettingsModel::loadProperties()
{
    loadPropertyItem(m_quarterSymbol);
    loadPropertyItem(m_restVisualDuration);
}

void RestBeamSettingsModel::resetProperties()
{
    m_quarterSymbol->resetToDefault();
    m_restVisualDuration->resetToDefault();
}

PropertyItem* RestBeamSettingsModel::quarterSymbol() const
{
    return m_quarterSymbol;
}

PropertyItem* RestBeamSettingsModel::restVisualDuration() const
{
    return m_restVisualDuration;
}

bool RestBeamSettingsModel::quarterSymbolVisible() const
{
    return m_quarterSymbolVisible;
}

void RestBeamSettingsModel::updateQuarterSymbolVisible()
{
    bool visible = false;
    for (EngravingItem* element : m_elementList) {
        engraving::EngravingItem* elementBase = element->elementBase();
        if (elementBase->isRest()) {
            engraving::Rest* rest = engraving::toRest(elementBase);
            if (rest->getSymbol(rest->durationType().type(), 0, 0) == rest->quarterSymbol()) {
                visible = true;
                break;
            }
        }
    }
    setQuarterSymbolVisible(visible);
}

void RestBeamSettingsModel::setQuarterSymbolVisible(bool visible)
{
    if (visible == m_quarterSymbolVisible) {
        return;
    }

    m_quarterSymbolVisible = visible;
    emit quarterSymbolVisibleChanged(m_quarterSymbolVisible);
}

void RestBeamSettingsModel::onCurrentNotationChanged()
{
    updateQuarterSymbolVisible();
    AbstractInspectorModel::onCurrentNotationChanged();

    m_beamModesModel->onCurrentNotationChanged();
}
