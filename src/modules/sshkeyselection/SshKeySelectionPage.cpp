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

#include "SshKeySelectionPage.h"

#include "Config.h"
#include "ui_SshKeySelectionPage.h"
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

SshKeySelectionPage::SshKeySelectionPage( Config* config, QWidget* parent )
    : QWidget( parent )
    , ui( new Ui::SshKeySelectionPage )
{
    ui->setupUi( this );

    ui->treeWidget->setSelectionMode( QAbstractItemView::NoSelection );
    ui->treeWidget->header()->setStretchLastSection( true );
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    ui->treeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
#else
    ui->treeWidget->header()->setResizeMode(0, QHeaderView::ResizeToContents);
#endif
    scanAvailableKeys();

    connect( ui->treeWidget, &QTreeWidget::itemChanged, this, [this]( QTreeWidgetItem* changed, int column )
    {

        if ( column != 0 )
            return;

        // Collect checked items
        QStringList selectedKeys;

        for ( int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i )
        {
            auto* it = ui->treeWidget->topLevelItem( i );
            if ( it->checkState(0) == Qt::Checked )
            {
                selectedKeys << it->data( 0, Qt::UserRole ).toString(); // hidden path
                cDebug() << "debug:" << selectedKeys;
            }
        }
        cDebug() << "imageselection: selected keys:" << selectedKeys;
        emit selectionChanged( !selectedKeys.isEmpty() );

        auto* gs = Calamares::JobQueue::instance()->globalStorage();
        gs->insert( "sshkeyselection.selectedKeys", selectedKeys );

    } );
}

bool SshKeySelectionPage::hasSelection() const
{
    for ( int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i )
        if ( ui->treeWidget->topLevelItem( i )->checkState(0) == Qt::Checked )
            return true;
    return false;
}

void
SshKeySelectionPage::scanAvailableKeys()
{
    QDir dir("/seapath/ssh");

    QStringList pub_files = dir.entryList(QStringList() << "*.pub", QDir::Files);
    cDebug() << "sshkeyselection: scanning" << dir.path() << "found" << pub_files.size() << "key file(s)";
    for (const QString& fn : pub_files)
    {
        QFile f(dir.absoluteFilePath(fn));
        if (!f.open(QIODevice::ReadOnly))
        {
            cDebug() << "sshkeyselection: cannot open" << f.fileName();
            continue;
        }

        // Optionally, get the comment from the public key line
        QString keyLine = QString::fromUtf8(f.readLine()).trimmed();
        f.close();
        QString keyLabel = fn;
        QStringList parts = keyLine.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 3)
            keyLabel = parts[2]; // Use the comment as the label if present

        cDebug() << "sshkeyselection: found key" << keyLabel;

        m_availableImages << keyLabel;

        auto* item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, keyLabel);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, Qt::Unchecked);

        item->setData(0, Qt::UserRole, dir.absoluteFilePath(fn)); // Store the file path
    }
}


void
SshKeySelectionPage::focusInEvent( QFocusEvent* e )
{
    e->accept();
}

void
SshKeySelectionPage::onInstallationFailed( const QString& message, const QString& details )
{
    m_failure = !message.isEmpty() ? message : details;
    retranslate();
}

void
SshKeySelectionPage::retranslate()
{

    const auto* branding = Calamares::Branding::instance();
}
