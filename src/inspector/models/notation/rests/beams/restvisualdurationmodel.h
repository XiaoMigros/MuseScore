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
#ifndef MU_INSPECTOR_RESTVISUALDURATIONMODEL_H
#define MU_INSPECTOR_RESTVISUALDURATIONMODEL_H

#include <QObject>
#include <QAbstractListModel>

#include "modularity/ioc.h"
#include "engraving/iengravingfontsprovider.h"

namespace mu::inspector {
class RestVisualDurationModel : public QAbstractListModel
{
    Q_OBJECT
    INJECT(engraving::IEngravingFontsProvider, engravingFonts)
public:
    explicit RestVisualDurationModel(QObject* parent = nullptr);

    QHash<int, QByteArray> roleNames() const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;

private:
    enum RoleNames {
        VisualDurationRole = Qt::UserRole + 1,
        HintRole,
        IconCodeRole
    };
};
}

#endif // MU_INSPECTOR_RESTVISUALDURATIONMODEL_H
