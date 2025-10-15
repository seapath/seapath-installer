//   SPDX-FileCopyrightText: 2025 Savoir-faire Linux, Inc.
//   SPDX-License-Identifier: GPL-3.0-or-later


#include "SshKeySelectionJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include <QDir>
#include <QFile>

SshKeySelectionJob::SshKeySelectionJob(bool skipIfNoRoot )
    : Calamares::Job()
    , m_skipIfNoRoot( skipIfNoRoot )
{
}

Calamares::JobResult
SshKeySelectionJob::exec()
{
    cDebug() << "Executing SshKeySelectionJob";

    Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();
    QDir destDir( gs->value( "homeMountPoint" ).toString() );
    QString sshKeys = gs->value( "sshkeyselection.selectedKeys" ).toString();
    cDebug() << "SshKeySelectionJob: selected keys:" << sshKeys;

    if ( !( m_skipIfNoRoot && ( destDir.isEmpty() || destDir.isRoot() ) ) )
    {
        QString dotSSHConfPath = destDir.absoluteFilePath( "admin/.ssh" );
        cDebug() << "SshKeySelectionJob: writing selected keys to" << dotSSHConfPath;

        for (const QString& keyPath : sshKeys.split(',', Qt::SkipEmptyParts))
        {
            cDebug() << "SshKeySelectionJob: processing key" << keyPath;
            QFile srcFile(keyPath);
            if (!srcFile.open(QIODevice::ReadOnly))
            {
                cError() << "SshKeySelectionJob: cannot open source key file" << srcFile.fileName();
                return Calamares::JobResult::error(
                    tr("Failed to open SSH key file.", "@error"),
                    tr("Could not open SSH key file %1 for reading.", "@error, %1 is the SSH key file path").arg(srcFile.fileName()));
            }

            QString destFilePath = dotSSHConfPath + "/authorized_keys";
            QFile destFile(destFilePath);
            if (!destFile.open(QIODevice::Append | QIODevice::Text))
            {
                cError() << "SshKeySelectionJob: cannot open destination key file" << destFile.fileName();
                return Calamares::JobResult::error(
                    tr("Failed to write SSH key file.", "@error"),
                    tr("Could not open SSH key file %1 for writing.", "@error, %1 is the SSH key file path").arg(destFile.fileName()));
            }

            destFile.write(srcFile.readAll());
            srcFile.close();
            destFile.close();

            if (destFile.error() != QFile::NoError)
            {
                cError() << "SshKeySelectionJob: error writing to destination key file" << destFile.fileName();
                return Calamares::JobResult::error(
                    tr("Failed to write SSH key file.", "@error"),
                    tr("Error occurred while writing SSH key file %1.", "@error, %1 is the SSH key file path").arg(destFile.fileName()));
            }

            cDebug() << "SshKeySelectionJob: successfully copied key to" << destFilePath;
        }


    }

    return Calamares::JobResult::ok();
}

QString
SshKeySelectionJob::prettyName() const
{
    return tr( "SSH Key Selection" );
}
