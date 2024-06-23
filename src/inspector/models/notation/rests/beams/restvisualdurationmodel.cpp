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
#include "restvisualdurationmodel.h"

#include "engraving/dom/rest.h"
#include "engraving/types/typesconv.h"

using namespace mu::inspector;
using namespace mu::engraving;

RestVisualDurationModel::RestVisualDurationModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

QHash<int, QByteArray> RestVisualDurationModel::roleNames() const
{
    return {
        { VisualDurationRole, "visualDuration" },
        { HintRole, "headHint" },
        { IconCodeRole, "iconCode" }
    };
}

int RestVisualDurationModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(RestVisualDurationType::REST_DURATION_TYPES) + 2;
}

QVariant RestVisualDurationModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) {
        return QVariant();
    }

    int row = index.row();
    auto group = static_cast<RestVisualDurationType>(row);

    switch (role) {
    case VisualDurationRole:
        return row;
    case HintRole:
        return QString(); //TConv::translatedUserName(group).toQString();
    case IconCodeRole: {
        //auto type = (group == NoteHeadGroup::HEAD_BREVIS_ALT) ? NoteHeadType::HEAD_BREVIS : NoteHeadType::HEAD_QUARTER;
        return engravingFonts()->fallbackFont()->symCode(SymId::restQuarter);
    }
    default: break;
    }

    return QVariant();
}
