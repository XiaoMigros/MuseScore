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
#include "dom/masterscore.h"
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
};

String FinaleParser::stringFromText(const std::string& rawString)
{
    String endString;
    std::shared_ptr<FontInfo> prevFont;
    FontStyle currFontStyle = FontStyle::Normal; // keep track of this to avoid badly nested tags
    /// @todo textstyle support: initialise value by checking if with &, then using +/- to set font style

    // The processTextChunk function process each chunk of processed text with font information. It is only
    // called when the font information changes.
    auto processTextChunk = [&](const std::string& nextChunk, const std::shared_ptr<FontInfo>& font) -> bool {
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
                for (size_t i = 0; i < fontTags.size() - 1; ++i) {
                    auto [tagA, msEnumA, addA] = fontTags[i];
                    auto [tagB, msEnumB, addB] = fontTags[i + 1];
                    if (msEnumA == msEnumB) {
                        fontTags.erase(fontTags.begin() + i + 1);
                        if (i > 0) --i; // back up to recheck the new pair at this position
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
            if (e && !m_score->isMaster()) {
                return e->name().toStdString();
            } else {
                return "Score"; /// @todo this string is stored in the musx options
            }
        } else if (parsedCommand[0] == "totpages") {
            /// @todo header/footer should be handled differently
            /// @todo should we get this number from musx? I'm not sure whether MuseScore yet knows the number of pages.
            /// Either way, the point is this value in Finale changes as the number of pages changes.
            return std::to_string(m_score->pages().size());
        } else if (parsedCommand[0] ==  "filename") {
            /// @todo Does the file have a name at import time?
            return m_score->masterScore()->name().toStdString();
        } else {
            /// @todo strip unknown commands or embed them. Returning "" strips them. There are many more,
            /// and some can probably be handled in the musx library by the entities that use them.
            return "";
        }
    };

    musx::util::EnigmaString::parseEnigmaText(m_doc, rawString, processTextChunk, processCommand,
            musx::util::EnigmaString::AccidentalStyle::Unicode); /// @todo we may want eventually to be smarter here and use Smufl for Smufl fonts. Not sure.

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

        [[maybe_unused]]const bool importAsHeaderFooter = [&]() {
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
        [[maybe_unused]]const bool importAsFrameText = [&]() {
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

