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

#include "elements.h"

#include "engraving/dom/guitarbend.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/property.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/spacer.h"
#include "engraving/dom/system.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/dom/undo.h"

#include <QQmlListProperty>

// api
#include "fraction.h"
#include "part.h"
#include "scoreelement.h"

#include "log.h"

using namespace mu::engraving::apiv1;

//---------------------------------------------------------
//   EngravingItem::setOffsetX
//---------------------------------------------------------

void EngravingItem::setOffsetX(qreal offX)
{
    const qreal offY = element()->offset().y() / element()->spatium();
    set(mu::engraving::Pid::OFFSET, QPointF(offX, offY));
}

//---------------------------------------------------------
//   EngravingItem::setOffsetY
//---------------------------------------------------------

void EngravingItem::setOffsetY(qreal offY)
{
    const qreal offX = element()->offset().x() / element()->spatium();
    set(mu::engraving::Pid::OFFSET, QPointF(offX, offY));
}

static QRectF scaleRect(const mu::engraving::RectF& rect, double spatium)
{
    return QRectF(rect.x() / spatium, rect.y() / spatium, rect.width() / spatium, rect.height() / spatium);
}

//---------------------------------------------------------
//   EngravingItem::bbox
//   return the element bbox in spatium units, rather than in raster units as stored internally
//---------------------------------------------------------

QRectF EngravingItem::bbox() const
{
    return scaleRect(element()->ldata()->bbox(), element()->spatium());
}

void EngravingItem::setEID(QString eid)
{
    EID id = EID::fromStdString(eid.toStdString());
    element()->setEID(id);
}

QQmlListProperty<EngravingItem> EngravingItem::children(bool all)
{
    return wrapContainerProperty<EngravingItem>(this, &element()->childrenItems(all));
}

bool EngravingItem::up() const
{
    if (element()->isChordRest()) {
        return toChordRest(element())->ldata()->up;
    } else if (element()->isStem()) {
        return toStem(element())->up();
    } else if (element()->isSlur()) {
        return toSlur(element())->up();
    } else if (element()->isTie()) {
        return toTie(element())->up();
    } else if (element()->isSlurTieSegment()) {
        return toSlurTieSegment(element())->slurTie()->up();
    } else if (element()->isArticulation()) {
        return toArticulation(element())->ldata()->up;
    } else if (element()->isGuitarBend()) {
        return toGuitarBend(element())->ldata()->up();
    } else if (element()->isGuitarBendSegment()) {
        return toGuitarBendSegment(element())->guitarBend()->ldata()->up();
    } else if (element()->isBeam()) {
        return toBeam(element())->up();
    } else if (element()->type() == mu::engraving::ElementType::TREMOLO_TWOCHORD) {
        return item_cast<const TremoloTwoChord*>(element())->up();
    }
    return false;
}

FractionWrapper* EngravingItem::tick() const
{
    return wrap(element()->tick());
}

//---------------------------------------------------------
//   Segment::elementAt
//---------------------------------------------------------

EngravingItem* Segment::elementAt(int track)
{
    mu::engraving::EngravingItem* el = segment()->elementAt(track);
    if (!el) {
        return nullptr;
    }
    return wrap(el, Ownership::SCORE);
}

FractionWrapper* Segment::fraction() const
{
    return wrap(segment()->tick());
}

//---------------------------------------------------------
//   Note::setTpc
//---------------------------------------------------------

void Note::setTpc(int val)
{
    if (!tpcIsValid(val)) {
        LOGW("PluginAPI::Note::setTpc: invalid tpc: %d", val);
        return;
    }

    if (note()->concertPitch()) {
        set(Pid::TPC1, val);
    } else {
        set(Pid::TPC2, val);
    }
}

//---------------------------------------------------------
//   Note::isChildAllowed
///   Check if element type can be a child of note.
///   \since MuseScore 3.3.3
//---------------------------------------------------------

bool Note::isChildAllowed(mu::engraving::ElementType elementType)
{
    switch (elementType) {
    case ElementType::NOTEHEAD:
    case ElementType::NOTEDOT:
    case ElementType::FINGERING:
    case ElementType::SYMBOL:
    case ElementType::IMAGE:
    case ElementType::TEXT:
    case ElementType::BEND:
    case ElementType::TIE:
    case ElementType::PARTIAL_TIE:
    case ElementType::LAISSEZ_VIB:
    case ElementType::ACCIDENTAL:
    case ElementType::TEXTLINE:
    case ElementType::NOTELINE:
    case ElementType::GLISSANDO:
    case ElementType::HAMMER_ON_PULL_OFF:
        return true;
    default:
        return false;
    }
}

//---------------------------------------------------------
//   Note::add
///   \since MuseScore 3.3.3
//---------------------------------------------------------

void Note::add(apiv1::EngravingItem* wrapped)
{
    mu::engraving::EngravingItem* s = wrapped ? wrapped->element() : nullptr;
    if (s) {
        // Ensure that the object has the expected ownership
        if (wrapped->ownership() == Ownership::SCORE) {
            LOGW("Note::add: Cannot add this element. The element is already part of the score.");
            return;              // Don't allow operation.
        }
        // Score now owns the object.
        wrapped->setOwnership(Ownership::SCORE);

        addInternal(note(), s);
    }
}

//---------------------------------------------------------
//   Note::addInternal
///   \since MuseScore 3.3.3
//---------------------------------------------------------

void Note::addInternal(mu::engraving::Note* note, mu::engraving::EngravingItem* s)
{
    // Provide parentage for element.
    s->setScore(note->score());
    s->setParent(note);
    s->setTrack(note->track());

    if (s && isChildAllowed(s->type())) {
        // Create undo op and add the element.
        toScore(note->score())->undoAddElement(s);
    } else if (s) {
        LOGD("Note::add() not impl. %s", s->typeName());
    }
}

//---------------------------------------------------------
//   Note::remove
///   \since MuseScore 3.3.3
//---------------------------------------------------------

void Note::remove(apiv1::EngravingItem* wrapped)
{
    mu::engraving::EngravingItem* s = wrapped->element();
    if (!s) {
        LOGW("PluginAPI::Note::remove: Unable to retrieve element. %s", qPrintable(wrapped->name()));
    } else if (s->explicitParent() != note()) {
        LOGW("PluginAPI::Note::remove: The element is not a child of this note. Use removeElement() instead.");
    } else if (isChildAllowed(s->type())) {
        note()->score()->deleteItem(s);     // Create undo op and remove the element.
    } else {
        LOGD("Note::remove() not impl. %s", s->typeName());
    }
}

//---------------------------------------------------------
//   DurationElement::globalDuration
//---------------------------------------------------------

FractionWrapper* DurationElement::globalDuration() const
{
    return wrap(durationElement()->globalTicks());
}

//---------------------------------------------------------
//   DurationElement::actualDuration
//---------------------------------------------------------

FractionWrapper* DurationElement::actualDuration() const
{
    return wrap(durationElement()->actualTicks());
}

//---------------------------------------------------------
//   DurationElement::parentTuplet
//---------------------------------------------------------

Tuplet* DurationElement::parentTuplet()
{
    return wrap<Tuplet>(durationElement()->tuplet());
}

//---------------------------------------------------------
//   Chord::setPlayEventType
//---------------------------------------------------------

void Chord::setPlayEventType(mu::engraving::PlayEventType v)
{
    // Only create undo operation if the value has changed.
    if (v != chord()->playEventType()) {
        chord()->score()->setPlaylistDirty();
        chord()->score()->undo(new ChangeChordPlayEventType(chord(), v));
    }
}

//---------------------------------------------------------
//   Chord::add
//---------------------------------------------------------

void Chord::add(apiv1::EngravingItem* wrapped)
{
    mu::engraving::EngravingItem* s = wrapped ? wrapped->element() : nullptr;
    if (s) {
        // Ensure that the object has the expected ownership
        if (wrapped->ownership() == Ownership::SCORE) {
            LOGW("Chord::add: Cannot add this element. The element is already part of the score.");
            return;              // Don't allow operation.
        }
        // Score now owns the object.
        wrapped->setOwnership(Ownership::SCORE);

        addInternal(chord(), s);
    }
}

//---------------------------------------------------------
//   Chord::addInternal
//---------------------------------------------------------

void Chord::addInternal(mu::engraving::Chord* chord, mu::engraving::EngravingItem* s)
{
    // Provide parentage for element.
    s->setScore(chord->score());
    s->setParent(chord);
    // If a note, ensure the element has proper Tpc values. (Will crash otherwise)
    if (s->isNote()) {
        s->setTrack(chord->track());
        toNote(s)->setTpcFromPitch();
    }
    // Create undo op and add the element.
    chord->score()->undoAddElement(s);
}

//---------------------------------------------------------
//   Chord::remove
//---------------------------------------------------------

void Chord::remove(apiv1::EngravingItem* wrapped)
{
    mu::engraving::EngravingItem* s = wrapped->element();
    if (!s) {
        LOGW("PluginAPI::Chord::remove: Unable to retrieve element. %s", qPrintable(wrapped->name()));
    } else if (s->explicitParent() != chord()) {
        LOGW("PluginAPI::Chord::remove: The element is not a child of this chord. Use removeElement() instead.");
    } else if (chord()->notes().size() <= 1 && s->type() == ElementType::NOTE) {
        LOGW("PluginAPI::Chord::remove: Removal of final note is not allowed.");
    } else {
        chord()->score()->deleteItem(s);     // Create undo op and remove the element.
    }
}

EngravingItem* Measure::vspacerUp(int staffIdx)
{
    mu::engraving::EngravingItem* el = measure()->vspacerUp(static_cast<staff_idx_t>(staffIdx));
    return el ? wrap(el, Ownership::SCORE) : nullptr;
}

EngravingItem* Measure::vspacerDown(int staffIdx)
{
    mu::engraving::EngravingItem* el = measure()->vspacerDown(static_cast<staff_idx_t>(staffIdx));
    return el ? wrap(el, Ownership::SCORE) : nullptr;
}

FractionWrapper* MeasureBase::tick() const
{
    return wrap(measureBase()->tick());
}

void MeasureBase::add(apiv1::EngravingItem* wrapped)
{
    mu::engraving::EngravingItem* s = wrapped ? wrapped->element() : nullptr;
    if (s) {
        // Ensure that the object has the expected ownership
        if (wrapped->ownership() == Ownership::SCORE) {
            LOGW("MeasureBase::add: Cannot add this element. The element is already part of the score.");
            return;              // Don't allow operation.
        }
        // Score now owns the object.
        wrapped->setOwnership(Ownership::SCORE);

        addInternal(measureBase(), s);
    }
}

void MeasureBase::addInternal(mu::engraving::MeasureBase* measureBase, mu::engraving::EngravingItem* s)
{
    s->setScore(measureBase->score());
    s->setParent(measureBase);
    measureBase->score()->undoAddElement(s);
}

//---------------------------------------------------------
//   Chord::remove
//---------------------------------------------------------

void MeasureBase::remove(apiv1::EngravingItem* wrapped)
{
    mu::engraving::EngravingItem* s = wrapped->element();
    if (!s) {
        LOGW("PluginAPI::MeasureBase::remove: Unable to retrieve element. %s", qPrintable(wrapped->name()));
    } else if (s->explicitParent() != measureBase()) {
        LOGW("PluginAPI::MeasureBase::remove: The element is not a child of this measure base. Use removeElement() instead.");
    } else {
        measureBase()->score()->deleteItem(s);     // Create undo op and remove the element.
    }
}

QRectF System::bbox(int staffIdx)
{
    mu::engraving::SysStaff* ss = muse::value(system()->staves(), static_cast<staff_idx_t>(staffIdx));
    return ss ? scaleRect(ss->bbox(), system()->spatium()) : QRectF();
}

qreal System::yOffset(int staffIdx)
{
    mu::engraving::SysStaff* ss = muse::value(system()->staves(), static_cast<staff_idx_t>(staffIdx));
    return ss ? ss->yOffset() / system()->spatium() : 0.0;
}

bool System::show(int staffIdx)
{
    mu::engraving::SysStaff* ss = muse::value(system()->staves(), static_cast<staff_idx_t>(staffIdx));
    return ss ? ss->show() : false;
}

//---------------------------------------------------------
//   Page::pagenumber
//---------------------------------------------------------

int Page::pagenumber() const
{
    return static_cast<int>(page()->no());
}

//---------------------------------------------------------
//   Staff::part
//---------------------------------------------------------

Part* Staff::part()
{
    return wrap<Part>(staff()->part());
}

//---------------------------------------------------------
//   wrap
///   \cond PLUGIN_API \private \endcond
///   Wraps mu::engraving::EngravingItem choosing the correct wrapper type
///   at runtime based on the actual element type.
//---------------------------------------------------------

EngravingItem* mu::engraving::apiv1::wrap(mu::engraving::EngravingItem* e, Ownership own)
{
    if (!e) {
        return nullptr;
    }

    using mu::engraving::ElementType;
    switch (e->type()) {
    case ElementType::TIE:
        return wrap<Tie>(toTie(e), own);
    case ElementType::NOTE:
        return wrap<Note>(toNote(e), own);
    case ElementType::CHORD:
        return wrap<Chord>(toChord(e), own);
    case ElementType::TUPLET:
        return wrap<Tuplet>(toTuplet(e), own);
    case ElementType::SEGMENT:
        return wrap<Segment>(toSegment(e), own);
    case ElementType::MEASURE:
        return wrap<Measure>(toMeasure(e), own);
    case ElementType::HBOX:
    case ElementType::VBOX:
    case ElementType::TBOX:
    case ElementType::FBOX:
        return wrap<MeasureBase>(toMeasureBase(e), own);
    case ElementType::SYSTEM:
        return wrap<System>(toSystem(e), own);
    case ElementType::PAGE:
        return wrap<Page>(toPage(e), own);
    default:
        if (e->isDurationElement()) {
            if (e->isChordRest()) {
                return wrap<ChordRest>(toChordRest(e), own);
            }
            return wrap<DurationElement>(toDurationElement(e), own);
        }
        break;
    }
    return wrap<EngravingItem>(e, own);
}
