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
#pragma once

#include "abstractstyledialogmodel.h"

namespace mu::notation {
class BraceDesignerSectionModel : public AbstractStyleDialogModel
{
    Q_OBJECT

    Q_PROPERTY(StyleItem * useCustomPath READ useCustomPath CONSTANT)
    Q_PROPERTY(StyleItem * customPath READ customPath CONSTANT)

public:
    explicit BraceDesignerSectionModel(QObject* parent = nullptr);

    Q_INVOKABLE void resetPath();
    Q_INVOKABLE void loadPreset(int preset);

    StyleItem* useCustomPath() const;
    StyleItem* customPath() const;
};
}
