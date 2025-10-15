/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "Config.h"
#include "SshKeySelectionJob.h"

#include "JobQueue.h"


Config::Config( QObject* parent )
    : QObject( parent )
{
}

Calamares::JobList
Config::createJobs()
{
    QList< Calamares::job_ptr > list;

    Calamares::Job* j = new SshKeySelectionJob( true );
    list.append( Calamares::job_ptr( j ) );

    return list;
}
