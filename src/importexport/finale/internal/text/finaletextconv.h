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

#include <optional>

#include "musx/musx.h"

#include "engraving/types/types.h"
#include "engraving/types/symid.h"

namespace mu::iex::finale {

static const std::vector<std::pair<engraving::FontStyle, engraving::String>> fontStyleTags {
    { engraving::FontStyle::Bold,      u"b" },
    { engraving::FontStyle::Italic,    u"i" },
    { engraving::FontStyle::Underline, u"u" },
    { engraving::FontStyle::Strike,    u"s" },
};

class FinaleTextConv
{
public:
    /// @brief Converts the font style of a finale instance into a value of MuseScore's FontStyle enum.
    static engraving::FontStyle museFontEfx(const musx::dom::MusxInstance<musx::dom::FontInfo>& font);

    /// @brief Maps a codepoint in the font's encoding to a SymId
    static engraving::SymId symIdFromFinaleChar(char32_t c, const musx::dom::MusxInstance<musx::dom::FontInfo>& font, engraving::SymId def = engraving::SymId::noSym);

    /// @brief Maps a codepoint in the font's encoding to a `<sym>` tag. It must be a glyph that MuseScore can convert to SymId.
    static std::optional<engraving::String> symIdInsertFromFinaleChar(char32_t c, const musx::dom::MusxInstance<musx::dom::FontInfo>& font);

    /// @brief Only returns a String if *all* characters in the input text can be mapped to `<sym>` tags.
    static std::optional<engraving::String> symIdInsertsFromStdString(const std::string& text, const musx::dom::MusxInstance<musx::dom::FontInfo>& font);

    /// @brief Maps a codepoint in the font's encoding to String containing Smufl-encoded character. If the input font is Smufl, it may be an optional codepoint.
    static std::optional<engraving::String> smuflStringFromFinaleChar(char32_t c, const musx::dom::MusxInstance<musx::dom::FontInfo>& font);
};

}
