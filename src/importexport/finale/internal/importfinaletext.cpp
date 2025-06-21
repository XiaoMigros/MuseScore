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
#include "dom/sig.h"

#include <vector>
#include <exception>

#include "musx/musx.h"

#include "types/string.h"

#include "engraving/dom/excerpt.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/page.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
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
}

String FinaleParser::stringFromText(std::string rawString) {
    String endString;
    auto fontInfo = std::make_shared<FontInfo>(m_doc); // Finale font tracker. Currently only used for font name.
    FontStyle currFontStyle = FontStyle::Normal; // keep track of this to avoid badly nested tags
    /// @todo textstyle support: initialise value by checking if with &, then using +/- to set font style

    auto testForTag = [](std::string& main, std::string& test) -> bool {
        if (main.substr(0, test.length) == test) {
            main.erase(0, test.length);
            return true;
        }
        return false;
    };

    while (rawString.size() > 0) {
        if (rawString.front() == '^') {
            rawString.erase(0, 1);
            if (rawString.front() == '^') {
                endString.append(u"^"); // add '^' to endString
                rawString.erase(0, 1);  // remove from rawString
            } else if (rawString.substr(0, 4) =="font") {
                size_t endBracketPos = rawString.find(")");
                if (rawString.front() == "(" && endBracketPos > 0) {
                    musx::util::EnigmaString::parseFontCommand("^" + rawString.subString(0, endBracketPos), *fontInfo.get());
                    auto fontDefinition = m_doc->getOthers()->get<others::FontDefinition>(m_currentMusxPartId, Cmper(fontInfo.fontId));
                    // When using musical fonts, don't actually set the font type since symbols are loaded separately.
                    /// @todo decide when we want to not convert symbols/fonts, e.g. to allow multiple musical fonts in one score.
                    if (fontDefinition->name != "Maestro") {
                        endString.append(String(u'<font face="' + String::fromStdString(fontDefinition->name) + u'"/>'));
						/// @todo append this based on whether symbol ends up being replaced or not
                    }
                    rawString.erase(0, endBracketPos);
                } else {
                    // log error
                    rawString.erase(0, 4);
                }
            } else if (testForTag(rawString, "size")) {
                size_t endBracketPos = rawString.find(")");
                if (rawString.front() == "(" && endBracketPos > 0) {
                    endString.append(String(u'<font size="'));
                    endString.append(String::fromStdString(rawString.substr(1, endBracketPos - 1) + String(u'"/>'));
                    rawString.erase(0, endBracketPos); // remove up to endbracketpos
                } else {
                    // log error
                }
            } else if (testForTag(rawString, "nfx")) {
                size_t endBracketPos = rawString.find(")");
                if (rawString.front() == "(" && endBracketPos > 0) {
                    // retrieve value in brackets
                    int finaleFontStyle = String::fromStdString(rawString.substr(1, endBracketPos - 1).toInt();
                    std::vector<std::tuple<String, FontStyle, bool> > fontTags;

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
                        for (size_t i = 0; i < fontTags.size() - 1; ++i) {
                            auto [tagA, msEnumA, addA] = fontTags[i];
                            auto [tagB, msEnumB, addB] = fontTags[i + 1];
                            if (msEnumA == msEnumB) {
                                fontTags.erase(i, i + 1);
                                i = 0; // reset so we can find new pairs made available by the removal
                            }
                        }

                        // append the finished product
                        for (const auto& fontTag : fontTags) {
                            auto [tag, msEnum, add] = fontTag;
                            endString.append(tag);
                        }
                    }
                    rawString.erase(0, endBracketPos);
                } else {
                    // log error
                }
            } else if (testForTag(rawString, "page")) {
                size_t endBracketPos = rawString.find(")");
                if (rawString.front() == "(" && endBracketPos > 0) {
                    // do something
                    rawString.erase(0, endBracketPos);
                } else {
                    // log error
                }
            } else if (testForTag(rawString, "cprsym()")) {
                endString.append(Char(u'\u00A9'));
            } else if (testForTag(rawString, "partname()")) {
                Excerpt* e = m_score->excerpt();
                if (e && !m_score->isMaster()) {
                    endString.append(e->name());
                } else {
                    endString.append(String(u"Score")); // Finale default (observed)
                }
            } else if (testForTag(rawString, "totpages()")) {
                // header/footer should be handled differently
                endString.append(String::number(m_score->pages().size()));
            } else if (testForTag(rawString, "filename()")) {
                 endString.append(m_score->masterScore()->name()); // correct behaviour apparently, + .mscz???
            } else {
                // Find and replace metaTags with their actual text value
                for (const auto& metaTag : kEnigmaTags) {
                    if (rawString.substr(0, metaTag.length) == metaTag) {
                        rawString.insert(f, m_score->metaTag(FinaleTConv::metaTagFromTextComponent(metaTag)).toStdString());
                        rawString.erase(0, metaTag.length);
                    }
                }
            }
            continue;
        }
        endString.append(FinaleTextConv::symIdFromFinaleChar(rawString.front()));
        rawString.erase(0, 1);
    }
    return endString;
};


void FinaleParser::importTexts()
{
    FinaleTextConv::init();
    std::vector<std::shared_ptr<others::PageTextAssign>> pageTextAssignList = m_doc->getOthers()->getArray<others::PageTextAssign>(m_currentMusxPartId);
    std::vector<std::shared_ptr<others::TextBlock>> textBlocks = m_doc->getOthers()->getArray<others::TextBlock>(m_currentMusxPartId);

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
        std::string fileInfoValue = fileInfoTextBlock ? fileInfoTextBlock->getText(/*trimTags*/ true, musx::util::EnigmaString::AccidentalStyle::Unicode) : std::string();
        if (!metaTag.empty() && !fileInfoValue.empty()) {
            m_score->setMetaTag(metaTag, String::fromStdString(fileInfoValue));
        }
    }

    const std::vector<std::string> kEnigmaFontCommands = { "font", "fontid", "Font", "fontMus", "fontTxt", "fontNum", "size", "nfx", "baseline", "tracking" };
    // fontMus, fontTxt, fontNum are tempomarks category fonts. dynamics, tempomarks, etc. (5) all have different categories. 5x3
    const std::vector<std::string> kEnigmaTags = { "arranger()", "title()", "composer()", "copyright()", "subtitle(), lyricist(), description()" };
    const std::vector<std::string> kEnigmaOthers = { "partname()", "perftime(x)", "page(numberOffset)", "totpages()", "cprsym()", "filename()", "date(x)", "fdate(x)", "time(x)" };

    for (std::shared_ptr<others::PageTextAssign> pageTextAssign : pageTextAssignList) {
        std::shared_ptr<others::TextBlock> pageTextBlock = m_doc->getOthers()->get<others::TextBlock>(m_currentMusxPartId, pageTextAssign->block);
        std::string rawText = pageTextBlock->getRawTextBlock()->text;

        // convert this to xmlText
        String pageXmlText = stringFromText(rawText);

        const bool importAsHeaderFooter = [&]() {
            // if text is not at top or bottom, invisible,
            // not recurring, or not on page 1
            if (pageTextAssign->vPos == others::PageTextAssign::VerticalAlignment::Center
                || pageTextAssign->hidden || pageTextAssign->getCmper() > 1) {
                return false;
            }

            // valid cases:
            // text styles always need to match: page numbers, headers, footers and copyright all have separate settings.
            // text block alignment can alternate, we would need to match this with the odd/even footer feature.
            // start is 2, end is 0    // yes, be aware of collisions. If not page numbers or part names, disable headers or footers on page 1 and import page 1 text as frames.
            // start is 1, end is 0    // yes, if headers/footers are still enabled.
            // start and end page is 1 // yes, if headers/footers are still enabled AND we can find a suitable $ substitute to use in place.

            // page number
            if (rawText.find("^page(") != std::string::npos) {
                // use Page::no() for number, try to set score's pagenumberoffset if possible
            }
            // check for text styles, recurring until the end

            // page number 2+ (remember to check for alternating)
            //check for existing header/footer text in the same location of the same page(s). if so, don't import.
            return true; // dbg
        }();
        const bool importAsFrameText = [&]() {
            if (pageTextAssign->vPos == others::PageTextAssign::VerticalAlignment::Center) {
                return false;
            }
            // if top or bottom, we should hopefully be able to check for distance to surrounding music and work from that
            // if not enough space, attempt to position based on closest measure
            return true; // dbg
        }();
    }
    for (std::shared_ptr<others::TextBlock> textBlock : textBlocks) {
        // others::TextBlock::TextType::Block / Expression
    }
    //note: text is placed slightly lower than indicated position (line space?)
    // todo: read text properties but also tempo, swing, etc
}

}

