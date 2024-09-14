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
static const ElementStyle noteAnchoredLineStyle {
    { Sid::noteAnchoredLinesAvoidNotes, Pid::LAYOUT_GLISS_STYLE },
    { Sid::noteAnchoredLineWidth,       Pid::LINE_WIDTH },
    { Sid::noteAnchoredLineLineStyle,   Pid::LINE_STYLE },
    { Sid::noteAnchoredLineDashLineLen, Pid::DASH_LINE_LEN },
    { Sid::noteAnchoredLineDashGapLen,  Pid::DASH_GAP_LEN },
};

NoteAnchoredLineSegment::NoteAnchoredLineSegment(NoteAnchoredLine* sp, System* parent)
    : NoteLineBaseSegment(ElementType::NOTE_ANCHORED_LINE_SEGMENT, sp, parent)
{
}

NoteAnchoredLine::NoteAnchoredLine(EngravingItem* parent)
    : NoteLineBase(ElementType::NOTE_ANCHORED_LINE, parent)
{
    initElementStyle(&noteAnchoredLineStyle);
}

NoteAnchoredLine::NoteAnchoredLine(const NoteAnchoredLine& nal)
    : NoteLineBase(nal)
{
}

LineSegment* NoteAnchoredLine::createLineSegment(System* parent)
{
    NoteAnchoredLineSegment* seg = new NoteAnchoredLineSegment(this, parent);
    seg->setTrack(track());
    seg->setColor(color());
    return seg;
}
} // namespace mu::engraving
