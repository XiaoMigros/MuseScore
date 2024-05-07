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

#include "noteanchoredline.h"

#include "score.h"
#include "system.h"
#include "undo.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   noteAnchoredLineStyle
//---------------------------------------------------------

static const ElementStyle noteAnchoredLineStyle {
    { Sid::glissandoLineWidth, Pid::LINE_WIDTH },
};

//---------------------------------------------------------
//   NoteAnchoredLineSegment
//---------------------------------------------------------

NoteAnchoredLineSegment::NoteAnchoredLineSegment(NoteAnchoredLine* sp, System* parent)
    : NoteLineBaseSegment(ElementType::NOTE_ANCHORED_LINE_SEGMENT, sp, parent)
{
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingItem* NoteAnchoredLineSegment::propertyDelegate(Pid pid)
{
    switch (pid) {
    /*case Pid::GLISS_TEXT:
    case Pid::GLISS_SHOW_TEXT:
    case Pid::FONT_FACE:
    case Pid::FONT_SIZE:
    case Pid::FONT_STYLE:
    case Pid::LINE_WIDTH:
    case Pid::LAYOUT_GLISS_STYLE:
        return noteAnchoredLine();*/
    default:
        return NoteLineBaseSegment::propertyDelegate(pid);
    }
}

//---------------------------------------------------------
//   NoteAnchoredLine
//---------------------------------------------------------

NoteAnchoredLine::NoteAnchoredLine(EngravingItem* parent)
    : NoteLineBase(ElementType::NOTE_ANCHORED_LINE, parent)
{
    initElementStyle(&noteAnchoredLineStyle);
}

NoteAnchoredLine::NoteAnchoredLine(const NoteAnchoredLine& nal)
    : NoteLineBase(nal)
{
    /*_text           = g._text;
    _showText       = g._showText;
    _fontFace       = g._fontFace;
    _fontSize       = g._fontSize;
    _fontStyle      = g._fontStyle;*/
}

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

LineSegment* NoteAnchoredLine::createLineSegment(System* parent)
{
    NoteAnchoredLineSegment* seg = new NoteAnchoredLineSegment(this, parent);
    seg->setTrack(track());
    seg->setColor(color());
    return seg;
}

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid NoteAnchoredLine::getPropertyStyle(Pid id) const
{
    return NoteLineBase::getPropertyStyle(id);
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue NoteAnchoredLine::getProperty(Pid id) const
{
    return NoteLineBase::getProperty(id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool NoteAnchoredLine::setProperty(Pid id, const PropertyValue& v)
{
    return NoteLineBase::setProperty(id, v);
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue NoteAnchoredLine::propertyDefault(Pid id) const
{
    return NoteLineBase::propertyDefault(id);
}
} // namespace mu::engraving
