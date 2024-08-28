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

#include "notelinebase.h"

#include <cmath>
#include <algorithm>

#include "types/typesconv.h"

#include "chord.h"
#include "measure.h"
#include "note.h"
#include "part.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "system.h"
#include "utils.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
NoteLineBaseSegment::NoteLineBaseSegment(const ElementType& type, NoteLineBase* sp, System* parent)
    : LineSegment(type, sp, parent, ElementFlag::MOVABLE)
{
}

EngravingItem* NoteLineBaseSegment::propertyDelegate(Pid pid)
{
    switch (pid) {
    case Pid::LAYOUT_GLISS_STYLE:
        return noteLineBase();
    default:
        return LineSegment::propertyDelegate(pid);
    }
}

NoteLineBase::NoteLineBase(const ElementType& type, EngravingItem* parent)
    : SLine(type, parent, ElementFlag::MOVABLE)
{
    setAnchor(Spanner::Anchor::NOTE);
    setDiagonal(true);
}

NoteLineBase::NoteLineBase(const NoteLineBase& nlb)
    : SLine(nlb)
{
    m_layoutGlissStyle = nlb.m_layoutGlissStyle;
}

LineSegment* NoteLineBase::createLineSegment(System* parent)
{
    NoteLineBaseSegment* seg = new NoteLineBaseSegment(type(), this, parent);
    seg->setTrack(track());
    seg->setColor(color());
    return seg;
}

Sid NoteLineBase::getPropertyStyle(Pid id) const
{
    return SLine::getPropertyStyle(id);
}

void NoteLineBase::addLineAttachPoints()
{
    NoteLineBaseSegment* frontSeg = toNoteLineBaseSegment(frontSegment());
    NoteLineBaseSegment* backSeg = toNoteLineBaseSegment(backSegment());
    Note* startNote = nullptr;
    Note* endNote = nullptr;
    if (startElement() && startElement()->isNote()) {
        startNote = toNote(startElement());
    }
    if (endElement() && endElement()->isNote()) {
        endNote = toNote(endElement());
    }
    if (!frontSeg || !backSeg || !startNote || !endNote) {
        return;
    }
    double startX = frontSeg->ldata()->pos().x();
    double endX = backSeg->pos2().x() + backSeg->ldata()->pos().x(); // because pos2 is relative to ipos
    // Here we don't pass y() because its value is unreliable during the first stages of layout.
    // The y() is irrelevant anyway for horizontal spacing.
    startNote->addLineAttachPoint(PointF(startX, 0.0), this);
    endNote->addLineAttachPoint(PointF(endX, 0.0), this);
}

//---------------------------------------------------------
//   guessInitialNote
//
//    Used while reading old scores (either 1.x or transitional 2.0) to determine (guess!)
//    the glissando initial note from its final chord. Returns the top note of previous chord
//    of the same instrument, preferring the chord in the same track as chord, if it exists.
//
//    CANNOT be called if the final chord and/or its segment do not exist yet in the score
//
//    Parameter:  chord: the chord this glissando ends into
//    Returns:    the top note in a suitable previous chord or nullptr if none found.
//---------------------------------------------------------

Note* NoteLineBase::guessInitialNote(Chord* chord)
{
    if (chord->isGraceBefore()) {
        // for grace notes before, previous chord is previous chord of parent chord
        // move unto parent chord and proceed to standard case
        if (chord->explicitParent() && chord->explicitParent()->isChord()) {
            chord = toChord(chord->explicitParent());
        } else {
            return nullptr;
        }
    } else if (chord->isGraceAfter()) {
        // for grace notes after, return top note of parent chord
        if (chord->explicitParent() && chord->explicitParent()->isChord()) {
            return toChord(chord->explicitParent())->upNote();
        } else {
            return nullptr;
        }
    } else {
        // if chord has grace notes before, the last one is the previous note
        std::vector<Chord*> graces = chord->graceNotesBefore();
        if (!graces.empty()) {
            return graces.back()->upNote();
        }
    }

    // standard case (NORMAL or grace before chord)

    // if parent not a segment, can't locate a target note
    if (!chord->explicitParent()->isSegment()) {
        return nullptr;
    }

    track_idx_t chordTrack = chord->track();
    Segment* segm = chord->segment();
    Part* part = chord->part();
    if (segm != nullptr) {
        segm = segm->prev1();
    }
    while (segm) {
        // if previous segment is a ChordRest segment
        if (segm->segmentType() == SegmentType::ChordRest) {
            Chord* target = nullptr;
            // look for a Chord in the same track
            if (segm->element(chordTrack) && segm->element(chordTrack)->isChord()) {
                target = toChord(segm->element(chordTrack));
            } else {                 // if no same track, look for other chords in the same instrument
                for (EngravingItem* currChord : segm->elist()) {
                    if (currChord && currChord->isChord() && toChord(currChord)->part() == part) {
                        target = toChord(currChord);
                        break;
                    }
                }
            }
            // if we found a target previous chord
            if (target) {
                // if chord has grace notes after, the last one is the previous note
                std::vector<Chord*> graces = target->graceNotesAfter();
                if (!graces.empty()) {
                    return graces.back()->upNote();
                }
                return target->upNote();              // if no grace after, return top note
            }
        }
        segm = segm->prev1();
    }
    LOGD("no start note for noteLineBase found");
    return nullptr;
}

//---------------------------------------------------------
//   guessFinalNote
//
//    Used by NoteLineBase elements and Guitar Bends to determine
//    which note to end on when only the starting note is provided
//---------------------------------------------------------

Note* NoteLineBase::guessFinalNote(Chord* chord, Note* startNote)
{
    if (chord->isGraceBefore()) {
        Chord* parentChord = toChord(chord->parent());
        GraceNotesGroup& gracesBefore = parentChord->graceNotesBefore();
        auto positionOfThis = std::find(gracesBefore.begin(), gracesBefore.end(), chord);
        if (positionOfThis != gracesBefore.end()) {
            auto nextPosition = ++positionOfThis;
            if (nextPosition != gracesBefore.end()) {
                return (*nextPosition)->upNote();
            }
        }
        return parentChord->upNote();
    } else if (chord->isGraceAfter()) {
        Chord* parentChord = toChord(chord->parent());
        GraceNotesGroup& gracesAfter = parentChord->graceNotesAfter();
        auto positionOfThis = std::find(gracesAfter.begin(), gracesAfter.end(), chord);
        if (positionOfThis != gracesAfter.end()) {
            auto nextPosition = ++positionOfThis;
            if (nextPosition != gracesAfter.end()) {
                return (*nextPosition)->upNote();
            }
        }
        chord = toChord(chord->parent());
    } else {
        std::vector<Chord*> graces = chord->graceNotesAfter();
        if (!graces.empty()) {
            return graces.front()->upNote();
        }
    }

    if (!chord->explicitParent()->isSegment()) {
        return nullptr;
    }

    Segment* segm = chord->score()->tick2rightSegment(chord->tick() + chord->actualTicks());
    while (segm && !segm->isChordRestType()) {
        segm = segm->next1();
    }

    if (!segm) {
        return nullptr;
    }

    track_idx_t chordTrack = chord->track();
    Part* part = chord->part();

    Chord* target = nullptr;
    if (segm->element(chordTrack) && segm->element(chordTrack)->isChord()) {
        target = toChord(segm->element(chordTrack));
    } else {
        for (EngravingItem* currChord : segm->elist()) {
            if (currChord && currChord->isChord() && toChord(currChord)->part() == part) {
                target = toChord(currChord);
                break;
            }
        }
    }

    if (target && !target->notes().empty()) {
        const std::vector<Chord*>& graces = target->graceNotesBefore();
        if (!graces.empty()) {
            return graces.front()->upNote();
        }
        // normal case: try to return the note in the next chord that is in the
        // same position as the start note relative to the end chord
        size_t startNoteIdx = muse::indexOf(chord->notes(), startNote);
        size_t endNoteIdx = std::min(startNoteIdx, target->notes().size() - 1);
        return target->notes().at(endNoteIdx);
    }

    LOGD("no end note for noteLineBase found");
    return nullptr;
}

PropertyValue NoteLineBase::getProperty(Pid propertyId) const
{
    switch (propertyId) {
    case Pid::LAYOUT_GLISS_STYLE:
        return layoutGlissStyle();
    default:
        break;
    }
    return SLine::getProperty(propertyId);
}

bool NoteLineBase::setProperty(Pid propertyId, const PropertyValue& v)
{
    switch (propertyId) {
    case Pid::LAYOUT_GLISS_STYLE:
        setLayoutGlissStyle(v.toBool());
        break;
    default:
        if (!SLine::setProperty(propertyId, v)) {
            return false;
        }
        break;
    }
    triggerLayout();
    return true;
}

PropertyValue NoteLineBase::propertyDefault(Pid propertyId) const
{
    switch (propertyId) {
    case Pid::LAYOUT_GLISS_STYLE:
        return isGlissando();
    default:
        break;
    }
    return SLine::propertyDefault(propertyId);
}
}
