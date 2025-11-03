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
#include "GlobalStorage.h"
#include "utils/Logger.h"

#include <QApplication>
#include <QFile>
#include <QVariantList>
#include <QVariantMap>

extern "C" {
#include <zlib.h>
}

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
    return true;
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

void
ImageSelectionViewStep::onLeave()
{
    auto* gs = Calamares::JobQueue::instance()->globalStorage();

    cDebug() << "Running GPT parsing on leaving imageselection";

    QVariant selectedFilesVariant = gs->value( "imageselection.selectedFiles" );
    QStringList selectedFiles = selectedFilesVariant.toStringList();

    if ( selectedFiles.isEmpty() )
    {
        cDebug() << "No selected image in imageselection page.";
        return;
    }

    QString imagePath = selectedFiles.first();
    cDebug() << "Parsing GPT from image:" << imagePath;

    // Read GPT headers from gzipped image
    const int BLOCK_SIZE = 512; // Default GPT block size
    const int COUNT = 40; // 40 * 512 = 20KB, enough for GPT headers
    QByteArray rawData;
    rawData.reserve( BLOCK_SIZE * COUNT );

    gzFile gzf = gzopen( imagePath.toUtf8().constData(), "rb" );
    if ( !gzf )
    {
        cError() << "Failed to open gzipped image:" << imagePath;
        ::exit( EXIT_FAILURE );
    }

    char buffer[ BLOCK_SIZE ];
    for ( int i = 0; i < COUNT; ++i )
    {
        int bytesRead = gzread( gzf, buffer, BLOCK_SIZE );
        if ( bytesRead <= 0 )
            break;
        rawData.append( buffer, bytesRead );
    }
    gzclose( gzf );

    if ( rawData.size() < 1024 )
    {
        cError() << "Failed to read enough data from image";
        ::exit( EXIT_FAILURE );
    }

    // Parse GPT header (at block 1, offset 512)
    const unsigned char* gptHeader = reinterpret_cast< const unsigned char* >( rawData.constData() + 512 );

    // Extract GPT header fields
    quint32 numEntries = *reinterpret_cast< const quint32* >( gptHeader + 80 );
    quint32 entrySize = *reinterpret_cast< const quint32* >( gptHeader + 84 );
    quint64 partLba = *reinterpret_cast< const quint64* >( gptHeader + 72 );

    cDebug() << "GPT detected: entries= " << numEntries << " entry_size= " << entrySize << " table_lba= " << partLba;

    // Parse partition entries (starting at block 2, offset 1024)
    const unsigned char* entriesData = reinterpret_cast< const unsigned char* >( rawData.constData() + 1024 );
    int maxEntries = qMin( static_cast< int >( numEntries ), ( rawData.size() - 1024 ) / static_cast< int >( entrySize ) );

    QVariantList partitions;
    for ( int i = 0; i < maxEntries; ++i )
    {
        const unsigned char* entry = entriesData + ( i * entrySize );

        quint64 firstLba = *reinterpret_cast< const quint64* >( entry + 32 );
        quint64 lastLba = *reinterpret_cast< const quint64* >( entry + 40 );
        quint64 attrs = *reinterpret_cast< const quint64* >( entry + 48 );

        // Extract partition name (UTF-16LE, starts at offset 56)
        QString name;
        const quint16* nameData = reinterpret_cast< const quint16* >( entry + 56 );
        int maxNameChars = ( entrySize - 56 ) / 2;

        if ( firstLba == 0 && lastLba == 0 )
            continue;

        for ( int j = 0; j < maxNameChars; ++j )
        {
            if ( nameData[ j ] == 0 )
                break;
            name.append( QChar( nameData[ j ] ) );
        }



        quint64 sizeSectors = lastLba - firstLba + 1;

        cDebug() << ( i + 1 ) << ": start=" << firstLba << "end=" << lastLba
                 << "size_sectors=" << sizeSectors << "name='" << name << "' attrs=0x" << QString::number( attrs, 16 );

        // Extract GUIDs as hex strings
        QByteArray typeGuid( reinterpret_cast< const char* >( entry ), 16 );
        QByteArray uniqGuid( reinterpret_cast< const char* >( entry + 16 ), 16 );

        QVariantMap partition;
        partition[ "index" ] = i + 1;
        partition[ "type_guid" ] = typeGuid.toHex();
        partition[ "uniq_guid" ] = uniqGuid.toHex();
        partition[ "first_lba" ] = firstLba;
        partition[ "last_lba" ] = lastLba;
        partition[ "size_sectors" ] = sizeSectors;
        partition[ "name" ] = name;
        partition[ "attrs" ] = attrs;

        partitions.append( partition );
    }

    cDebug() << "Extracted" << partitions.size() << "partitions from GPT layout";

    gs->insert( "imageselection.gptPartitions", partitions );
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
