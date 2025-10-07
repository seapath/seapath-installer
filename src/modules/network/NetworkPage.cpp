/* === This file is part of Seapath installer ===
 *
 *   SPDX-FileCopyrightText: 2014-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Collabora Ltd <arnaud.ferraris@collabora.com>
 *   SPDX-FileCopyrightText: 2020 Gabriel Craciunescu <crazy@frugalware.org>
 *   SPDX-FileCopyrightText: 2025 Savoir-faire Linux, Inc.
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Portions from the Manjaro Installation Framework
 *   by Roland Singer <roland@manjaro.org>
 *   Copyright (C) 2007 Free Software Foundation, Inc.
 *
 */

#include "NetworkPage.h"

#include "Config.h"
#include "ui_NetworkPage.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "Settings.h"
#include "compat/CheckBox.h"
#include "utils/Gui.h"
#include "utils/Logger.h"
#include "utils/Retranslator.h"
#include "utils/String.h"

#include <QBoxLayout>
#include <QFile>
#include <QLabel>
#include <QLineEdit>

/** @brief Add an error message and pixmap to a label. */
static inline void
labelError( QLabel* pix, QLabel* label, Calamares::ImageType icon, const QString& message )
{
    label->setText( message );
    pix->setPixmap( Calamares::defaultPixmap( icon, Calamares::Original, label->size() ) );
}

/** @brief Clear error, set happy pixmap on a label to indicate "ok". */
static inline void
labelOk( QLabel* pix, QLabel* label )
{
    label->clear();
    pix->setPixmap( Calamares::defaultPixmap( Calamares::StatusOk, Calamares::Original, label->size() ) );
}

/** @brief Sets error or ok on a label depending on @p status and @p value
 *
 * - An **empty** @p value gets no message and no icon.
 * - A non-empty @p value, with an **empty** @p status gets an "ok".
 * - A non-empty @p value with a non-empty @p status gets an error indicator.
 */
static inline void
labelStatus( QLabel* pix, QLabel* label, const QString& value, const QString& status )
{
    if ( status.isEmpty() )
    {
        if ( value.isEmpty() )
        {
            // This is different from labelOK() because no checkmark is shown
            label->clear();
            pix->clear();
        }
        else
        {
            labelOk( pix, label );
        }
    }
    else
    {
        labelError( pix, label, Calamares::ImageType::StatusError, status );
    }
}

NetworkPage::NetworkPage( Config* config, QWidget* parent )
    : QWidget( parent )
    , ui( new Ui::NetworkPage )
    , m_config( config )
{
    ui->setupUi( this );

    // Connect signals and slots
    ui->checkBoxUseDhcp->setChecked( m_config->useDhcpForStaticIp() );
    connect( config, &Config::useDhcpForStaticIpChanged, ui->checkBoxUseDhcp, &QCheckBox::setChecked );
    connect( ui->checkBoxUseDhcp, Calamares::checkBoxStateChangedSignal, this, &NetworkPage::onUseDhcpChanged  );

    ui->textBoxNetworkInterface->setText( config->networkInterface() );
    connect( ui->textBoxNetworkInterface, &QLineEdit::textEdited, config, &Config::setNetworkInterface );
    connect( config, &Config::networkInterfaceChanged, this, &NetworkPage::onNetworkInterfaceTextEdited );

    ui->textBoxIpAddress->setText( config->ipAddress() );
    connect( ui->textBoxIpAddress, &QLineEdit::textEdited, config, &Config::setIpAddress );
    connect( config, &Config::ipAddressStatusChanged, this, &NetworkPage::reportIpAddressStatus );

    ui->textBoxMask->setText( config->mask() );
    connect( ui->textBoxMask, &QLineEdit::textEdited, config, &Config::setMask );
    connect( config, &Config::maskStatusChanged, this, &NetworkPage::reportMaskStatus );

    ui->textBoxGateway->setText( config->gateway() );
    connect( ui->textBoxGateway, &QLineEdit::textEdited, config, &Config::setGateway );
    connect( config, &Config::gatewayStatusChanged, this, &NetworkPage::reportGatewayStatus );

    CALAMARES_RETRANSLATE_SLOT( &NetworkPage::retranslate );

    onUseDhcpChanged( m_config->useDhcpForStaticIp() );
    onNetworkInterfaceTextEdited( m_config->networkInterface() );
    reportIpAddressStatus( m_config->ipAddressStatus() );
    reportGatewayStatus( m_config->gatewayStatus() );
    ui->textBoxNetworkInterface->setEnabled( m_config->isEditable( "networkInterface" ) );

    retranslate();
}

NetworkPage::~NetworkPage()
{
    delete ui;
}

void
NetworkPage::retranslate()
{
    ui->retranslateUi( this );
}

void
NetworkPage::onActivate()
{
    ui->textBoxNetworkInterface->setFocus();
}

void
NetworkPage::onNetworkInterfaceTextEdited( const QString& networkInterface )
{
    labelStatus( ui->labelNetworkInterface, ui->labelNetworkInterfaceError, networkInterface, QString() );
}

void
NetworkPage::reportIpAddressStatus( const QString& status )
{
    labelStatus( ui->labelIpAddress, ui->labelIpAddressError, m_config->ipAddress(), status );
}

void
NetworkPage::reportMaskStatus( const QString& status )
{
    labelStatus( ui->labelMask, ui->labelMaskError, m_config->mask(), status );
}

void
NetworkPage::reportGatewayStatus( const QString& status )
{
    labelStatus( ui->labelGateway, ui->labelGatewayError, m_config->gateway(), status );
}

void
NetworkPage::onUseDhcpChanged( const int checked )
{
    // Pass the change to config
    m_config->setUseDhcpForStaticIp( checked != Qt::Unchecked );
    // When the useDhcp checkbox is checked, Ip address, Mask and
    // Gateway text boxes should not editable.
    const bool editable = !m_config->useDhcpForStaticIp();
    ui->textBoxIpAddress->setEnabled( editable );
    ui->textBoxMask->setEnabled( editable );
    ui->textBoxGateway->setEnabled( editable );
}
