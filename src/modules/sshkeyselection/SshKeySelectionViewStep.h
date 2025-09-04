/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef SSHKEYSELECTIONVIEWSTEP_H
#define SSHKEYSELECTIONVIEWSTEP_H

#include "DllMacro.h"
#include "utils/PluginFactory.h"
#include "viewpages/ViewStep.h"

class Config;
class SshKeySelectionPage;

class PLUGINDLLEXPORT SshKeySelectionViewStep : public Calamares::ViewStep
{
    Q_OBJECT

public:
    explicit SshKeySelectionViewStep( QObject* parent = nullptr );
    ~SshKeySelectionViewStep() override;

    QString prettyName() const override;

    QWidget* widget() override;

    bool isNextEnabled() const override; // NEW / adapt
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    void onActivate() override;

    Calamares::JobList jobs() const override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    Config* m_config;
    SshKeySelectionPage* m_widget;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION( SshKeySelectionViewStepFactory )

#endif
