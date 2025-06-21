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
#pragma once

#include <array>
#include <map>

#include "engraving/types/string.h"
#include "engraving/types/symid.h"

#include "musx/musx.h"

#include "importfinalelogger.h"

namespace mu::iex::finale {
using CharacterMap = std::pair<char16_t, char16_t>;

class FinaleLogger;
class FinaleTextConv
{
public:
    FinaleTextConv() = default;

    static bool init();

    static char16_t smuflCode(char c, std::string font);
    static String symIdFromFinaleChar(char c, std::string font);

private:

    static bool initConversionJson();

    std::unordered_map<std::string, std::vector<CharacterMap> > m_convertedFonts;
};

}
