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

#include "editdata.h"

#include "realfn.h"

#include "engravingitem.h"
#include "mscore.h"

using namespace mu::engraving;

void EditData::clear()
{
    *this = EditData(m_view);
}

std::shared_ptr<ElementEditData> EditData::getData(const EngravingItem* e) const
{
    for (std::shared_ptr<ElementEditData> ed : m_data) {
        if (ed->e == e) {
            return ed;
        }
    }
    return nullptr;
}

bool EditData::control(bool textEditing) const
{
    if (textEditing) {
        return modifiers & TextEditingControlModifier;
    } else {
        return modifiers & ControlModifier;
    }
}

PointF EditData::gridSnapped(PointF p, double spatium) const
{
    double x = p.x();
    double y = p.y();

    if (hRaster && !muse::RealIsEqual(x, 0.0) && true && !(modifiers & AltModifier)) { // true: decide which elements can later
        double h = spatium * MScore::hRaster();
        int n = lrint(x / h);
        x = h * n;
    }

    if (vRaster && !muse::RealIsEqual(y, 0.0) && true && !(modifiers & AltModifier)) { // vRaster already set in notationinteraction->drag
        double v = spatium * MScore::vRaster();
        int n = lrint(y / v);
        y = v * n;
    }

    return PointF(x, y);
}
