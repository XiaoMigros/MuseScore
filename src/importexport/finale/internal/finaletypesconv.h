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

#include "musx/musx.h"
#include "types/types.h"
#include "engraving/types/types.h"

namespace mu::iex::finale {
class FinaleTConv
{
public:
    FinaleTConv() = default;

    static engraving::ID createPartId(int partNumber);
    static engraving::ID createStaffId(musx::dom::InstCmper staffId);

    static engraving::DurationType noteTypeToDurationType(musx::dom::NoteType noteType);
    static engraving::ClefType toMuseScoreClefType(musx::dom::ClefIndex clef);
    static engraving::String instrTemplateIdfromUuid(std::string uuid);
    static engraving::BracketType toMuseScoreBracketType(musx::dom::details::StaffGroup::BracketStyle bracketStyle);
    static engraving::TupletNumberType toMuseScoreTupletNumberType(musx::dom::options::TupletOptions::NumberStyle numberStyle);
    static engraving::Align justifyToAlignment(musx::dom::others::NamePositioning::AlignJustify alignJustify);
    static engraving::CourtesyBarlineMode boolToCourtesyBarlineMode(bool useDoubleBarlines);
};

}
