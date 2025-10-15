//   SPDX-FileCopyrightText: 2025 Savoir-faire Linux, Inc.
//   SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SSHKEYSELECTIONJOB_H
#define SSHKEYSELECTIONJOB_H
#include "Job.h"
#include "utils/Logger.h"

class SshKeySelectionJob : public Calamares::Job
{
    Q_OBJECT
public:
    SshKeySelectionJob( bool skipIfNoRoot );

    Calamares::JobResult exec() override;
    QString prettyName() const override;

private:
    const bool m_skipIfNoRoot;
};

#endif /* SSHKEYSELECTIONJOB_H */
