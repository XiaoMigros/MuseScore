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
#include "finaletypesconv.h"
#include "internal/text/finaletextconv.h"

#include <vector>
#include <exception>

#include "musx/musx.h"

#include "types/string.h"

#include "engraving/dom/excerpt.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/page.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/sig.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/system.h"
#include "engraving/dom/utils.h"

#include "engraving/types/symnames.h"

#include "log.h"

using namespace mu::engraving;
using namespace muse;
using namespace musx::dom;

namespace mu::iex::finale {

static const std::vector<std::tuple<int, String, FontStyle> > fontStyleMapper = {
    // raw finale value, musescore text tag, musescore fontstyle
    { 1,  u"b", FontStyle::Bold },
    { 2,  u"i", FontStyle::Italic },
    { 4,  u"u", FontStyle::Underline },
    { 32, u"s", FontStyle::Strike },
};

static const std::vector<std::string> kEnigmaTags = {
    "arranger", "title", "composer", // "copyright",
    "subtitle, lyricist, description",
};

String FinaleParser::stringFromText(const std::shared_ptr<others::PageTextAssign>& pageTextAssign, bool isHeaderOrFooter)
{
    std::shared_ptr<others::TextBlock> pageTextBlock = m_doc->getOthers()->get<others::TextBlock>(m_currentMusxPartId, pageTextAssign->block);
    std::string rawString = pageTextBlock->getRawTextCtx(m_currentMusxPartId).getRawText()->text;
    String endString;
    std::shared_ptr<FontInfo> prevFont;
    FontStyle currFontStyle = FontStyle::Normal; // keep track of this to avoid badly nested tags
    /// @todo textstyle support: initialise value by checking if with &, then using +/- to set font style

    auto isOnlyPage = [this](const std::shared_ptr<others::PageTextAssign>& pageTextAssign, uint16_t page) {
        return (pageTextAssign->getCmper() == page || (pageTextAssign->startPage == page && pageTextAssign->endPage == page)
        || (pageTextAssign->startPage == uint16_t(m_score->npages()) && pageTextAssign->endPage == 0));
    };

    // The processTextChunk function process each chunk of processed text with font information. It is only
    // called when the font information changes.
    auto processTextChunk = [&](const std::string& nextChunk, const musx::util::EnigmaStyles& styles) -> bool {
        const std::shared_ptr<FontInfo>& font = styles.font;
        if (!prevFont || prevFont->fontId != font->fontId) {
            // When using musical fonts, don't actually set the font type since symbols are loaded separately.
            /// @todo decide when we want to not convert symbols/fonts, e.g. to allow multiple musical fonts in one score.
            if (!font->calcIsDefaultMusic()) { /// @todo RGP changed from a name check, but each notation element has its own default font setting in Finale. We need to handle that.
                endString.append(String(u"<font face=\"" + String::fromStdString(font->getName()) + u"\"/>"));
                /// @todo append this based on whether symbol ends up being replaced or not.
            }
        }
        if (!prevFont || prevFont->fontSize != font->fontSize) {
            endString.append(String(u"<font size=\""));
            endString.append(String::number(font->fontSize) + String(u"\"/>'"));
        }
        if (!prevFont || prevFont->getEnigmaStyles() != font->getEnigmaStyles()) {
            int finaleFontStyle = font->getEnigmaStyles();
            std::vector<std::tuple<String, FontStyle, bool> > fontTags;
            /// @todo: RGP: it might be a lot simpler to use the bool properties in FontInfo. Not sure.

            // How this works:
            // If Finale says the text is a certain style, we add a tag saying text should match the style.
            // If MuseScore says the text is currently like that, we add a tag saying text should stop matching that style.
            //    If Finale says to set a style, and MuseScore is currently not that style, we add a positive tag.
            //    If MuseScore is currently a style and Finale does not re-tag it, we cancel that style with a negative tag.
            //    If neither mention the style (it isn't currently active nor has it been set), do nothing.
            //    If Finale and MuseScore contain it (meaning it's active currently AND was re-tagged, presumably due to
            //     a different style changing), add a positive and negative tag. This ensures we won't end up with bad nesting.
            // We then order the tags, and remove occuring duplicates, if the nesting allows it.
            // Yes, this can result in a lot of excessive tags, but even native MuseScore is currently no different!

            for (const auto& fontStyle : fontStyleMapper) {
                auto [finaleInt, museScoreTag, museScoreEnum] = fontStyle;

                if (finaleFontStyle & finaleInt) {
                    fontTags.emplace_back(std::make_tuple(String(u"<" + museScoreTag + u">"), museScoreEnum, true));
                }
                if (currFontStyle & museScoreEnum) {
                    fontTags.emplace_back(std::make_tuple(String(u"</" + museScoreTag + u">"), museScoreEnum, false));
                }
            }
            if (!fontTags.empty()) {
                std::sort(fontTags.begin(), fontTags.end(),[] (const auto& a, const auto& b) {
                    auto [tagA, msEnumA, addA] = a;
                    auto [tagB, msEnumB, addB] = b;
                    if (addA == addB) {
                        return msEnumA < msEnumB == addA;
                    }
                    return addA;
                });

                // remove duplicate adjacent tags
                for (size_t i = 1; i < fontTags.size();) {
                    auto [tagA, msEnumA, addA] = fontTags[i -1];
                    auto [tagB, msEnumB, addB] = fontTags[i];
                    if (msEnumA == msEnumB) {
                        fontTags.erase(fontTags.begin() + i - 1, fontTags.begin() + i + 1);
                        if (i == 0) {
                            break;
                        }
                        --i;
                    } else {
                        ++i;
                    }
                }

                // append the finished product
                for (const auto& fontTag : fontTags) {
                    auto [tag, msEnum, add] = fontTag;
                    endString.append(tag);
                }
            }
        }
        prevFont = std::make_shared<FontInfo>(*font);
        endString.append(String::fromStdString(nextChunk));
        return true;
    };

    // The processCommand function sends back to the parser a subsitution string for the Enigma command.
    // The command is parsed with the command in the first element and any parameters in subsequent elements.
    // Return "" to remove the command from the processed string. Return std::nullopt to copy the command text into the processed string.
    auto processCommand = [&](const std::vector<std::string>& parsedCommand) -> std::optional<std::string> {
        if (parsedCommand.empty()) {
            // log error
            return std::nullopt;
        }
        /// @todo Perhaps add parse functions to classes like PageTextAssign to handle this automatically. But it also may be important
        /// to handle it here for an intelligent import, if text can reference a page number offset in MuseScore.
        if (parsedCommand[0] == "page") {
            if (parsedCommand.size() < 2) {
                // log error
            }
            // do something
            return std::nullopt; // for now, just dump the command into the processed string
        } else if (parsedCommand[0] == "cprsym") {
            return String(u"\u00A9").toStdString();
        } else if (parsedCommand[0] == "partname") {
            /// @todo it may make more sense to get the partname from musx. Not sure. For now, not changing it.
            Excerpt* e = m_score->excerpt();
            std::string partName = (e && !m_score->isMaster()) ? e->name().toStdString() : "Score"; /// @todo this "Score" string is stored in the musx options
            if (isHeaderOrFooter) {
                return (pageTextAssign->startPage == 2 && pageTextAssign->endPage == 0) ? "$i" : "$I";
            }
            return partName;
        } else if (parsedCommand[0] ==  "totpages") {
            if (isHeaderOrFooter) {
                return "$n";
            }
            return std::to_string(m_score->npages());
        } else if (parsedCommand[0] == "filename") {
            if (isHeaderOrFooter) {
                return "$f";
            }
            /// @todo Does the file have a name at import time?
            return m_score->masterScore()->name().toStdString();
        } else if (parsedCommand[0] ==  "date") {
            // This command takes an argument. Why? What does it do?
            // RGP: It is a format code. I will have to dig to see what all the values are,
            //      but I would guess 0 means use the document default and the other values
            //      are like enum values specifying a specific format. (Same for other datetime args.)
            return muse::Date::currentDate().toString(muse::DateFormat::ISODate).toStdString();
            /// @todo check for correct date format
        } else if (parsedCommand[0] ==  "time") {
            // This command takes an argument. Why? What does it do?
            return muse::Time::currentTime().toString(muse::DateFormat::ISODate).toStdString();
            /// @todo check for correct date format
        } else if (parsedCommand[0] == "fdate") {
            // This command takes an argument. Why? What does it do?
            musx::dom::header::FileInfo fDate = m_doc->getHeader()->created;
            return std::to_string(fDate.day) + "/" + std::to_string(fDate.month) + "/" + std::to_string(fDate.year);
            /// @todo check for correct date format
        } else if (parsedCommand[0] ==  "perftime") {
            // This command takes an argument. Why? What does it do?
            int rawDurationSeconds = m_score->duration();
            int minutes = rawDurationSeconds / 60;
            int seconds = rawDurationSeconds % 60;
            return std::to_string(minutes) + "'" + std::to_string(seconds) + '"';
        } else if (parsedCommand[0] ==  "copyright") {
            if (isHeaderOrFooter) {
                return isOnlyPage(pageTextAssign, 1) ? "$C" : "$c";
            }
            // fall through
        }
        // Find and replace metaTags with their actual text value
        for (const auto& metaTag : kEnigmaTags) {
            if (parsedCommand[0] == metaTag) {
                std::string tagValue = m_score->metaTag(FinaleTConv::metaTagFromTextComponent(metaTag)).toStdString();
                if (isHeaderOrFooter) {
                    return "$:" + tagValue + ":";
                }
                return tagValue;
            }
        }
        // Returning std::nullopt allows the musx library to fill in any we have not handled.
        return std::nullopt;
    };

    musx::util::EnigmaString::parseEnigmaText(m_doc, m_currentMusxPartId, rawString, processTextChunk, processCommand);

    return endString;
};


void FinaleParser::importTexts()
{
    FinaleTextConv::init();
    std::vector<std::shared_ptr<others::PageTextAssign>> pageTextAssignList = m_doc->getOthers()->getArray<others::PageTextAssign>(m_currentMusxPartId);

    // we need to work with real-time positions and pages, so we layout the score.
    m_score->setLayoutAll();
    m_score->doLayout();


    // code idea:
    // first, read score metadata
    // then, handle page text as header/footer vframes where applicable and if not, assign it to a measure
    // each handled textblock is parsed as possible with fontdata and appended to a list of used fontdata
    // whenever an element is read, check with Cmper in a map to see if textblock has already been parsed, if so, used it from there, if not, parse and add
    // measure-anchored-texts: determine text style based on (??? contents? font settings?), add to score, same procedure with text block
    // if centered or rightmost, create as marking to get correct anchoring???
    // expressions::
    // tempo changes
    // need to create character conversion map for non-smufl fonts???

    // set score metadata
    std::vector<std::shared_ptr<texts::FileInfoText>> fileInfoTexts = m_doc->getTexts()->getArray<texts::FileInfoText>(m_currentMusxPartId);
    for (std::shared_ptr<texts::FileInfoText> fileInfoText : fileInfoTexts) {
        String metaTag = FinaleTConv::metaTagFromFileInfo(fileInfoText->getTextType());
        std::shared_ptr<others::TextBlock> fileInfoTextBlock = m_doc->getOthers()->get<others::TextBlock>(m_currentMusxPartId, Cmper(fileInfoText->getTextType()));
        std::string fileInfoValue = fileInfoTextBlock ? fileInfoTextBlock->getRawTextCtx(m_currentMusxPartId).getText(/*trimTags*/ true, musx::util::EnigmaString::AccidentalStyle::Unicode) : std::string();
        if (!metaTag.empty() && !fileInfoValue.empty()) {
            m_score->setMetaTag(metaTag, String::fromStdString(fileInfoValue));
        }
    }

    const std::vector<std::string> kEnigmaFontCommands = { "font", "fontid", "Font", "fontMus", "fontTxt", "fontNum", "size", "nfx", "baseline", "tracking" };
    // fontMus, fontTxt, fontNum are tempomarks category fonts. dynamics, tempomarks, etc. (5) all have different categories. 5x3
    const std::vector<std::string> kEnigmaOthers = { "partname()", "perftime(???)", "page(numberOffset)", "totpages()", "cprsym()", "filename()", "date(???)", "fdate(???)", "time(x)" };

    // This means we can import page 1 text as header /footer, irrespective of whether it would appear on a hypothetical page 3.
    const bool twoPage = m_score->npages() == 2;
    const bool threePage = m_score->npages() == 3;
    bool singleHeader = true; //for now
    bool singleFooter = true; //for now

    struct HeaderFooter {
        bool show = false;
        bool showFirstPage = true;
        bool oddEven = false; // different odd/even pages
        std::vector<std::shared_ptr<others::PageTextAssign>> oddLeftTexts;
        std::vector<std::shared_ptr<others::PageTextAssign>> oddMiddleTexts;
        std::vector<std::shared_ptr<others::PageTextAssign>> oddRightTexts;
        std::vector<std::shared_ptr<others::PageTextAssign>> evenLeftTexts;
        std::vector<std::shared_ptr<others::PageTextAssign>> evenMiddleTexts;
        std::vector<std::shared_ptr<others::PageTextAssign>> evenRightTexts;
    };

    HeaderFooter header;
    HeaderFooter footer;
    std::vector<std::shared_ptr<others::PageTextAssign>> notHF;
    std::vector<std::shared_ptr<others::PageTextAssign>> remainder;

    auto isOnlyPage = [this](const std::shared_ptr<others::PageTextAssign>& pageTextAssign, uint16_t page) {
        return (pageTextAssign->getCmper() == page || (pageTextAssign->startPage == page && pageTextAssign->endPage == page)
        || (pageTextAssign->startPage == uint16_t(m_score->npages()) && pageTextAssign->endPage == 0));
    };
    std::optional<int> scorePageOffset = std::nullopt;
    // gather texts by position
    for (std::shared_ptr<others::PageTextAssign> pageTextAssign : pageTextAssignList) {
        std::shared_ptr<others::TextBlock> pageTextBlock = m_doc->getOthers()->get<others::TextBlock>(m_currentMusxPartId, pageTextAssign->block);
        std::string rawText = pageTextBlock->getRawTextCtx(m_currentMusxPartId).getRawText()->text;

        // if text is not at top or bottom, invisible,
        // not recurring, or not on page 1, don't import as hf
        // For 2-page scores, we can import text only assigned to page 2 as a regular even hf.
        // For 3-page scores, we can import text only assigned to page 2 as a regular odd hf if we disable hf on page one.
        /// @todo add sensible limits for xDisp and such.
        if (pageTextAssign->vPos == others::PageTextAssign::VerticalAlignment::Center
            || pageTextAssign->hidden || pageTextAssign->getCmper() > (m_score->npages() <= 3 ? m_score->npages() : 1)
            || pageTextAssign->startPage > (m_score->npages() <= 3 ? m_score->npages() : (pageTextAssign->endPage == 0 ? 2 : 1))
            || pageTextAssign->endPage < (m_score->npages() <= 3 ? -1 : m_score->npages())) {
            notHF.emplace_back(pageTextAssign);
            continue;
        }

        HeaderFooter& hf = pageTextAssign->vPos == others::PageTextAssign::VerticalAlignment::Top ? header : footer;

        // Only the part name or page number can be displayed on pages 2+
        // without disabling the HF on page 1.
        /// @todo parse page number / part name except on first page ($p, $i). If not that, set to false
        if (pageTextAssign->startPage == 2 && pageTextAssign->endPage == 0) {
            std::shared_ptr<others::TextBlock> pageTextBlock = m_doc->getOthers()->get<others::TextBlock>(m_currentMusxPartId, pageTextAssign->block);
            std::string rawText = pageTextBlock->getRawTextCtx(m_currentMusxPartId).getRawText()->text;
            String endString;
            auto processTextChunk = [&](const std::string& nextChunk, const musx::util::EnigmaStyles&) -> bool {
                endString.append(String::fromStdString(nextChunk));
                return true;
            };

            auto processCommand = [&](const std::vector<std::string>& parsedCommand) -> std::optional<std::string> {
                if (parsedCommand.empty()) {
                    // log error
                    return std::nullopt;
                }
                if (parsedCommand[0] == "page") {
                    if (parsedCommand.size() < 2) {
                        return "false";
                    }
                    if (scorePageOffset.has_value()) {
                        if (scorePageOffset.value() == std::stoi(parsedCommand[1])) {
                            return "true";
                        }
                    } else {
                        scorePageOffset = std::stoi(parsedCommand[1]);
                        return "true";
                    }
                } else if (parsedCommand[0] == "partname") {
                    return "true";
                }
                return "false";
            };

            musx::util::EnigmaString::parseEnigmaText(m_doc, m_currentMusxPartId, rawText, processTextChunk, processCommand,
                    musx::util::EnigmaString::AccidentalStyle::Unicode);
            if (endString != String(u"true")) {
                hf.showFirstPage = false;
            }
        }
        remainder.emplace_back(pageTextAssign);
    }

    if (scorePageOffset.has_value()) {
        m_score->setPageNumberOffset(scorePageOffset.value());
    }

    for (std::shared_ptr<others::PageTextAssign> pageTextAssign : remainder) {
        HeaderFooter& hf = pageTextAssign->vPos == others::PageTextAssign::VerticalAlignment::Top ? header : footer;
        // requires showFirstPage
        if (isOnlyPage(pageTextAssign, 1)) {
            if (!hf.showFirstPage) {
                muse::remove(remainder, pageTextAssign);
                notHF.emplace_back(pageTextAssign);
                continue;
            }
            if (!twoPage) {
                String pageXmlText = stringFromText(pageTextAssign, false);
                if (pageXmlText != m_score->metaTag(u"copyright")) {
                    muse::remove(remainder, pageTextAssign);
                    notHF.emplace_back(pageTextAssign);
                    continue;
                }
            }
        }

        if (isOnlyPage(pageTextAssign, 1) || isOnlyPage(pageTextAssign, 3)
            || pageTextAssign->endPage == 0) {
            hf.show = true;
        }

        if (isOnlyPage(pageTextAssign, 2) || pageTextAssign->endPage == 0) {
            if (pageTextAssign->indRpPos) {
                hf.oddEven = true;
            }
        }
    }
    for (std::shared_ptr<others::PageTextAssign> pageTextAssign : remainder) {
        HeaderFooter& hf = pageTextAssign->vPos == others::PageTextAssign::VerticalAlignment::Top ? header : footer;
        // requires oddEven and showFirstPage
        if (isOnlyPage(pageTextAssign, 3)) {
            if (!hf.oddEven || m_score->npages() > 4 || hf.showFirstPage) {
                // muse::remove(remainder, pageTextAssign);
                notHF.emplace_back(pageTextAssign);
                continue;
            }
        }
        if (isOnlyPage(pageTextAssign, 1) || isOnlyPage(pageTextAssign, 3)
            || pageTextAssign->endPage == 0) {
            switch (pageTextAssign->hPosLp) {
            case others::PageTextAssign::HorizontalAlignment::Left:
                hf.oddLeftTexts.emplace_back(pageTextAssign);
                break;
            case others::PageTextAssign::HorizontalAlignment::Center:
                hf.oddMiddleTexts.emplace_back(pageTextAssign);
                break;
            case others::PageTextAssign::HorizontalAlignment::Right:
                hf.oddRightTexts.emplace_back(pageTextAssign);
                break;
            }
        }
        if (isOnlyPage(pageTextAssign, 2) || pageTextAssign->endPage == 0) {
            switch (pageTextAssign->indRpPos ? pageTextAssign->hPosLp : pageTextAssign->hPosRp) {
            case others::PageTextAssign::HorizontalAlignment::Left:
                hf.evenLeftTexts.emplace_back(pageTextAssign);
                break;
            case others::PageTextAssign::HorizontalAlignment::Center:
                hf.evenMiddleTexts.emplace_back(pageTextAssign);
                break;
            case others::PageTextAssign::HorizontalAlignment::Right:
                hf.evenRightTexts.emplace_back(pageTextAssign);
                break;
            }
        }
    }

    if (header.show) {
        m_score->style().set(Sid::showHeader,      true);
        m_score->style().set(Sid::headerFirstPage, header.showFirstPage);
        m_score->style().set(Sid::headerOddEven,   header.oddEven);
        m_score->style().set(Sid::evenHeaderL,     header.evenLeftTexts.empty() ? String() : stringFromText(header.evenLeftTexts.front(), true)); // for now
        m_score->style().set(Sid::evenHeaderC,     header.evenMiddleTexts.empty() ? String() : stringFromText(header.evenMiddleTexts.front(), true));
        m_score->style().set(Sid::evenHeaderR,     header.evenRightTexts.empty() ? String() : stringFromText(header.evenRightTexts.front(), true));
        m_score->style().set(Sid::oddHeaderL,      header.oddLeftTexts.empty() ? String() : stringFromText(header.oddLeftTexts.front(), true));
        m_score->style().set(Sid::oddHeaderC,      header.oddMiddleTexts.empty() ? String() : stringFromText(header.oddMiddleTexts.front(), true));
        m_score->style().set(Sid::oddHeaderR,      header.oddRightTexts.empty() ? String() : stringFromText(header.oddRightTexts.front(), true));
    }

    if (footer.show) {
        m_score->style().set(Sid::showFooter,      true);
        m_score->style().set(Sid::footerFirstPage, footer.showFirstPage);
        m_score->style().set(Sid::footerOddEven,   footer.oddEven);
        m_score->style().set(Sid::evenFooterL,     footer.evenLeftTexts.empty() ? String() : stringFromText(footer.evenLeftTexts.front(), true)); // for now
        m_score->style().set(Sid::evenFooterC,     footer.evenMiddleTexts.empty() ? String() : stringFromText(footer.evenMiddleTexts.front(), true));
        m_score->style().set(Sid::evenFooterR,     footer.evenRightTexts.empty() ? String() : stringFromText(footer.evenRightTexts.front(), true));
        m_score->style().set(Sid::oddFooterL,      footer.oddLeftTexts.empty() ? String() : stringFromText(footer.oddLeftTexts.front(), true));
        m_score->style().set(Sid::oddFooterC,      footer.oddMiddleTexts.empty() ? String() : stringFromText(footer.oddMiddleTexts.front(), true));
        m_score->style().set(Sid::oddFooterR,      footer.oddRightTexts.empty() ? String() : stringFromText(footer.oddRightTexts.front(), true));
    }

    std::vector<Cmper> pagesWithHeaderFrames;
    std::vector<Cmper> pagesWithFooterFrames;

    auto getPages = [this](const std::shared_ptr<others::PageTextAssign>& pageTextAssign) -> std::vector<page_idx_t> {
        std::vector<page_idx_t> pagesWithText;
        if (pageTextAssign->getCmper() == 0) {
            page_idx_t startP = page_idx_t(pageTextAssign->startPage);
            page_idx_t endP = pageTextAssign->endPage == 0 ? page_idx_t(m_score->npages()) : page_idx_t(pageTextAssign->endPage);
            for (page_idx_t i = startP; i <= endP; ++i) {
                pagesWithText.emplace_back(i);
            }
        } else {
            pagesWithText.emplace_back(page_idx_t(pageTextAssign->getCmper()));
        }
        return pagesWithText;
    };

    auto pagePosOfPageTextAssign = [](Page* page, const std::shared_ptr<others::PageTextAssign>& pageTextAssign) -> PointF {
        RectF pageBox = page->ldata()->bbox(); // height and width definitely work, this hopefully too
        PointF p;

        switch (pageTextAssign->vPos) {
        case others::PageTextAssign::VerticalAlignment::Center:
            p.ry() = pageBox.y() / 2;
            break;
        case others::PageTextAssign::VerticalAlignment::Top:
            p.ry() = pageBox.y();
            break;
        case others::PageTextAssign::VerticalAlignment::Bottom:;
            break;
        }

        if (pageTextAssign->indRpPos && !(page->no() & 1)) {
            switch(pageTextAssign->hPosRp) {
            case others::PageTextAssign::HorizontalAlignment::Center:
                p.rx() = pageBox.x() / 2;
                break;
            case others::PageTextAssign::HorizontalAlignment::Right:;
                p.rx() = pageBox.x();
                break;
            case others::PageTextAssign::HorizontalAlignment::Left:
                break;
            }
            p.rx() += FinaleTConv::doubleFromEvpu(pageTextAssign->rightPgXDisp);
            p.ry() += FinaleTConv::doubleFromEvpu(pageTextAssign->rightPgYDisp);
        } else {
            switch(pageTextAssign->hPosLp) {
            case others::PageTextAssign::HorizontalAlignment::Center:
                p.rx() = pageBox.x() / 2;
                break;
            case others::PageTextAssign::HorizontalAlignment::Right:;
                p.rx() = pageBox.x();
                break;
            case others::PageTextAssign::HorizontalAlignment::Left:
                break;
            }
            p.rx() += FinaleTConv::doubleFromEvpu(pageTextAssign->xDisp);
            p.ry() += FinaleTConv::doubleFromEvpu(pageTextAssign->yDisp);
        }
        return p;
    };

    auto getMeasureForPageTextAssign = [](Page* page, PointF p, bool allowNonMeasures = true) -> MeasureBase* {
        MeasureBase* closestMB = nullptr;
        double prevDist = DBL_MAX;
        for (System* s : page->systems()) {
            for (MeasureBase* m : s->measures()) {
                if (allowNonMeasures || m->isMeasure()) {
                    if (m->ldata()->bbox().distanceTo(p) < prevDist) {
                        closestMB = m;
                        prevDist = m->ldata()->bbox().distanceTo(p);
                    }
                }
            }
        }
        return closestMB;
    };

    auto addPageTextToMeasure = [](const std::shared_ptr<others::PageTextAssign>& pageTextAssign, PointF p, MeasureBase* mb, Page* page) {
        PointF relativePos = p - mb->pagePos();
        // if (item->placeBelow()) {
        // ldata->setPosY(item->staff() ? item->staff()->staffHeight(item->tick()) : 0.0);
    };

    for (std::shared_ptr<others::PageTextAssign> pageTextAssign : remainder) {
        //@todo: use sophisticated check for whether to import as frame or not. (i.e. distance to measure is too large, frame would get in the way of music)
        if (pageTextAssign->vPos == others::PageTextAssign::VerticalAlignment::Center) {
            std::vector<page_idx_t> pagesWithText = getPages(pageTextAssign);
            for (page_idx_t i : pagesWithText) {
                Page* page = m_score->pages().at(i);
                PointF pagePosOfPageText = pagePosOfPageTextAssign(page, pageTextAssign);
                MeasureBase* mb = getMeasureForPageTextAssign(page, pagePosOfPageText);
                IF_ASSERT_FAILED (mb) {
                    // RGP: Finale pages can be blank, so this will definitely happen on the Finale side. (Check others::Page::isBlank to determine if it is blank)
                    // log error
                    // this should never happen! all pages need at least one measurebase
                }
                addPageTextToMeasure(pageTextAssign, pagePosOfPageText, mb, page);
            }
        }
    }

    // Don't add frames for text vertically aligned to the center.
    // if top or bottom, we should hopefully be able to check for distance to surrounding music and work from that
    // if not enough space, attempt to position based on closest measure
    //note: text is placed slightly lower than indicated position (line space?)
    // todo: read text properties but also tempo, swing, etc
}

}
