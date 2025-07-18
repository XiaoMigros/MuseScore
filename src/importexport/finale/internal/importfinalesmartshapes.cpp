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
#include "internal/importfinaleparser.h"
#include "internal/importfinalelogger.h"
#include "internal/finaletypesconv.h"

#include <vector>
#include <exception>

#include "musx/musx.h"

#include "types/string.h"

#include "engraving/dom/anchors.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/score.h"
#include "engraving/dom/slurtie.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/textlinebase.h"
#include "engraving/dom/utils.h"

#include "log.h"

using namespace mu::engraving;
using namespace muse;
using namespace musx::dom;

namespace mu::iex::finale {

void FinaleParser::importSmartShapes()
{
    auto elementFromTerminationSeg = [&](const std::shared_ptr<others::SmartShape>& smartShape, bool start) -> EngravingItem* {
        std::shared_ptr<others::SmartShape::TerminationSeg> termSeg = start ? smartShape->startTermSeg : smartShape->endTermSeg;
        EntryInfoPtr entryInfoPtr = termSeg->endPoint->calcAssociatedEntry();
        if (entryInfoPtr) {
            NoteNumber nn = start ? smartShape->startNoteId : smartShape->endNoteId;
            if (nn) {
                return toEngravingItem(noteFromEntryInfoAndNumber(entryInfoPtr, nn));
            } else {
                EngravingItem* e = toEngravingItem(muse::value(m_entryInfoPtr2CR, entryInfoPtr, nullptr));
                if (e) {
                    return e;
                }
            }
        }
        staff_idx_t staffIdx = muse::value(m_inst2Staff, termSeg->endPoint->staffId, muse::nidx);
        Fraction mTick = muse::value(m_meas2Tick, termSeg->endPoint->measId, Fraction(-1, 1));
        Measure* measure = !mTick.negative() ? m_score->tick2measure(mTick) : nullptr;
        if (!measure || staffIdx == muse::nidx) {
            return nullptr;
        }
        Fraction tick = FinaleTConv::musxFractionToFraction(termSeg->endPoint->calcGlobalPosition());
        TimeTickAnchor* anchor = EditTimeTickAnchors::createTimeTickAnchor(measure, tick, staffIdx);
        EditTimeTickAnchors::updateLayout(measure);
        return toEngravingItem(anchor->segment());
    };

    std::vector<std::shared_ptr<others::SmartShape>> smartShapes = m_doc->getOthers()->getArray<others::SmartShape>(m_currentMusxPartId, BASE_SYSTEM_ID);
    for (const std::shared_ptr<others::SmartShape>& smartShape : smartShapes) {
        if (smartShape->shapeType == others::SmartShape::ShapeType::WordExtension
            || smartShape->shapeType == others::SmartShape::ShapeType::Hyphen) {
            // will be handled elsewhere
            continue;
        }
        ElementType type = FinaleTConv::elementTypeFromShapeType(smartShape->shapeType);
        if (type == ElementType::INVALID) {
            // custom smartshapes, not yet supported
            continue;
        }

        Spanner* newSpanner = toSpanner(Factory::createItem(type, m_score->dummy()));

        // Set anchors and start/end elements
        EngravingItem* startElement = elementFromTerminationSeg(smartShape, true);
        EngravingItem* endElement = elementFromTerminationSeg(smartShape, false);
        IF_ASSERT_FAILED(startElement && endElement) {
            delete newSpanner;
            continue;
        }
        if (smartShape->entryBased) {
            if (smartShape->startNoteId && smartShape->endNoteId) {
                newSpanner->setAnchor(Spanner::Anchor::NOTE); //todo create notelines instead of textlines?
            } else {
                newSpanner->setAnchor(Spanner::Anchor::CHORD);
            }
            newSpanner->setTrack(startElement->track());
            newSpanner->setTrack2(endElement->track());
            newSpanner->setStartElement(startElement);
            newSpanner->setEndElement(endElement);
        } else {
            newSpanner->setAnchor(Spanner::Anchor::SEGMENT);
            staff_idx_t staffIdx1 = muse::value(m_inst2Staff, smartShape->startTermSeg->endPoint->staffId, muse::nidx);
            staff_idx_t staffIdx2 = muse::value(m_inst2Staff, smartShape->endTermSeg->endPoint->staffId, muse::nidx);
            if (staffIdx1 == muse::nidx || staffIdx2 == muse::nidx) {
                delete newSpanner;
                continue;
            }
            newSpanner->setTrack(staff2track(staffIdx1));
            newSpanner->setTrack2(staff2track(staffIdx2));
            // don't set end elements, instead a computed start/end segment is called
        }
        newSpanner->setTick(startElement->tick());
        newSpanner->setTick2(endElement->tick());

        // Set properties
        newSpanner->setVisible(!smartShape->hidden);
        if (type == ElementType::OTTAVA) {
            toOttava(newSpanner)->setOttavaType(FinaleTConv::ottavaTypeFromShapeType(smartShape->shapeType));
        } else if (type == ElementType::SLUR) {
            toSlur(newSpanner)->setStyleType(FinaleTConv::slurStyleTypeFromShapeType(smartShape->shapeType));
            /// @todo is there a way to read the calculated direction
            toSlur(newSpanner)->setSlurDirection(FinaleTConv::directionVFromShapeType(smartShape->shapeType));
        } else if (type == ElementType::TEXTLINE) {
            TextLineBase* textLine = toTextLineBase(newSpanner);
            textLine->setLineStyle(FinaleTConv::lineTypeFromShapeType(smartShape->shapeType));
            std::pair<double, double> hookHeights = FinaleTConv::hookHeightsFromShapeType(smartShape->shapeType);
            if (!muse::RealIsEqual(hookHeights.first, 0)) {
                textLine->setBeginHookType(HookType::HOOK_90);
                textLine->setBeginHookHeight(Spatium(hookHeights.first));
                // continue should have no hook
            }
            if (!muse::RealIsEqual(hookHeights.second, 0)) {
                textLine->setEndHookType(HookType::HOOK_90);
                textLine->setEndHookHeight(Spatium(hookHeights.second));
            }
        }
        /// @todo set guitar bend type

        m_score->addElement(newSpanner);
    }
}

}
