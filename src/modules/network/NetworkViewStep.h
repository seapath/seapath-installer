/* === This file is part of Seapath installer ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2025 Savoir-faire Linux, Inc.
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef USERSPAGEPLUGIN_H
#define USERSPAGEPLUGIN_H

#include "DllMacro.h"
#include "utils/PluginFactory.h"
#include "viewpages/ViewStep.h"

#include <QObject>
#include <QVariant>

class Config;
class NetworkPage;

class PLUGINDLLEXPORT NetworkViewStep : public Calamares::ViewStep
{
    Q_OBJECT

public:
    explicit NetworkViewStep( QObject* parent = nullptr );
    ~NetworkViewStep() override;

    QString prettyName() const override;

    QWidget* widget() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    Calamares::JobList jobs() const override;

    void onActivate() override;
    void onLeave() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    NetworkPage* m_widget;
    Config* m_config;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION( NetworkViewStepFactory )

#endif  // NETWORKPAGEPLUGIN_H
