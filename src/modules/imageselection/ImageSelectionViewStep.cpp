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

#include "ImageSelectionViewStep.h"

#include "Config.h"
#include "ImageSelectionPage.h"

#include "JobQueue.h"

#include <QApplication>

ImageSelectionViewStep::ImageSelectionViewStep( QObject* parent )
    : Calamares::ViewStep( parent )
    , m_config( new Config( this ) )
    , m_widget( new ImageSelectionPage( m_config ) )
{
    auto jq = Calamares::JobQueue::instance();
    connect( jq, &Calamares::JobQueue::failed, m_config, &Config::onInstallationFailed );
    connect( jq, &Calamares::JobQueue::failed, m_widget, &ImageSelectionPage::onInstallationFailed );

    emit nextStatusChanged( true );
            connect( m_widget, &ImageSelectionPage::selectionChanged,
            this, [this]( bool ) { emit nextStatusChanged( isNextEnabled() ); } );
}


ImageSelectionViewStep::~ImageSelectionViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}


QString
ImageSelectionViewStep::prettyName() const
{
    return tr( "Image selection", "@label" );
}


QWidget*
ImageSelectionViewStep::widget()
{
    return m_widget;
}


bool
ImageSelectionViewStep::isNextEnabled() const
{
    return m_widget && m_widget->hasSelection();
}


bool
ImageSelectionViewStep::isBackEnabled() const
{
    return false;
}


bool
ImageSelectionViewStep::isAtBeginning() const
{
    return true;
}


bool
ImageSelectionViewStep::isAtEnd() const
{
    return true;
}


void
ImageSelectionViewStep::onActivate()
{
    m_config->doNotify();
    connect( qApp, &QApplication::aboutToQuit, m_config, qOverload<>( &Config::doRestart ) );
}


Calamares::JobList
ImageSelectionViewStep::jobs() const
{
    return Calamares::JobList();
}

void
ImageSelectionViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
}

CALAMARES_PLUGIN_FACTORY_DEFINITION( ImageSelectionViewStepFactory, registerPlugin< ImageSelectionViewStep >(); )
