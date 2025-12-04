/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "Config.h"

#include "Branding.h"
#include "Settings.h"
#include "utils/Logger.h"
#include "utils/Variant.h"

#include <QProcess>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>

const NamedEnumTable< Config::RestartMode >&
restartModes()
{
    using M = Config::RestartMode;
    static const NamedEnumTable< M > table { { "never", M::Never },
                                             { "user-unchecked", M::UserDefaultUnchecked },
                                             { "unchecked", M::UserDefaultUnchecked },
                                             { "user-checked", M::UserDefaultChecked },
                                             { "checked", M::UserDefaultChecked },
                                             { "always", M::Always }

    };
    return table;
}

Config::Config( QObject* parent )
    : QObject( parent )
{
}


void
Config::onInstallationFailed( const QString& message, const QString& details )
{
    const bool msgChange = message != m_failureMessage;
    const bool detChange = details != m_failureDetails;
    m_failureMessage = message;
    m_failureDetails = details;
    if ( msgChange )
    {
        emit failureMessageChanged( message );
    }
    if ( detChange )
    {
        emit failureDetailsChanged( message );
    }
    if ( ( msgChange || detChange ) )
    {
        emit failureChanged( hasFailed() );
    }
}


void
Config::doNotify( bool hasFailed, bool sendAnyway )
{
    const char* const failName = hasFailed ? "failed" : "succeeded";

    if ( !sendAnyway )
    {
        cDebug() << "Notification not sent; completion:" << failName;
        return;
    }

    QDBusInterface notify(
        "org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications" );
    if ( notify.isValid() )
    {
        cDebug() << "Sending notification of completion:" << failName;

        QString title;
        QString message;
        if ( hasFailed )
        {
            title = Calamares::Settings::instance()->isSetupMode() ? tr( "Setup Failed", "@title" )
                                                                   : tr( "Installation Failed", "@title" );
            message = Calamares::Settings::instance()->isSetupMode()
                ? tr( "The setup of %1 did not complete successfully.", "@info" )
                : tr( "The installation of %1 did not complete successfully.", "@info" );
        }
        else
        {
            title = Calamares::Settings::instance()->isSetupMode() ? tr( "Setup Complete", "@title" )
                                                                   : tr( "Installation Complete", "@title" );
            message = Calamares::Settings::instance()->isSetupMode()
                ? tr( "The setup of %1 is complete.", "@info" )
                : tr( "The installation of %1 is complete.", "@info" );
        }

        const auto* branding = Calamares::Branding::instance();
        QDBusReply< uint > r = notify.call( "Notify",
                                            QString( "Calamares" ),
                                            QVariant( 0U ),
                                            QString( "calamares" ),
                                            title,
                                            message.arg( branding->versionedName() ),
                                            QStringList(),
                                            QVariantMap(),
                                            QVariant( 0 ) );
        if ( !r.isValid() )
        {
            cWarning() << "Could not call org.freedesktop.Notifications.Notify at end of installation." << r.error();
        }
    }
    else
    {
        cWarning() << "Could not get dbus interface for notifications at end of installation." << notify.lastError();
    }
}
