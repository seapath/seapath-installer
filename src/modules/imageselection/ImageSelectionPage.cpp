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
#include <QtXml/QDomDocument>

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
        QStringList seapathFlavor;
        auto* gs = Calamares::JobQueue::instance()->globalStorage();

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

            gs->insert( "imageselection.selected", selected );
            gs->insert( "imageselection.selectedFiles", selectedFiles );
            gs->insert( "seapathFlavor", seapathFlavor[0] );
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
        QString bmap = fn;
        QString imageName;
        QString imageDescription;
        QString imageVersion;
        QString imageFlavor;
        auto* gs = Calamares::JobQueue::instance()->globalStorage();

        bmap.replace(QRegularExpression("(\\.gz)$"), ".bmap");
        gz_image.replace(QRegularExpression("\\.json$"), ".gz");

        cDebug() << "imageselection: found" << fn;
        cDebug() << "imageselection: looking for bmap file" << bmap;

        /* Bmap file not readable/not present
        --> No metadata support, marked them as 'Not available'
        */
        if ( !QFile::exists( dir.absoluteFilePath( bmap ) ) )
        {
            cDebug() << "imageselection: no BMAP metadata file found for" << fn;
            QString name = fn;
            auto* item = new QTreeWidgetItem( ui->treeWidget );
            item->setText( 0, name );
            item->setText( 1, "Not available" );
            item->setText( 2, "Not available" );
            item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
            item->setCheckState( 0, Qt::Unchecked );
            item->setData( 0, Qt::UserRole, dir.absoluteFilePath( fn ) ); // Checkbox metadata (hidden)
            continue;
        }

        else {
            QFile bmapFile( dir.absoluteFilePath( bmap ) );
            if ( !bmapFile.open( QIODevice::ReadOnly ) )
            {
                cDebug() << "imageselection: cannot open BMAP file" << bmapFile.fileName();
                continue;
            }

            QDomDocument xmlBMAP;
            xmlBMAP.setContent(&bmapFile);

            QDomElement root=xmlBMAP.documentElement();

            // Extract value of a XML tag field
            auto getElementText = [](const QDomElement& parent, const QString& tag) -> QString {
                QDomElement elem = parent.firstChildElement(tag);
                return elem.text().trimmed();
            };

            // If XML tag do not exist mark the value as not available
            auto checkEmptyElement = [](const QString& tag) -> QString {
                if(tag == "")
                    return "Not available";
                return tag;
            };

            imageName = getElementText(root, "ImageName");

            // If no image name defined print the image file name
            if(imageName == "")
                imageName = fn;

            imageVersion = getElementText(root, "ImageVersion");
            imageDescription = getElementText(root, "ImageDescription");
            imageFlavor = getElementText(root, "ImageFlavor");

            imageVersion = checkEmptyElement(imageVersion);
            imageDescription = checkEmptyElement(imageDescription);
            imageFlavor = checkEmptyElement(imageFlavor);

            auto* item = new QTreeWidgetItem( ui->treeWidget );
            item->setText( 0, imageName );
            item->setText( 1, imageFlavor );
            item->setText( 2, imageVersion );
            item->setText( 3, imageDescription );
            item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
            item->setCheckState( 0, Qt::Unchecked );

            item->setData( 0, Qt::UserRole, dir.absoluteFilePath( fn ) );
            bmapFile.close();
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
