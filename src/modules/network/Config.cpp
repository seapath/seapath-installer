/* === This file is part of Seapath installer ===
 *
 *   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2025 Savoir-faire Linux, Inc.
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "Config.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "compat/Variant.h"
#include "utils/Logger.h"
#include "utils/Permissions.h"
#include "utils/String.h"
#include "utils/StringExpander.h"
#include "utils/Variant.h"

#include <QCoreApplication>
#include <QFile>
#include <QMetaProperty>
#include <QRegularExpression>
#include <QTimer>

#include <memory>

static const QRegularExpression IPV4ADDRESS_RX( "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$" );
static constexpr const int IPV4ADDRESS_MIN_LENGTH = 7;

static const QRegularExpression MASK_RX( "^(3[0-2]|[1-2]?[0-9])$" );
static const QString SUBNET_MASK_DEFAULT = QString::number(24);

Config::Config( QObject* parent )
    : Calamares::ModuleSystem::Config( parent )
{
    emit readyChanged( m_isReady );  // false

    // Gang together all the changes of status to one readyChanged() signal
    connect( this, &Config::useDhcpForStaticIpChanged, this, &Config::checkReady );
    connect( this, &Config::networkInterfaceChanged, this, &Config::checkReady );
    connect( this, &Config::ipAddressStatusChanged, this, &Config::checkReady );
    connect( this, &Config::maskStatusChanged, this, &Config::checkReady );
    connect( this, &Config::gatewayStatusChanged, this, &Config::checkReady );

}

Config::~Config() {}

static inline void
insertInGlobalStorage( const QString& key, const QString& group )
{
    auto* gs = Calamares::JobQueue::instance()->globalStorage();
    if ( !gs || group.isEmpty() )
    {
        return;
    }
    gs->insert( key, group );
}

void
Config::setNetworkInterface( const QString& interface )
{
    CONFIG_PREVENT_EDITING( QString, "networkInterface" );

    if ( interface != m_networkInterface )
    {
        m_networkInterface = interface;
        Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();
        if ( interface.isEmpty() )
        {
            gs->remove( "networkInterface" );
        }
        else
        {
            gs->insert( "networkInterface", interface );
        }
        emit networkInterfaceChanged( interface );

    }
}

void
Config::setIpAddress( const QString& ipaddress )
{
    if ( ipaddress != m_ipAddress )
    {
        m_ipAddress = ipaddress;
        Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();
        if ( ipaddress.isEmpty() )
        {
            gs->remove( "ipAddress" );
        }
        else
        {
            gs->insert( "ipAddress", ipaddress );
        }
        emit ipAddressChanged( ipaddress );
        emit ipAddressStatusChanged( ipAddressStatus() );
    }
}

QString
Config::ipAddressStatus() const
{
    if ( m_ipAddress.length() < IPV4ADDRESS_MIN_LENGTH )
    {
        return tr( "Your ipv4 address is too short." );
    }

    if ( m_ipAddress.indexOf( IPV4ADDRESS_RX ) != 0 )
    {
        return tr( "Check the format of your ipv4 address." );
    }

    return QString();
}

void
Config::setMask( const QString& mask )
{
    if ( mask != m_mask )
    {
        m_mask = mask;
        Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();
        if ( mask.isEmpty() )
        {
            m_mask = SUBNET_MASK_DEFAULT;
            gs->insert( "mask", m_mask );
        }
        else
        {
            gs->insert( "mask", mask );
        }

        emit maskChanged( mask );
        emit maskStatusChanged( maskStatus() );
    }
}

QString
Config::maskStatus() const
{
    if ( m_mask.indexOf( MASK_RX ) != 0 )
    {
        return tr( "The value of your subnet mask must be between 0 and 32." );
    }

    return QString();
}

void
Config::setGateway( const QString& gateway )
{
    if ( gateway != m_gateway )
    {
        m_gateway = gateway;
        Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();
        if ( gateway.isEmpty() )
        {
            gs->remove( "gateway" );
        }
        else
        {
            gs->insert( "gateway", gateway );
        }
        emit gatewayChanged( gateway );
        emit gatewayStatusChanged( gatewayStatus() );
    }
}

QString
Config::gatewayStatus() const
{
    if ( m_gateway.length() < IPV4ADDRESS_MIN_LENGTH )
    {
        return tr( "Your gateway address is too short." );
    }

    if ( m_gateway.indexOf( IPV4ADDRESS_RX ) != 0 )
    {
        return tr( "Check the format of your gateway address." );
    }

    return QString();
}

void
Config::setUseDhcpForStaticIp( bool useDhcp )
{
    m_useDhcpForStaticIp = useDhcp;
    Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();
    gs->insert( "useDhcp", useDhcp );
}

bool
Config::isReady() const
{
    bool readyUseDhcp = useDhcpForStaticIp();
    bool readyNetworkInterface = !networkInterface().isEmpty();  // Needs some text
    bool readyIpAddress = ipAddressStatus().isEmpty();
    bool readyGateway = gatewayStatus().isEmpty();
    if ( readyUseDhcp )
    {
        return readyNetworkInterface;
    }
    return readyNetworkInterface && readyIpAddress && readyGateway;
}

/** @brief Update ready status and emit signal
 *
 * This is a "concentrator" private slot for all the status-changed
 * signals, so that readyChanged() is emitted only when needed.
 */
void
Config::checkReady()
{
    bool b = isReady();
    if ( b != m_isReady )
    {
        m_isReady = b;
        emit readyChanged( b );
    }
}


/** @brief Returns a value of either key from the map
 *
 * Takes a function (e.g. getBool, or getString) and two keys,
 * returning the value in the map of the one that is there (or @p defaultArg)
 */
template < typename T, typename U >
T
either( T ( *f )( const QVariantMap&, const QString&, U ),
        const QVariantMap& configurationMap,
        const QString& oldKey,
        const QString& newKey,
        U defaultArg )
{
    if ( configurationMap.contains( oldKey ) )
    {
        return f( configurationMap, oldKey, defaultArg );
    }
    else
    {
        return f( configurationMap, newKey, defaultArg );
    }
}

void
Config::setConfigurationMap( const QVariantMap& configurationMap )
{
    checkReady();
}

void
Config::finalizeGlobalStorage() const
{

    Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();
    if ( !gs->contains("mask") )
    {
        gs->insert( "mask", SUBNET_MASK_DEFAULT );
    }

}

Calamares::JobList
Config::createJobs() const
{
    Calamares::JobList jobs;

    if ( !isReady() )
    {
        return jobs;
    }

    return jobs;
}
