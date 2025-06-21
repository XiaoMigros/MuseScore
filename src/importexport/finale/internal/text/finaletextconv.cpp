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
#include "musx/musx.h"

#include "finaletextconv.h"

#include "smufl.h"

#include "io/dir.h"
#include "serialization/json.h"

#include "engraving/types/symnames.h"

#include "engraving/iengravingfont.h"

#include "log.h"

using namespace mu;
using namespace muse;
using namespace muse::io;
using namespace mu::engraving;

bool FinaleTextConv::init()
{
    bool ok = initConversionJson();

    return ok;
}

/// @todo allow composite symbols
// MaestroTimes: import 194 as 2, 205 as 3, 202/203/193 as 2, 216 as 217 (no suitable smufl equivalent)"

String FinaleTextConv::symIdFromFinaleChar(char c, std::string font)
{
    Char oldC = Char(c);
    Char newC = Char(smuflCode(c, font));
    if (newC == oldC) {
        return String(c);
    }
    static std::shared_ptr<IEngravingFontsProvider> provider = muse::modularity::globalIoc()->resolve<IEngravingFontsProvider>("engraving");
    SymId id = provider->fallbackFont()->fromCode(c.unicode());
    if (id == SymId::noSym) {
        return String(c);
    }
    return String(u"<sym>" + String::fromAscii(SymNames::nameForSymId(symId).ascii()) + u"</sym>");
}

char16_t FinaleTextConv::smuflCode(char c, std::string font)
{
    std::vector<CharacterMap> characterList = muse::value(m_convertedFonts, font, nullptr);
    if (!characterList) {
        return char16_t(c);
    }
    return muse::value(characterList, char16_t(c), char16_t(c));
}

bool FinaleTextConv::initConversionJson()
{
    m_convertedFonts.clear();

    Dir thirdPartyFontsDir(":/src/importexport/finale/third_party/FinaleToSMuFL/");
    RetVal<io::paths_t> thirdPartyFiles = thirdPartyFontsDir->scanFiles(thirdPartyFontsDir->path(), { "*.json" });
    if (!thirdPartyFiles.ret) {
        LOGE() << "Failed to scan files in directory: " << thirdPartyFontsDir->path();
        return;
    }

    Dir homemadeFontsDir("./fontdata/");
    RetVal<io::paths_t> homemadeFiles = homemadeFontsDir->scanFiles(homemadeFontsDir->path(), { "*.json" });
    if (!homemadeFiles.ret) {
        LOGE() << "Failed to scan files in directory: " << homemadeFontsDir->path();
        return;
    }

    io::paths_t allFiles = muse::join(thirdPartyFiles.val, homemadeFiles.val);

    for (const io::path_t& file : allFiles) {
        RetVal<ByteArray> data = fileSystem->readFile(file);
        if (!data.ret) {
            LOGE() << "Failed to read file: " << file.toStdString();
            continue;
        }
        const std::string fileName = io::basename(file).toStdString();
        std::string error;
        JsonObject glyphNamesJson = JsonDocument::fromJson(data.val, &error).rootObject();

        if (!error.empty()) {
            LOGE() << "JSON parse error in file '" << fileName << "': " << error;
            return false;
        }

        IF_ASSERT_FAILED(!glyphNamesJson.empty()) {
            LOGE() << "Could not read file '" << fileName << "'.";
            return false;
        }

        std::vector<CharacterMap> characterList;

        for (size_t i = 0; i < size_t(SymId::lastSym) + 1; ++i) {
            SymId sym = static_cast<SymId>(i);
            if (sym == SymId::noSym || sym == SymId::lastSym) {
                continue;
            }

            std::string name(SymNames::nameForSymId(sym).ascii());
            JsonObject symObj = glyphNamesJson.value(name).toObject();
            if (!symObj.isValid()) {
                continue;
            }

            // Symbol is not mapped to a SMuFL equivalent by default.
            if (symObj.value("nameIsMakeMusic") != JsonValue()) {
                continue;
            }

            bool ok;
            char16_t smuflCode = symObj.value("codepoint").toString().mid(2).toUInt(&ok, 16);
            if (!ok) {
                // smuflCode = Smufl::smuflCode(sym); not matching types (16_t / 32_t)
                LOGD() << "could not read smufl codepoint for glyph " << name << "in font " << fileName;
                continue;
            }

            char16_t finaleCode = symObj.value("legacyCodepoint").toString().toUInt(&ok, 10);
            if (ok) {
                characterList.emplace_back(std::make_pair(smuflCode, finaleCode));
            } else  {
                LOGD() << "could not read finale codepoint for glyph " << name << "in font " << fileName;
            }
        }
        if (characterList.empty()) {
            continue;
        }
        m_convertedFonts.emplace_back(std::make_pair(fileName, characterList));
    }
    return true;
}
