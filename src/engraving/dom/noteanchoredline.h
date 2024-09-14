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

#ifndef MU_ENGRAVING_NOTEANCHOREDLINE_H
#define MU_ENGRAVING_NOTEANCHOREDLINE_H

#include "engravingitem.h"
#include "line.h"
#include "notelinebase.h"
#include "property.h"
#include "types.h"

namespace mu::engraving {
class NoteAnchoredLine;

//---------------------------------------------------------
//   @@ NoteAnchoredLineSegment
//---------------------------------------------------------

class NoteAnchoredLineSegment final : public NoteLineBaseSegment
{
    OBJECT_ALLOCATOR(engraving, NoteAnchoredLineSegment)
    DECLARE_CLASSOF(ElementType::NOTE_ANCHORED_LINE_SEGMENT)

public:
    NoteAnchoredLineSegment(NoteAnchoredLine* sp, System* parent);

    NoteAnchoredLine* noteAnchoredLine() const { return toNoteAnchoredLine(spanner()); }

    NoteAnchoredLineSegment* clone() const override { return new NoteAnchoredLineSegment(*this); }
};

//---------------------------------------------------------
//   NoteAnchoredLine
//---------------------------------------------------------

class NoteAnchoredLine final : public NoteLineBase
{
    OBJECT_ALLOCATOR(engraving, NoteAnchoredLine)
    DECLARE_CLASSOF(ElementType::NOTE_ANCHORED_LINE)

public:
    NoteAnchoredLine(EngravingItem* parent);
    NoteAnchoredLine(const NoteAnchoredLine&);

    NoteAnchoredLine* clone() const override { return new NoteAnchoredLine(*this); }

    LineSegment* createLineSegment(System* parent) override;
};
} // namespace mu::engraving

#endif
