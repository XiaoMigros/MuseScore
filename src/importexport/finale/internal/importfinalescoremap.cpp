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
#include "internal/importfinalescoremap.h"
#include "internal/importfinalelogger.h"
#include "internal/finaletypesconv.h"
#include "dom/sig.h"

#include <vector>
#include <exception>

#include "musx/musx.h"

#include "types/string.h"

#include "engraving/dom/box.h"
#include "engraving/dom/bracketItem.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/key.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/layoutbreak.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/part.h"
#include "engraving/dom/spacer.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftype.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/utils.h"

#include "log.h"

using namespace mu::engraving;
using namespace muse;
using namespace musx::dom;
using namespace mu::iex::finale;

namespace mu::iex::finale {

void EnigmaXmlImporter::import()
{
    // styles (first, so that spatium and other defaults are correct)
    importStyles(m_score->style(), m_currentMusxPartId);

    // scoremap
    importParts();
    importBrackets();
    importMeasures();
    importPageLayout();
    importStaffItems();

    // entries
    mapLayers();
    importEntries();
}

static std::optional<ClefTypeList> clefTypeListFromMusxStaff(const std::shared_ptr<const others::Staff> musxStaff)
{
    ClefType concertClef = FinaleTConv::toMuseScoreClefType(musxStaff->calcFirstClefIndex());
    ClefType transposeClef = concertClef;
    if (musxStaff->transposition && musxStaff->transposition->setToClef) {
        transposeClef = FinaleTConv::toMuseScoreClefType(musxStaff->transposedClef);
    }
    if (concertClef == ClefType::INVALID || transposeClef == ClefType::INVALID) {
        return std::nullopt;
    }
    return ClefTypeList(concertClef, transposeClef);
}

static std::string trimNewLineFromString(const std::string& src)
{
    size_t pos = src.find('\n');
    if (pos != std::string::npos) {
        return src.substr(0, pos);  // Truncate at the newline, excluding it
    }
    return src;
}

Staff* EnigmaXmlImporter::createStaff(Part* part, const std::shared_ptr<const others::Staff> musxStaff, const InstrumentTemplate* it)
{
    Staff* s = Factory::createStaff(part);

    /// @todo staff settings can change at any tick
    Fraction eventTick{0, 1};

    // initialise MuseScore's default values
    if (it) {
        s->init(it, 0, 0);
        // don't load bracket from template, we add it later (if it exists)
        s->setBracketType(0, BracketType::NO_BRACKET);
        s->setBracketSpan(0, 0);
        s->setBarLineSpan(0);
    }
    /// @todo This staffLines setting will move to wherever we parse staff styles
    /// @todo Need to intialize the staff type from presets?

    // # of staff lines (only load if custom)
    if (musxStaff->staffLines.has_value()) {
        if (musxStaff->staffLines.value() != s->lines(eventTick)) {
            s->setLines(eventTick, musxStaff->staffLines.value());
        }
    } else if (musxStaff->customStaff.has_value()) {
        int customStaffSize = static_cast<int>(musxStaff->customStaff.value().size());
        if (customStaffSize != s->lines(eventTick)) {
            s->setLines(eventTick, customStaffSize);
        }
    }

    // barline vertical offsets relative to staff
    auto calcBarlineOffsetHalfSpaces = [](Evpu offset) -> int {
        // Finale and MuseScore use opposite signs for up/down
        double halfSpaces = (double(-offset) * 2.0) / EVPU_PER_SPACE;
        return int(std::lround(halfSpaces));
    };
    s->setBarLineFrom(calcBarlineOffsetHalfSpaces(musxStaff->topBarlineOffset));
    s->setBarLineTo(calcBarlineOffsetHalfSpaces(musxStaff->botBarlineOffset));

    // hide when empty
    /// @todo inherit
    s->setHideWhenEmpty(Staff::HideMode::INSTRUMENT);

    // calculate whether to use small staff size from first system
    if (const auto& firstSystem = musxStaff->getDocument()->getOthers()->get<others::StaffSystem>(m_currentMusxPartId, 1)) {
        if (firstSystem->hasStaffScaling) {
            if (auto staffSize = musxStaff->getDocument()->getDetails()->get<details::StaffSize>(m_currentMusxPartId, 1, musxStaff->getCmper())) {
                if (staffSize->staffPercent < 100) {
                    s->staffType(eventTick)->setSmall(true);
                }
            }
        }
    }

    // clefs
    if (std::optional<ClefTypeList> defaultClefs = clefTypeListFromMusxStaff(musxStaff)) {
        s->setDefaultClefType(defaultClefs.value());
    } else {
        s->staffType(eventTick)->setGenClef(false);
    }
    m_score->appendStaff(s);
    m_staff2Inst.emplace(s->idx(), InstCmper(musxStaff->getCmper()));
    m_inst2Staff.emplace(InstCmper(musxStaff->getCmper()), s->idx());
    return s;
}

void EnigmaXmlImporter::importMeasures()
{
    // add default time signature
    Fraction currTimeSig = Fraction(4, 4);
    m_score->sigmap()->clear();
    m_score->sigmap()->add(0, currTimeSig);

    std::vector<std::shared_ptr<others::Measure>> musxMeasures = m_doc->getOthers()->getArray<others::Measure>(m_currentMusxPartId);
    for (const std::shared_ptr<others::Measure>& musxMeasure : musxMeasures) {
        Fraction tick{ 0, 1 };
        MeasureBase* lastMeasure = m_score->measures()->last();
        if (lastMeasure) {
            tick = lastMeasure->tick() + lastMeasure->ticks();
        }

        Measure* measure = Factory::createMeasure(m_score->dummy()->system());
        measure->setTick(tick);
        m_meas2Tick.emplace(musxMeasure->getCmper(), tick);
        m_tick2Meas.emplace(tick, musxMeasure->getCmper());
        std::shared_ptr<TimeSignature> musxTimeSig = musxMeasure->createTimeSignature();
        Fraction scoreTimeSig = FinaleTConv::simpleMusxTimeSigToFraction(musxTimeSig->calcSimplified(), logger());
        if (scoreTimeSig != currTimeSig) {
            m_score->sigmap()->add(tick.ticks(), scoreTimeSig);
            currTimeSig = scoreTimeSig;
        }
        measure->setTimesig(scoreTimeSig);
        measure->setTicks(scoreTimeSig);
        m_score->measures()->append(measure);

        /// @todo key signature
    }

    // import system layout
    /// @todo harmonise with coda creation plugin
    std::vector<std::shared_ptr<others::Page>> pages = m_doc->getOthers()->getArray<others::Page>(m_currentMusxPartId, BASE_SYSTEM_ID);
    std::vector<std::shared_ptr<others::StaffSystem>> staffSystems = m_doc->getOthers()->getArray<others::StaffSystem>(m_currentMusxPartId, BASE_SYSTEM_ID);
    for (const std::shared_ptr<others::StaffSystem>& staffSystem : staffSystems) {
        //retrieve leftmost and rightmost measures of system
        Fraction startTick = muse::value(m_meas2Tick, staffSystem->startMeas, Fraction(-1, 1));
        Measure* startMeasure = startTick >= Fraction(0, 1)  ? m_score->tick2measure(startTick) : nullptr;
        Fraction endTick = muse::value(m_meas2Tick, staffSystem->endMeas, Fraction(-1, 1));
        Measure* endMeasure = endTick >= Fraction(0, 1)  ? m_score->tick2measure(endTick) : nullptr;
        IF_ASSERT_FAILED(startMeasure && endMeasure) {
            logger()->logWarning(String(u"Unable to retrieve measure(s) by tick for staffsystem"));
            continue;
        }
        MeasureBase* sysStart = startMeasure;
        MeasureBase* sysEnd = endMeasure;
        
        // create left and right margins
        if (!muse::RealIsEqual(staffSystem->left, 0.0)) {
            HBox* leftBox = Factory::createHBox(m_score->dummy()->system());
            leftBox->setTick(startMeasure->tick());
            //leftBox->setNext(beforeMeasure);
            leftBox->setPrev(startMeasure->prev());
            leftBox->setBoxWidth(Spatium(staffSystem->left / EVPU_PER_SPACE));
            leftBox->setSizeIsSpatiumDependent(true); // ideally false, but could have unwanted consequences
            startMeasure->setPrev(leftBox);
            // newMeasureBase->manageExclusionFromParts(/*exclude =*/ true); // excluded by default
            sysStart = leftBox;
        }
        if (!muse::RealIsEqual(staffSystem->right, 0.0)) {
            HBox* rightBox = Factory::createHBox(m_score->dummy()->system());
            Fraction rightTick = endMeasure->nextMeasure() ? endMeasure->nextMeasure()->tick() : m_score->last()->endTick();
            rightBox->setTick(rightTick);
            rightBox->setNext(endMeasure->next());
            rightBox->setPrev(endMeasure);
            rightBox->setBoxWidth(Spatium(staffSystem->right / EVPU_PER_SPACE));
            rightBox->setSizeIsSpatiumDependent(true); // ideally false, but could have unwanted consequences
            endMeasure->setNext(rightBox);
            // newMeasureBase->manageExclusionFromParts(/*exclude =*/ true); // excluded by default
            sysEnd = rightBox;
        }
        // lock measures in place
        // we lock all systems to guarantee we end up with the correct measure distribution
        m_score->addSystemLock(new SystemLock(sysStart, sysEnd));

        bool isFirstSystemOnPage = false;
        bool isLastSystemOnPage = false;
        for (const std::shared_ptr<others::Page>& page : pages) {
            /// @todo check for blank pages. check for firstPageSystem is null.
            const std::shared_ptr<others::StaffSystem>& firstPageSystem = m_doc->getOthers()->get<others::StaffSystem>(m_currentMusxPartId, page->firstSystem);
            Fraction pageStartTick = muse::value(m_meas2Tick, firstPageSystem->startMeas, Fraction(-1, 1));
            if (pageStartTick == startTick) {
                isFirstSystemOnPage = true;
            }
            if (pageStartTick == endTick + endMeasure->ticks()) {
                isLastSystemOnPage = true;
            }
            if (isFirstSystemOnPage || isLastSystemOnPage) {
                break;
            }
        }

        // create top and bottom margins
        if (isFirstSystemOnPage) {
            Spacer* spacer = Factory::createSpacer(startMeasure);
            spacer->setSpacerType(SpacerType::UP);
            spacer->setTrack(0);
            spacer->setGap(Spatium((-staffSystem->top + staffSystem->distanceToPrev) / EVPU_PER_SPACE));
            /// @todo account for title frames / perhaps header frames
            //measure->add(spacer);
        }
        if (!isLastSystemOnPage) {
            Spacer* spacer = Factory::createSpacer(startMeasure);
            spacer->setSpacerType(SpacerType::FIXED);
            spacer->setTrack(m_score->nstaves() * VOICES); /// @todo account for invisible staves
            spacer->setGap(Spatium((staffSystem->bottom + staffSystem->distanceToPrev) / EVPU_PER_SPACE));
            //measure->add(spacer);
        }
    }
}

void EnigmaXmlImporter::importParts()
{
    std::vector<std::shared_ptr<others::InstrumentUsed>> scrollView = m_doc->getOthers()->getArray<others::InstrumentUsed>(m_currentMusxPartId, BASE_SYSTEM_ID);

    int partNumber = 0;
    for (const std::shared_ptr<others::InstrumentUsed>& item : scrollView) {
        std::shared_ptr<others::Staff> staff = item->getStaff();
        IF_ASSERT_FAILED(staff) {
            continue; // safety check
        }
        auto compositeStaff = others::StaffComposite::createCurrent(m_doc, m_currentMusxPartId, staff->getCmper(), 1, 0);
        IF_ASSERT_FAILED(compositeStaff) {
            continue; // safety check
        }

        auto multiStaffInst = staff->getMultiStaffInstGroup();
        if (multiStaffInst && m_inst2Part.find(staff->getCmper()) != m_inst2Part.end()) {
            continue;
        }

        Part* part = new Part(m_score);

        // load default part settings
        /// @todo overwrite most of these settings later
        const InstrumentTemplate* it = searchTemplate(FinaleTConv::instrTemplateIdfromUuid(compositeStaff->instUuid));
        if (it) {
            part->initFromInstrTemplate(it);
        }

        QString partId = String("P%1").arg(++partNumber);
        part->setId(partId);

        // names of part
        std::string fullBaseName = staff->getFullInstrumentName(musx::util::EnigmaString::AccidentalStyle::Unicode);
        part->setPartName(QString::fromStdString(trimNewLineFromString(fullBaseName)));

        std::string fullEffectiveName = compositeStaff->getFullInstrumentName(musx::util::EnigmaString::AccidentalStyle::Unicode);
        part->setLongName(QString::fromStdString(trimNewLineFromString(fullEffectiveName)));

        std::string abrvName = compositeStaff->getAbbreviatedInstrumentName(musx::util::EnigmaString::AccidentalStyle::Unicode);
        part->setShortName(QString::fromStdString(trimNewLineFromString(abrvName)));

        if (multiStaffInst) {
            m_part2Inst.emplace(partId, multiStaffInst->staffNums);
            for (auto inst : multiStaffInst->staffNums) {
                if (auto instStaff = others::StaffComposite::createCurrent(m_doc, m_currentMusxPartId, inst, 1, 0)) {
                    createStaff(part, instStaff, it);
                    m_inst2Part.emplace(inst, partId);
                }
            }
        } else {
            createStaff(part, compositeStaff, it);
            m_part2Inst.emplace(partId, std::vector<InstCmper>({ InstCmper(staff->getCmper()) }));
            m_inst2Part.emplace(staff->getCmper(), partId);
        }
        m_score->appendPart(part);
    }
}

void EnigmaXmlImporter::importBrackets()
{
    struct StaffGroupLayer
    {
        details::StaffGroupInfo info;
        int layer{};
    };

    auto computeStaffGroupLayers = [](std::vector<details::StaffGroupInfo> groups) -> std::vector<StaffGroupLayer> {
        const size_t n = groups.size();
        std::vector<StaffGroupLayer> result;
        result.reserve(n);

        for (auto& g : groups)
            result.push_back({ std::move(g), 0 });

        for (size_t i = 0; i < n; ++i) {
            const auto& gi = result[i].info;
            if (!gi.startSlot || !gi.endSlot) {
                continue;
            }

            for (size_t j = 0; j < n; ++j) {
                if (i == j) {
                    continue;
                }
                const auto& gj = result[j].info;
                if (!gj.startSlot || !gj.endSlot) {
                    continue;
                }

                if (*gi.startSlot >= *gj.startSlot && *gi.endSlot <= *gj.endSlot &&
                    (*gi.startSlot > *gj.startSlot || *gi.endSlot < *gj.endSlot)) {
                    result[i].layer = std::max(result[i].layer, result[j].layer + 1);
                }
            }
        }

        std::sort(result.begin(), result.end(), [](const StaffGroupLayer& a, const StaffGroupLayer& b) {
            if (a.layer != b.layer) {
                return a.layer < b.layer;
            }
            if (!a.info.startSlot || !b.info.startSlot) {
                return static_cast<bool>(b.info.startSlot);
            }
            return *a.info.startSlot < *b.info.startSlot;
        });

        return result;
    };

    auto scorePartInfo = m_doc->getOthers()->get<others::PartDefinition>(SCORE_PARTID, m_currentMusxPartId);
    if (!scorePartInfo) {
        throw std::logic_error("Unable to read PartDefinition for score");
        return;
    }
    auto scrollView = m_doc->getOthers()->getArray<others::InstrumentUsed>(m_currentMusxPartId, BASE_SYSTEM_ID);

    auto staffGroups = details::StaffGroupInfo::getGroupsAtMeasure(1, scorePartInfo, scrollView);
    auto groupsByLayer = computeStaffGroupLayers(staffGroups);
    for (const auto& groupInfo : groupsByLayer) {
        IF_ASSERT_FAILED(groupInfo.info.startSlot && groupInfo.info.endSlot) {
            logger()->logWarning(String(u"Group info encountered without start or end slot information"));
            continue;
        }
        auto musxStartStaff = others::InstrumentUsed::getStaffAtIndex(scrollView, groupInfo.info.startSlot.value());
        auto musxEndStaff = others::InstrumentUsed::getStaffAtIndex(scrollView, groupInfo.info.endSlot.value());
        IF_ASSERT_FAILED(musxStartStaff && musxEndStaff) {
            logger()->logWarning(String(u"Group info encountered missing start or end staff information"));
            continue;
        }
        staff_idx_t startStaffIdx = muse::value(m_inst2Staff, InstCmper(musxStartStaff->getCmper()), muse::nidx);
        IF_ASSERT_FAILED(startStaffIdx != muse::nidx) {
            logger()->logWarning(String(u"Create brackets: Musx inst value not found for staff cmper %1").arg(String::fromStdString(std::to_string(musxStartStaff->getCmper()))));
            continue;
        }
        BracketItem* bi = Factory::createBracketItem(m_score->dummy());
        bi->setBracketType(FinaleTConv::toMuseScoreBracketType(groupInfo.info.group->bracket->style));
        int groupSpan = int(groupInfo.info.endSlot.value() - groupInfo.info.startSlot.value() + 1);
        bi->setBracketSpan(groupSpan);
        bi->setColumn(size_t(groupInfo.layer));
        m_score->staff(startStaffIdx)->addBracket(bi);
        if (groupInfo.info.group->drawBarlines == details::StaffGroup::DrawBarlineStyle::ThroughStaves) {
            for (staff_idx_t idx = startStaffIdx; idx < startStaffIdx + groupSpan - 1; idx++) {
                Staff* s = m_score->staff(idx);
                s->setBarLineTo(0);
                s->setBarLineSpan(1);
            }
        }
    }
}

static Clef* createClef(Score* score, staff_idx_t staffIdx, ClefIndex musxClef, Measure* measure, Edu musxEduPos, bool afterBarline, bool visible)
{
    ClefType entryClefType = FinaleTConv::toMuseScoreClefType(musxClef);
    if (entryClefType == ClefType::INVALID) {
        return nullptr;
    }
    Clef* clef = Factory::createClef(score->dummy()->segment());
    clef->setTrack(staffIdx * VOICES);
    clef->setConcertClef(entryClefType);
    clef->setTransposingClef(entryClefType);
    // clef->setShowCourtesy();
    // clef->setForInstrumentChange();
    clef->setVisible(visible);
    clef->setGenerated(false);
    const bool isHeader = !afterBarline && !measure->prevMeasure() && musxEduPos == 0;
    clef->setIsHeader(isHeader);
    if (afterBarline) {
        clef->setClefToBarlinePosition(ClefToBarlinePosition::AFTER);
    } else if (musxEduPos == 0) {
        clef->setClefToBarlinePosition(ClefToBarlinePosition::BEFORE);
    }

    Fraction clefTick = measure->tick() + FinaleTConv::eduToFraction(musxEduPos);
    Segment* clefSeg = measure->getSegment(
        clef->isHeader() ? SegmentType::HeaderClef : SegmentType::Clef, clefTick);
    clefSeg->add(clef);
    return clef;
}

void EnigmaXmlImporter::importClefs(const std::shared_ptr<others::InstrumentUsed>& musxScrollViewItem,
                                    const std::shared_ptr<others::Measure>& musxMeasure, Measure* measure, staff_idx_t curStaffIdx,
                                    ClefIndex& musxCurrClef)
{
    // The Finale UI requires transposition to be a full-measure staff-style assignment, so checking only the beginning of the bar should be sufficient.
    // However, it is possible to defeat this requirement using plugins. That said, doing so produces erratic results, so I'm not sure we should support it.
    // For now, only check the start of the measure.
    auto musxStaffAtMeasureStart = others::StaffComposite::createCurrent(m_doc, m_currentMusxPartId, musxScrollViewItem->staffId, musxMeasure->getCmper(), 0);
    if (musxStaffAtMeasureStart && musxStaffAtMeasureStart->transposition && musxStaffAtMeasureStart->transposition->setToClef) {
        if (musxStaffAtMeasureStart->transposedClef != musxCurrClef) {
            if (createClef(m_score, curStaffIdx, musxStaffAtMeasureStart->transposedClef, measure, /*xEduPos*/ 0, false, true)) {
                musxCurrClef = musxStaffAtMeasureStart->transposedClef;
            }
        }
        return;
    }
    if (auto gfHold = m_doc->getDetails()->get<details::GFrameHold>(m_currentMusxPartId, musxScrollViewItem->staffId, musxMeasure->getCmper())) {
        if (gfHold->clefId.has_value()) {
            if (gfHold->clefId.value() != musxCurrClef || gfHold->showClefMode == ShowClefMode::Always) {
                const bool visible = gfHold->showClefMode != ShowClefMode::Never;
                if (createClef(m_score, curStaffIdx, gfHold->clefId.value(), measure, /*xEduPos*/ 0, gfHold->clefAfterBarline, visible)) {
                    musxCurrClef = gfHold->clefId.value();
                }
            }
        } else {
            std::vector<std::shared_ptr<others::ClefList>> midMeasureClefs = m_doc->getOthers()->getArray<others::ClefList>(m_currentMusxPartId, gfHold->clefListId);
            for (const std::shared_ptr<others::ClefList>& midMeasureClef : midMeasureClefs) {
                if (midMeasureClef->xEduPos > 0 || midMeasureClef->clefIndex != musxCurrClef || midMeasureClef->clefMode == ShowClefMode::Always) {
                    const bool visible = midMeasureClef->clefMode != ShowClefMode::Never;
                    const bool afterBarline = midMeasureClef->xEduPos == 0 && midMeasureClef->afterBarline;
                    /// @todo Test with stretched staff time. (midMeasureClef->xEduPos is in global edu values.)
                    if (Clef * clef = createClef(m_score, curStaffIdx, midMeasureClef->clefIndex, measure, midMeasureClef->xEduPos, afterBarline, visible)) {
                        // only set y offset because MuseScore automatically calculates the horizontal spacing offset
                        clef->setOffset(0.0, clef->spatium() * (-double(midMeasureClef->yEvpuPos) / EVPU_PER_SPACE));
                        /// @todo perhaps populate other fields from midMeasureClef, such as clef-specific mag, etc.?
                        musxCurrClef = midMeasureClef->clefIndex;
                    }
                }
            }
        }
    }
}

void EnigmaXmlImporter::importStaffItems()
{
    std::vector<std::shared_ptr<others::Measure>> musxMeasures = m_doc->getOthers()->getArray<others::Measure>(m_currentMusxPartId);
    std::vector<std::shared_ptr<others::InstrumentUsed>> musxScrollView = m_doc->getOthers()->getArray<others::InstrumentUsed>(m_currentMusxPartId, BASE_SYSTEM_ID);
    for (const std::shared_ptr<others::InstrumentUsed>& musxScrollViewItem : musxScrollView) {
        std::shared_ptr<TimeSignature> currMusxTimeSig;
        ClefIndex musxCurrClef = others::Staff::calcFirstClefIndex(m_doc, m_currentMusxPartId, musxScrollViewItem->staffId);
        /// @todo handle pickup measures and other measures where display and actual timesigs differ
        for (const std::shared_ptr<others::Measure>& musxMeasure : musxMeasures) {
            Fraction currTick = muse::value(m_meas2Tick, musxMeasure->getCmper(), Fraction(-1, 1));
            Measure * measure = currTick >= Fraction(0, 1)  ? m_score->tick2measure(currTick) : nullptr;
            IF_ASSERT_FAILED(measure) {
                logger()->logWarning(String(u"Unable to retrieve measure by tick"), m_doc, musxScrollViewItem->staffId, musxMeasure->getCmper());
                return;
            }
            staff_idx_t staffIdx = muse::value(m_inst2Staff, musxScrollViewItem->staffId, muse::nidx);
            Staff* staff = staffIdx != muse::nidx ? m_score->staff(staffIdx) : nullptr;
            IF_ASSERT_FAILED(staff) {
                logger()->logWarning(String(u"Unable to retrieve staff by idx"), m_doc, musxScrollViewItem->staffId, musxMeasure->getCmper());
                return;
            }
            auto currStaff = others::StaffComposite::createCurrent(m_doc, m_currentMusxPartId, musxScrollViewItem->staffId, musxMeasure->getCmper(), 0);
            IF_ASSERT_FAILED(currStaff) {
                logger()->logWarning(String(u"Unable to retrieve composite staff information"), m_doc, musxScrollViewItem->staffId, musxMeasure->getCmper());
                return;
            }

            // timesig
            std::shared_ptr<TimeSignature> globalTimeSig = musxMeasure->createTimeSignature();
            std::shared_ptr<TimeSignature> musxTimeSig = musxMeasure->createTimeSignature(musxScrollViewItem->staffId);
            if (!currMusxTimeSig || !currMusxTimeSig->isSame(*musxTimeSig) || musxMeasure->showTime == others::Measure::ShowTimeSigMode::Always) {
                Fraction timeSig = FinaleTConv::simpleMusxTimeSigToFraction(musxTimeSig->calcSimplified(), logger());
                Segment* seg = measure->getSegment(SegmentType::TimeSig, currTick);
                TimeSig* ts = Factory::createTimeSig(seg);
                ts->setSig(timeSig);
                ts->setTrack(staffIdx * VOICES);
                ts->setVisible(musxMeasure->showTime != others::Measure::ShowTimeSigMode::Never);
                Fraction stretch = Fraction(musxTimeSig->calcTotalDuration().calcEduDuration(), globalTimeSig->calcTotalDuration().calcEduDuration()).reduced();
                ts->setStretch(stretch);
                /// @todo other time signature options? Beaming? Composite list?
                seg->add(ts);
                staff->addTimeSig(ts);
            }
            currMusxTimeSig = musxTimeSig;

            // clefs
            importClefs(musxScrollViewItem, musxMeasure, measure, staffIdx, musxCurrClef);

            /// @todo key signatures (including independent key sigs)
        }
    }
}

void EnigmaXmlImporter:importPageLayout()
{
    /// @todo harmonise with coda creation plugin
    std::vector<std::shared_ptr<others::Page>> pages = m_doc->getOthers()->getArray<others::Page>(m_currentMusxPartId, BASE_SYSTEM_ID);
    std::vector<std::shared_ptr<others::StaffSystem>> staffSystems = m_doc->getOthers()->getArray<others::StaffSystem>(m_currentMusxPartId, BASE_SYSTEM_ID);
    for (const std::shared_ptr<others::StaffSystem>& staffSystem : staffSystems) {
        //retrieve leftmost and rightmost measures of system
        Fraction startTick = muse::value(m_meas2Tick, staffSystem->startMeas.getCmper(), Fraction(-1, 1));
        Measure* startMeasure = startTick >= Fraction(0, 1)  ? m_score->tick2measure(startTick) : nullptr;
        Fraction endTick = muse::value(m_meas2Tick, staffSystem->endMeas.getCmper(), Fraction(-1, 1));
        Measure* endMeasure = endTick >= Fraction(0, 1)  ? m_score->tick2measure(endTick) : nullptr;
        IF_ASSERT_FAILED(startMeasure && endMeasure) {
            logger()->logWarning(String(u"Unable to retrieve measure(s) by tick for staffsystem"));
            continue;
        }
        MeasureBase* sysStart = startMeasure;
        MeasureBase* sysEnd = endMeasure;
        
        // create system left and right margins
        if (!muse::realIsEqual(staffSystem->left, 0.0)) {
            HBox* leftBox = Factory::createHBox(m_score->dummy()->system());
            leftBox->setTick(startMeasure->tick());
            leftBox->setNext(beforeMeasure);
            leftBox->setPrev(startMeasure->prev());
            leftBox->setBoxWidth(staffSystem->left / EVPU_PER_SPACE);
            leftBox->setSizeIsSpatiumDependent(true); // ideally false, but could have unwanted consequences
            startMeasure->setPrev(leftBox);
            // newMeasureBase->manageExclusionFromParts(/*exclude =*/ true); // excluded by default
            sysStart = leftBox;
        }
        if (!muse::realIsEqual(staffSystem->right, 0.0)) {
            HBox* rightBox = Factory::createHBox(m_score->dummy()->system());
            Fraction rightTick = endMeasure->nextMeasure() ? endMeasure->nextMeasure()->tick() : m_score->last()->endTick();
            rightBox->setTick(rightTick);
            rightBox->setNext(endMeasure->next());
            rightBox->setPrev(endMeasure);
            rightBox->setBoxWidth(staffSystem->right / EVPU_PER_SPACE);
            rightBox->setSizeIsSpatiumDependent(true); // ideally false, but could have unwanted consequences
            endMeasure->setNext(rightBox);
            // newMeasureBase->manageExclusionFromParts(/*exclude =*/ true); // excluded by default
            sysEnd = rightBox;
        }
        // lock measures in place
        // we lock all systems to guarantee we end up with the correct measure distribution
        m_score->addSystemLock(new SystemLock(sysStart, sysEnd));

        // determine position of system on page, and add page break if appropriate
        bool isFirstSystemOnPage = false;
        bool isLastSystemOnPage = false;
        for (const std::shared_ptr<others::Page>& page : pages) {
            const std::shared_ptr<others::StaffSystem>& firstPageSystem = page->firstSystem;
            Fraction pageStartTick = muse::value(m_meas2Tick, firstPageSystem->startMeas.getCmper(), Fraction(-1, 1));
            if (pageStartTick == startTick) {
                isFirstSystemOnPage = true;
            }
            if (pageStartTick == endTick + endMeasure->ticks()) {
                isLastSystemOnPage = true;
                LayoutBreak* lb = Factory::createLayoutBreak(sysEnd);
                lb->setLayoutBreakType(LayoutBreakType::PAGE);
                sysEnd->add(lb);
            }
            if (isFirstSystemOnPage || isLastSystemOnPage) {
                break;
            }
        }

        // create system top and bottom margins
        if (isFirstSystemOnPage) {
            Spacer* spacer = Factory::createSpacer(startMeasure);
            spacer->setSpacerType(SpacerType::UP);
            spacer->setTrack(0);
            spacer->setGap((-staffSystem->top + staffSystem->distanceToPrev) / EVPU_PER_SPACE);
            /// @todo account for title frames / perhaps header frames
            startMeasure->add(spacer);
        }
        if (!isLastSystemOnPage) {
            Spacer* spacer = Factory::createSpacer(startMeasure);
            spacer->setSpacerType(SpacerType::FIXED);
            spacer->setTrack(m_score->nstaves() * VOICES); // invisible staves are correctly accounted for on layout
            spacer->setGap((staffSystem->bottom + staffSystem->distanceToPrev) / EVPU_PER_SPACE);
            startMeasure->add(spacer);
        }
    }
}

}
