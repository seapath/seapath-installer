/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Collabora Ltd <arnaud.ferraris@collabora.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "ImageSelectionPage.h"

#include "Config.h"
#include "ui_ImageSelectionPage.h"
#include "GlobalStorage.h"
#include "JobQueue.h"

#include "Branding.h"
#include "Settings.h"
#include "compat/CheckBox.h"
#include "utils/Retranslator.h"
#include <QProcess>

#include <QFocusEvent>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

ImageSelectionPage::ImageSelectionPage( Config* config, QWidget* parent )
    : QWidget( parent )
    , ui( new Ui::ImageSelectionPage )
{
    ui->setupUi( this );

    // Example list (later: read from config file imageselection.conf)

    // const ImageRow images[] = {
    //     { "Minimal Image",   "1.0.1", "A minimal image to test your system" },
    //     { "Standard Image",  "1.2.0", "Standard desktop environment" },
    //     { "Developer Image", "2.0.0", "Includes build and debug tools" },
    //     { "Rescue Image",    "1.0.0", "Recovery / rescue utilities" }
    // };

    ui->treeWidget->setSelectionMode( QAbstractItemView::NoSelection );
    ui->treeWidget->header()->setStretchLastSection( true );
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    ui->treeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->treeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->treeWidget->header()->setSectionResizeMode(2, QHeaderView::Stretch);
#else
    ui->treeWidget->header()->setResizeMode(0, QHeaderView::ResizeToContents);
    ui->treeWidget->header()->setResizeMode(1, QHeaderView::ResizeToContents);
    ui->treeWidget->header()->setResizeMode(2, QHeaderView::Stretch);
#endif
    scanAvailableImages();


    connect( ui->treeWidget, &QTreeWidget::itemChanged, this, [this]( QTreeWidgetItem* changed, int column )
    {

        if ( column != 0 )
            return;

        if ( changed->checkState(0) == Qt::Checked )
        {
            // Enforce single selection (optional)
            for ( int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i )
            {
                auto* it = ui->treeWidget->topLevelItem( i );
                if ( it != changed && it->checkState(0) == Qt::Checked )
                    it->setCheckState(0, Qt::Unchecked);
            }
        }

        // Collect checked items
        QStringList selected;
        QStringList selectedFiles;     // hidden file paths

        for ( int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i )
        {
            auto* it = ui->treeWidget->topLevelItem( i );
            if ( it->checkState(0) == Qt::Checked )
            {
                selected << it->text(0);
                selectedFiles << it->data( 0, Qt::UserRole ).toString(); // hidden path
                cDebug() << "debug:" << selectedFiles;
            }
        }

        cDebug() << "imageselection: selected images:" << selected;
        cDebug() << "imageselection: selected files:" << selectedFiles;

        if ( !selected.isEmpty() )
        {
            emit selectionChanged( !selected.isEmpty() );

            auto* gs = Calamares::JobQueue::instance()->globalStorage();
            gs->insert( "imageselection.selected", selected );
            gs->insert( "imageselection.selectedFiles", selectedFiles );

            if ( selected[0].toLower().contains("debian") )
            {
                gs->insert( "seapathFlavor", "debian" );
            }
            else
            {
                gs->insert( "seapathFlavor", "yocto" );
            }
        }

        else{
            // Disable next button if nothing selected
            emit selectionChanged( selected.isEmpty() );
        }
    } );
}

bool ImageSelectionPage::hasSelection() const
{
    for ( int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i )
        if ( ui->treeWidget->topLevelItem( i )->checkState(0) == Qt::Checked )
            return true;
    return false;
}

void
ImageSelectionPage::scanAvailableImages()
{
    QDir dir( "/seapath/images" );

    QStringList image_files = dir.entryList(QStringList() << "*.wic.gz" << "*.raw.gz" << "*.iso", QDir::Files);
    cDebug() << "imageselection: scanning" << dir.path() << "found" << image_files.size() << "image file(s)";
    for ( const QString& fn : image_files )
    {
        QString gz_image = fn;
        QString json = fn;

        json.replace(QRegularExpression("\\.(wic\\.gz|raw\\.gz|iso)$"), ".json");
        gz_image.replace(QRegularExpression("\\.json$"), ".gz");

        cDebug() << "imageselection: found" << fn;
        cDebug() << "imageselection: looking for metadata file" << json;
        cDebug() << "JSON path:" << dir.absoluteFilePath( json );
        if ( !QFile::exists( dir.absoluteFilePath( json ) ) )
        {
            cDebug() << "imageselection: no metadata file found for" << fn;
            // Not a JSON file, just add it with minimal info
            QString name = fn;
            name.replace(QRegularExpression("\\.(wic\\.gz|raw\\.gz|iso)$"), "");
            cDebug() << "imageselection: found image" << name << "(non-JSON file)";
            m_availableImages << name;

            auto* item = new QTreeWidgetItem( ui->treeWidget );
            item->setText( 0, name );
            item->setText( 1, "-" );
            item->setText( 2, "(no description available)" );
            item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
            item->setCheckState( 0, Qt::Unchecked );

            item->setData( 0, Qt::UserRole, dir.absoluteFilePath( gz_image ) );          // original file path
            continue;
        }

        else {
            QFile jsonFile( dir.absoluteFilePath( json ) );
            if ( !jsonFile.open( QIODevice::ReadOnly ) )
            {
                cDebug() << "imageselection: cannot open JSON file" << jsonFile.fileName();
                continue;
            }

            QJsonParseError pe;
            QJsonDocument doc = QJsonDocument::fromJson( jsonFile.readAll(), &pe );

            QJsonObject o = doc.object();
            QString name = o.value( "name" ).toString( fn );
            QString version = o.value( "version" ).toString( "-" );
            QString description = o.value( "description" ).toString();

            cDebug() << "imageselection: found image" << name << version << description;
            m_availableImages << name;

            auto* item = new QTreeWidgetItem( ui->treeWidget );
            item->setText( 0, name );
            item->setText( 1, version );
            item->setText( 2, description );
            item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
            item->setCheckState( 0, Qt::Unchecked );

            item->setData( 0, Qt::UserRole, dir.absoluteFilePath( fn ) );          // original JSON file path
            jsonFile.close();
            continue;
        }


    }
}


void
ImageSelectionPage::focusInEvent( QFocusEvent* e )
{
    e->accept();
}

void
ImageSelectionPage::onInstallationFailed( const QString& message, const QString& details )
{
    m_failure = !message.isEmpty() ? message : details;
    retranslate();
}

void
ImageSelectionPage::retranslate()
{

    const auto* branding = Calamares::Branding::instance();
}
