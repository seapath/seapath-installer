/* === This file is part of Seapath installer ===
 *
 *   SPDX-FileCopyrightText: 2007 Free Software Foundation, Inc.
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2025 Savoir-faire Linux, Inc.
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Portions from the Manjaro Installation Framework
 *   by Roland Singer <roland@manjaro.org>
 *   Copyright (C) 2007 Free Software Foundation, Inc.
 *
 */

#ifndef NETWORKPAGE_H
#define NETWORKPAGE_H

#include <QWidget>

class Config;

class QLabel;

namespace Ui
{
class NetworkPage;
}  // namespace Ui

class NetworkPage : public QWidget
{
    Q_OBJECT
public:
    explicit NetworkPage( Config* config, QWidget* parent = nullptr );
    ~NetworkPage() override;

    void onActivate();

protected slots:
    void onUseDhcpChanged( const int checked );
    void onNetworkInterfaceTextEdited( const QString& );
    void reportIpAddressStatus( const QString& );
    void reportMaskStatus( const QString& );
    void reportGatewayStatus( const QString& );

private:
    void retranslate();

    Ui::NetworkPage* ui;
    Config* m_config;
};

#endif  // NETWORKPAGE_H
