/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef SSHKEYSELECTION_H
#define SSHKEYSELECTION_H


#include <QWidget>

#include <optional>

class Config;
namespace Ui
{
class SshKeySelectionPage;
}  // namespace Ui


struct ImageRow { const char* name; const char* version; const char* desc; };

class SshKeySelectionPage : public QWidget
{
    Q_OBJECT
public:
    explicit SshKeySelectionPage( Config* config, QWidget* parent = nullptr );
    bool hasSelection() const;


public slots:
    void onInstallationFailed( const QString& message, const QString& details );
    void retranslate();


protected:
    void focusInEvent( QFocusEvent* e ) override;  //choose the child widget to focus
    void scanAvailableKeys();


private:
    Ui::SshKeySelectionPage* ui;
    std::optional< QString > m_failure;
    QStringList m_availableImages;

signals:
    void selectionChanged( bool hasAny );   // NEW
};

#endif  // SSHKEYSELECTION_H
