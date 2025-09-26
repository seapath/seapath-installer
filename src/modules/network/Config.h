/* === This file is part of Seapath installer ===
 *
 *   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2025 Savoir-faire Linux, Inc.
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include "Job.h"
#include "modulesystem/Config.h"
#include "utils/NamedEnum.h"

#include <QList>
#include <QObject>
#include <QVariantMap>

class PLUGINDLLEXPORT Config : public Calamares::ModuleSystem::Config
{
    Q_OBJECT

    Q_PROPERTY( QString networkInterface READ networkInterface WRITE setNetworkInterface NOTIFY networkInterfaceChanged )

    Q_PROPERTY( QString ipAddress READ ipAddress WRITE setIpAddress NOTIFY ipAddressChanged )
    Q_PROPERTY( QString ipAddressStatus READ ipAddressStatus NOTIFY ipAddressStatusChanged )

    Q_PROPERTY( QString mask READ mask WRITE setMask NOTIFY maskChanged )
    Q_PROPERTY( QString maskStatus READ maskStatus NOTIFY maskStatusChanged )

    Q_PROPERTY( QString gateway READ gateway WRITE setGateway NOTIFY gatewayChanged )
    Q_PROPERTY( QString gatewayStatus READ gatewayStatus NOTIFY gatewayStatusChanged )

    Q_PROPERTY( bool ready READ isReady NOTIFY readyChanged STORED false )

public:
    Config( QObject* parent = nullptr );
    ~Config() override;

    void setConfigurationMap( const QVariantMap& ) override;

    /** @brief Fill Global Storage with some settings
     *
     * This should be called when moving on from the view step,
     * and copies some things to GS that otherwise would not.
     */
    void finalizeGlobalStorage() const;

    /** @brief Jobs for creating network config
     *
     * If the Config object isn't ready yet, returns an empty list.
     */
    Calamares::JobList createJobs() const;

    /// The network interface name to be used
    QString networkInterface() const { return m_networkInterface; }
    /// The ip address to be used
    QString ipAddress() const { return m_ipAddress; }
    /// Status message about ip address
    QString ipAddressStatus() const;
    /// The subnet mask to be used
    QString mask() const { return m_mask; }
    /// Status message about mask
    QString maskStatus() const;
    /// The gateway address to be used
    QString gateway() const { return m_gateway; }
    /// Status message about gateway
    QString gatewayStatus() const;

    bool isReady() const;

public Q_SLOTS:
    /// Sets the network interface
    void setNetworkInterface( const QString& interface );
    /// Sets the ipv4 address
    void setIpAddress( const QString& ipaddress );
    /// Sets the subnet mask value
    void setMask( const QString& mask );
    /// Sets the gateway address
    void setGateway( const QString& gateway );

signals:
    void networkInterfaceChanged( const QString& );
    void ipAddressChanged( const QString& );
    void ipAddressStatusChanged( const QString& );
    void maskChanged( const QString& );
    void maskStatusChanged( const QString& );
    void gatewayChanged( const QString& );
    void gatewayStatusChanged( const QString& );
    void readyChanged( bool ) const;

private:
    void checkReady();

    QString m_networkInterface;
    QString m_ipAddress;
    QString m_mask;
    QString m_gateway;

    bool m_isReady = false;  ///< Used to reduce readyChanged signals
};

#endif
