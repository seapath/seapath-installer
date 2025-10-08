/* === This file is part of SEAPATH installer ===
 *
 *   SPDX-FileCopyrightText: 2014-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2025 Savoir-faire Linux, Inc.
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "NetworkInterfaceList.h"

#include "utils/Logger.h"
#include "utils/System.h"

#include <QProcess>

namespace NetworkUtils
{

NetworkInterfaceList::NetworkInterfaceList()
{
    populateInterfaceList();
}

QStringList
NetworkInterfaceList::getPhysicalNetworkInterfaces() const
{
    return interfaceList;
}

void
NetworkInterfaceList::populateInterfaceList()
{
    auto result = Calamares::System::runCommand(
            { "nmcli", "--terse", "--field", "DEVICE,TYPE,STATE", "device" },
            std::chrono::seconds( 30 ) );
 
    if ( result.getExitCode() == 0 )
    {
        QString output = result.getOutput();
        QStringList lines = output.split("\n");

        for (const QString &line : lines)
        {
            QStringList parts = line.split(":");
            if ( parts.size() == 3 && parts.at(1) == "ethernet" )
            {
                interfaceList.append(parts.at(0));
            }
        }

    }
    else
    {
        cWarning() << "Failed to list network interfaces." << result.getOutput();
    }

}

}  // namespace NetworkUtils
