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

#include "measurerepeat.h"

#include "translation.h"

#include "style/style.h"

#include "measure.h"
#include "staff.h"
#include "text.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
static const ElementStyle measureRepeatStyle {
    { Sid::measureRepeatOffset,               Pid::MEASURE_REPEAT_NUMBER_POS },
    { Sid::measureRepeatFontFace,             Pid::FONT_FACE },
    { Sid::measureRepeatFontSize,             Pid::FONT_SIZE },
    { Sid::measureRepeatFontStyle,            Pid::FONT_STYLE },
    { Sid::measureRepeatAlign,                Pid::ALIGN },
    { Sid::measureRepeatFontSpatiumDependent, Pid::SIZE_SPATIUM_DEPENDENT },
};

//---------------------------------------------------------
//   MeasureRepeat
//---------------------------------------------------------

MeasureRepeat::MeasureRepeat(Segment* parent)
    : Rest(ElementType::MEASURE_REPEAT, parent), m_numMeasures(0)
{
    m_number = nullptr;
    // however many measures the group, the element itself is always exactly the duration of its containing measure
    setDurationType(DurationType::V_MEASURE);
    if (parent) {
        initElementStyle(&measureRepeatStyle);
    }
}

MeasureRepeat::MeasureRepeat(const MeasureRepeat& r)
    : Rest(r)
{
    m_numMeasures = r.m_numMeasures;
    m_numberPos   = r.m_numberPos;
    // recreated on layout
    m_number = nullptr;
}

//---------------------------------------------------------
//   ~MeasureRepeat
//---------------------------------------------------------

MeasureRepeat::~MeasureRepeat()
{
    delete m_number;
}

//---------------------------------------------------------
//   setSelected
//---------------------------------------------------------

void MeasureRepeat::setSelected(bool f)
{
    EngravingItem::setSelected(f);
    if (m_number) {
        m_number->setSelected(f);
    }
}

//---------------------------------------------------------
//   setVisible
//---------------------------------------------------------

void MeasureRepeat::setVisible(bool f)
{
    EngravingItem::setVisible(f);
    if (m_number) {
        m_number->setVisible(f);
    }
}

//---------------------------------------------------------
//   setColor
//---------------------------------------------------------

void MeasureRepeat::setColor(const Color& col)
{
    EngravingItem::setColor(col);
    if (m_number) {
        m_number->setColor(col);
    }
}

//---------------------------------------------------------
//   resetNumberProperty
//   reset number properties to default values
//   Set FONT_ITALIC to true, because for MeasureRepeats number should be italic
//---------------------------------------------------------

void MeasureRepeat::resetNumberProperty()
{
    resetNumberProperty(m_number);
}

void MeasureRepeat::resetNumberProperty(Text* number)
{
    for (auto p : { Pid::FONT_FACE, Pid::FONT_STYLE, Pid::FONT_SIZE, Pid::ALIGN, Pid::SIZE_SPATIUM_DEPENDENT }) {
        number->resetProperty(p);
    }
}

void MeasureRepeat::setNumMeasures(int n)
{
    IF_ASSERT_FAILED(n <= MAX_NUM_MEASURES) {
        m_numMeasures = MAX_NUM_MEASURES;
        return;
    }

    m_numMeasures = n;
}

//---------------------------------------------------------
//   firstMeasureOfGroup
//---------------------------------------------------------

Measure* MeasureRepeat::firstMeasureOfGroup() const
{
    return measure()->firstOfMeasureRepeatGroup(staffIdx());
}

const Measure* MeasureRepeat::referringMeasure(const Measure* measure) const
{
    IF_ASSERT_FAILED(measure) {
        return nullptr;
    }

    const Measure* firstMeasure = firstMeasureOfGroup();
    if (!firstMeasure) {
        return nullptr;
    }

    const Measure* referringMeasure = firstMeasure->prevMeasure();

    int measuresBack = m_numMeasures - measure->measureRepeatCount(staffIdx());

    for (int i = 0; i < measuresBack && referringMeasure; ++i) {
        referringMeasure = referringMeasure->prevMeasure();
    }

    return referringMeasure;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue MeasureRepeat::propertyDefault(Pid propertyId) const
{
    switch (propertyId) {
    case Pid::MEASURE_REPEAT_NUMBER_POS:
        return style().styleV(Sid::measureRepeatOffset);
    case Pid::ALIGN:
        return style().styleV(Sid::measureRepeatAlign);
    case Pid::FONT_FACE:
        return style().styleV(Sid::measureRepeatFontFace);
    case Pid::FONT_SIZE:
        return style().styleV(Sid::measureRepeatFontSize);
    case Pid::FONT_STYLE:
        return style().styleV(Sid::measureRepeatFontStyle);
    case Pid::SIZE_SPATIUM_DEPENDENT:
        return style().styleV(Sid::measureRepeatFontSpatiumDependent);
    default:
        return Rest::propertyDefault(propertyId);
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue MeasureRepeat::getProperty(Pid propertyId) const
{
    switch (propertyId) {
    case Pid::SUBTYPE:
        return numMeasures();
    case Pid::MEASURE_REPEAT_NUMBER_POS:
        return numberPos();
    case Pid::FONT_SIZE:
    case Pid::FONT_FACE:
    case Pid::FONT_STYLE:
    case Pid::ALIGN:
    case Pid::SIZE_SPATIUM_DEPENDENT:
        return m_number ? m_number->getProperty(propertyId) : PropertyValue();
    default:
        return Rest::getProperty(propertyId);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool MeasureRepeat::setProperty(Pid propertyId, const PropertyValue& v)
{
    switch (propertyId) {
    case Pid::SUBTYPE:
        setNumMeasures(v.toInt());
        break;
    case Pid::MEASURE_REPEAT_NUMBER_POS:
        setNumberPos(v.value<PointF>());
        triggerLayout();
        break;
    case Pid::FONT_SIZE:
    case Pid::FONT_FACE:
    case Pid::FONT_STYLE:
    case Pid::ALIGN:
    case Pid::SIZE_SPATIUM_DEPENDENT:
        if (m_number) {
            m_number->setProperty(propertyId, v);
        }
        break;
    default:
        return Rest::setProperty(propertyId, v);
    }
    return true;
}

//---------------------------------------------------------
//   ticks
//---------------------------------------------------------

Fraction MeasureRepeat::ticks() const
{
    if (measure()) {
        return measure()->stretchedLen(staff());
    }
    return Fraction(0, 1);
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

String MeasureRepeat::accessibleInfo() const
{
    return muse::mtrc("engraving", "%1; Duration: %n measure(s)", nullptr, numMeasures()).arg(EngravingItem::accessibleInfo());
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString MeasureRepeat::subtypeUserName() const
{
    return muse::TranslatableString("engraving", "%n measure(s)", nullptr, numMeasures());
}

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid MeasureRepeat::getPropertyStyle(Pid propertyId) const
{
    if (propertyId == Pid::MEASURE_REPEAT_NUMBER_POS) {
        return Sid::measureRepeatOffset;
    }
    return Rest::getPropertyStyle(propertyId);
}
}
