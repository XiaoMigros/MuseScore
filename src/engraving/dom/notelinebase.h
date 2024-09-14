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

#ifndef MU_ENGRAVING_NOTELINEBASE_H
#define MU_ENGRAVING_NOTELINEBASE_H

#include "engravingitem.h"
#include "line.h"
#include "property.h"
#include "types.h"

namespace mu::engraving {
class NoteLineBase;

//---------------------------------------------------------
//   @@ NoteLineBaseSegment
//---------------------------------------------------------

class NoteLineBaseSegment : public LineSegment
{
    OBJECT_ALLOCATOR(engraving, NoteLineBaseSegment)

public:
    NoteLineBaseSegment(const ElementType& type, NoteLineBase* sp, System* parent);

    NoteLineBase* noteLineBase() const { return toNoteLineBase(spanner()); }

    NoteLineBaseSegment* clone() const override { return new NoteLineBaseSegment(*this); }

    EngravingItem* propertyDelegate(Pid) override;
};

//---------------------------------------------------------
//   NoteLineBase
//---------------------------------------------------------

class NoteLineBase : public SLine
{
    OBJECT_ALLOCATOR(engraving, NoteLineBase)

public:
    static constexpr double NLB_PALETTE_WIDTH = 4.0;
    static constexpr double NLB_PALETTE_HEIGHT = 4.0;

    static Note* guessInitialNote(Chord* chord);
    static Note* guessFinalNote(Chord* chord, Note* startNote);

    NoteLineBase(const ElementType& type, EngravingItem* parent);
    NoteLineBase(const NoteLineBase&);

    bool layoutGlissStyle() const { return m_layoutGlissStyle; }
    void setLayoutGlissStyle(bool v) { m_layoutGlissStyle = v; }

    // overridden inherited methods
    NoteLineBase* clone() const override { return new NoteLineBase(*this); }

    LineSegment* createLineSegment(System* parent) override;

    // property/style methods
    Sid getPropertyStyle(Pid id) const override;
    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue&) override;
    PropertyValue propertyDefault(Pid propertyId) const override;
    void addLineAttachPoints();

private:

    bool m_layoutGlissStyle = isGlissando();
};
} // namespace mu::engraving

#endif
