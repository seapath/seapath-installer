/* === This file is part of Seapath installer ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2017 Gabriel Craciunescu <crazy@frugalware.org>
 *   SPDX-FileCopyrightText: 2025 Savoir-faire Linux, Inc.
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "NetworkViewStep.h"

#include "Config.h"
#include "NetworkPage.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"
#include "utils/NamedEnum.h"
#include "utils/Variant.h"

CALAMARES_PLUGIN_FACTORY_DEFINITION( NetworkViewStepFactory, registerPlugin< NetworkViewStep >(); )

NetworkViewStep::NetworkViewStep( QObject* parent )
    : Calamares::ViewStep( parent )
    , m_widget( nullptr )
    , m_config( new Config( this ) )
{
    connect( m_config, &Config::readyChanged, this, &NetworkViewStep::nextStatusChanged );

    emit nextStatusChanged( m_config->isReady() );
}


NetworkViewStep::~NetworkViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}


QString
NetworkViewStep::prettyName() const
{
    return tr( "Network" );
}


QWidget*
NetworkViewStep::widget()
{
    if ( !m_widget )
    {
        m_widget = new NetworkPage( m_config );
    }
    return m_widget;
}


bool
NetworkViewStep::isNextEnabled() const
{
    return m_config->isReady();
}


bool
NetworkViewStep::isBackEnabled() const
{
    return true;
}


bool
NetworkViewStep::isAtBeginning() const
{
    return true;
}


bool
NetworkViewStep::isAtEnd() const
{
    return true;
}


Calamares::JobList
NetworkViewStep::jobs() const
{
    return m_config->createJobs();
}


void
NetworkViewStep::onActivate()
{
    if ( m_widget )
    {
        m_widget->onActivate();
    }
}


void
NetworkViewStep::onLeave()
{
    m_config->finalizeGlobalStorage();
}


void
NetworkViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
}
