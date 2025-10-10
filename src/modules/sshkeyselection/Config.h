/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef SSHKEYSELECTION_CONFIG_H
#define SSHKEYSELECTION_CONFIG_H

#include "utils/NamedEnum.h"

#include <QObject>

class Config : public QObject
{
    Q_OBJECT
public:
    Config( QObject* parent = nullptr );

private:

};

#endif
