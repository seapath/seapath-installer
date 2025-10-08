/* === This file is part of SEAPATH installer ===
 *
 *   SPDX-FileCopyrightText: 2014-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2025 Savoir-faire Linux, Inc.
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef NETWORKINTERFACELIST_H
#define NETWORKINTERFACELIST_H

#include <QStringList>
#include <QString>

namespace NetworkUtils
{

class NetworkInterfaceList
{
    public:
        NetworkInterfaceList();
        /**
         * @brief Gets a list of network interfaces.
         *
         * This returns a list of physical network interfaces
         * using the nmcli.
         */
        QStringList getPhysicalNetworkInterfaces() const;
    private:
        QStringList interfaceList;
        void populateInterfaceList();
};

}  // namespace NetworkUtils

#endif  // NETWORKINTERFACELIST_H
