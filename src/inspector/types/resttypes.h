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
#ifndef MU_INSPECTOR_RESTTYPES_H
#define MU_INSPECTOR_RESTTYPES_H

#include "qobjectdefs.h"

namespace mu::inspector {
class RestTypes
{
    Q_GADGET

public:
    enum RestVisualDurationType {
        AUTO = -1,
        REST_MAXIMA,
        REST_LONGA,
        REST_DOUBLE_WHOLE,
        REST_WHOLE,
        REST_HALF,
        REST_QUARTER,
        REST_8TH,
        REST_16TH,
        REST_32ND,
        REST_64TH,
        REST_128TH,
        REST_256TH,
        REST_512TH,
        REST_1024TH,
        REST_DURATION_TYPES,
    };

    enum class QuarterSymbol {
        TYPE_REGULAR = -1,
        TYPE_TRADITIONAL,
        TYPE_Z
    };

    Q_ENUM(RestVisualDurationType)
    Q_ENUM(QuarterSymbol)
};
}

#endif // MU_INSPECTOR_RESTTYPES_H
