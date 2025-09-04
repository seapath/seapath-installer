/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 2019, Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Collabora Ltd <arnaud.ferraris@collabora.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "SshKeySelectionViewStep.h"

#include "Config.h"
#include "SshKeySelectionPage.h"

#include "JobQueue.h"

#include <QApplication>

SshKeySelectionViewStep::SshKeySelectionViewStep( QObject* parent )
    : Calamares::ViewStep( parent )
    , m_config( new Config( this ) )
    , m_widget( new SshKeySelectionPage( m_config ) )
{
    auto jq = Calamares::JobQueue::instance();
    connect( jq, &Calamares::JobQueue::failed, m_config, &Config::onInstallationFailed );
    connect( jq, &Calamares::JobQueue::failed, m_widget, &SshKeySelectionPage::onInstallationFailed );

    emit nextStatusChanged( true );
            connect( m_widget, &SshKeySelectionPage::selectionChanged,
            this, [this]( bool ) { emit nextStatusChanged( isNextEnabled() ); } );
}


SshKeySelectionViewStep::~SshKeySelectionViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}


QString
SshKeySelectionViewStep::prettyName() const
{
    return tr( "SSH key selection", "@label" );
}


QWidget*
SshKeySelectionViewStep::widget()
{
    return m_widget;
}


bool
SshKeySelectionViewStep::isNextEnabled() const
{
    return m_widget && m_widget->hasSelection();
}


bool
SshKeySelectionViewStep::isBackEnabled() const
{
    return false;
}


bool
SshKeySelectionViewStep::isAtBeginning() const
{
    return true;
}


bool
SshKeySelectionViewStep::isAtEnd() const
{
    return true;
}


void
SshKeySelectionViewStep::onActivate()
{
    m_config->doNotify();
    connect( qApp, &QApplication::aboutToQuit, m_config, qOverload<>( &Config::doRestart ) );
}


Calamares::JobList
SshKeySelectionViewStep::jobs() const
{
    return Calamares::JobList();
}

void
SshKeySelectionViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
}

CALAMARES_PLUGIN_FACTORY_DEFINITION( SshKeySelectionViewStepFactory, registerPlugin< SshKeySelectionViewStep >(); )
