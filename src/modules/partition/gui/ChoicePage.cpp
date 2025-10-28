/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Collabora Ltd
 *   SPDX-FileCopyrightText: 2021 Anubhav Choudhary <ac.10edu@gmail.com>
 *   SPDX-FileCopyrightText: 2023 Evan James <dalto@fastmail.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */


#include <core/partitiontable.h>
#include <core/device.h>
#include <core/partition.h>
#include <fs/filesystem.h>
#include <fs/ext4.h>
#include <util/globallog.h>
#include "core/PartitionModel.h"
#include "core/PartitionInfo.h"
#include "tests/PartitionJobTests.h"
#include "utils/Units.h"
#include "core/OsproberEntry.h"

#include "ChoicePage.h"

#include "Config.h"

#include "core/BootLoaderModel.h"
#include "core/DeviceModel.h"
#include "core/KPMHelpers.h"
#include "core/OsproberEntry.h"
#include "core/PartUtils.h"
#include "core/PartitionActions.h"
#include "core/PartitionCoreModule.h"
#include "core/PartitionInfo.h"
#include "core/PartitionModel.h"
#include "gui/BootInfoWidget.h"
#include "gui/DeviceInfoWidget.h"
#include "gui/PartitionBarsView.h"
#include "gui/PartitionLabelsView.h"
#include "gui/PartitionSplitterWidget.h"
#include "gui/ScanningDialog.h"

#include "Branding.h"
#include "GlobalStorage.h"
#include "JobQueue.h"
#include "compat/CheckBox.h"
#include "partition/PartitionIterator.h"
#include "partition/PartitionQuery.h"
#include "utils/Gui.h"
#include "utils/Logger.h"
#include "utils/Retranslator.h"
#include "utils/String.h"
#include "utils/Units.h"
#include "widgets/PrettyRadioButton.h"

#include <kpmcore/core/device.h>
#include <kpmcore/core/softwareraid.h>

#include <QBoxLayout>
#include <QButtonGroup>
#include <QComboBox>
#include <QDir>
#include <QFutureWatcher>
#include <QLabel>
#include <QListView>
#include <QtConcurrent/QtConcurrent>

using Calamares::Partition::findPartitionByPath;
using Calamares::Partition::isPartitionFreeSpace;
using Calamares::Partition::PartitionIterator;
using Calamares::Widgets::PrettyRadioButton;
using InstallChoice = Config::InstallChoice;
using SwapChoice = Config::SwapChoice;

/**
 * @brief ChoicePage::ChoicePage is the default constructor. Called on startup as part of
 *      the module loading code path.
 * @param parent the QWidget parent.
 */
ChoicePage::ChoicePage( Config* config, QWidget* parent )
    : QWidget( parent )
    , m_config( config )
    , m_nextEnabled( false )
    , m_core( nullptr )
    , m_isEfi( false )
    , m_grp( nullptr )
    , m_eraseSwapChoiceComboBox( nullptr )
    , m_deviceInfoWidget( nullptr )
    , m_beforePartitionBarsView( nullptr )
    , m_beforePartitionLabelsView( nullptr )
    , m_bootloaderComboBox( nullptr )
{
    setupUi( this );

    auto gs = Calamares::JobQueue::instance()->globalStorage();

    // Set up drives combo
    m_mainLayout->setDirection( QBoxLayout::TopToBottom );
    m_drivesLayout->setDirection( QBoxLayout::LeftToRight );

    BootInfoWidget* bootInfoWidget = new BootInfoWidget( this );
    m_drivesLayout->insertWidget( 0, bootInfoWidget );
    m_drivesLayout->insertSpacing( 1, Calamares::defaultFontHeight() / 2 );

    m_drivesCombo = new QComboBox( this );
    m_mainLayout->setStretchFactor( m_drivesLayout, 0 );
    m_mainLayout->setStretchFactor( m_rightLayout, 1 );
    m_drivesLabel->setBuddy( m_drivesCombo );

    m_drivesLayout->addWidget( m_drivesCombo );

    m_deviceInfoWidget = new DeviceInfoWidget;
    m_drivesLayout->addWidget( m_deviceInfoWidget );
    m_drivesLayout->addStretch();

    m_messageLabel->setWordWrap( true );
    m_messageLabel->hide();

    Calamares::unmarginLayout( m_itemsLayout );

    // Drive selector + preview
    CALAMARES_RETRANSLATE_SLOT( &ChoicePage::retranslate );

    m_previewBeforeFrame->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Expanding );
    m_previewAfterFrame->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Expanding );
    m_previewBeforeLabel->hide();
    m_previewBeforeFrame->hide();
    m_previewAfterLabel->hide();
    m_previewAfterFrame->hide();
    m_reuseHomeCheckBox->hide();
    gs->insert( "reuseHome", false );

    updateNextEnabled();
}

ChoicePage::~ChoicePage() {}

void
ChoicePage::showEvent( QShowEvent* event )
{
    QWidget::showEvent( event );
    // Refresh preview each time the page becomes visible (user may have navigated back).
    if ( m_core )
    {
        updateActionChoicePreview( m_config->installChoice() );
    }
}

void
ChoicePage::retranslate()
{
    retranslateUi( this );
    m_drivesLabel->setText( tr( "Select storage de&vice:", "@label" ) );
    m_previewBeforeLabel->setText( tr( "Current:", "@label" ) );
    m_previewAfterLabel->setText( tr( "After:", "@label" ) );

    updateSwapChoicesTr();
    updateActionDescriptionsTr();
}

/** @brief Sets the @p model for the given @p box and adjusts UI sizes to match.
 *
 * The model provides data for drawing the items in the model; the
 * drawing itself is done by the delegate, which may end up drawing a
 * different width in the popup than in the collapsed combo box.
 *
 * Make the box wide enough to accomodate the whole expanded delegate;
 * this avoids cases where the popup would truncate data being drawn
 * because the overall box is sized too narrow.
 */
void
setModelToComboBox( QComboBox* box, QAbstractItemModel* model )
{
    box->setModel( model );
    if ( model->rowCount() > 0 )
    {
        QStyleOptionViewItem options;
        options.initFrom( box );
        auto delegateSize = box->itemDelegate()->sizeHint( options, model->index( 0, 0 ) );
        box->setMinimumWidth( delegateSize.width() );
    }
}

void
ChoicePage::init( PartitionCoreModule* core )
{
    m_core = core;
    m_isEfi = PartUtils::isEfiSystem();

    setupChoices();

    // We need to do this because a PCM revert invalidates the deviceModel.
    connect( core,
             &PartitionCoreModule::reverted,
             this,
             [ = ]
             {
                 setModelToComboBox( m_drivesCombo, core->deviceModel() );
                 m_drivesCombo->setCurrentIndex( m_lastSelectedDeviceIndex );
             } );
    setModelToComboBox( m_drivesCombo, core->deviceModel() );

    connect( m_drivesCombo, qOverload< int >( &QComboBox::currentIndexChanged ), this, &ChoicePage::applyDeviceChoice );
    connect(
        m_reuseHomeCheckBox, Calamares::checkBoxStateChangedSignal, this, &ChoicePage::onHomeCheckBoxStateChanged );

    ChoicePage::applyDeviceChoice();
}

/** @brief Creates a combobox with the given choices in it.
 *
 * Pre-selects the choice given by @p dflt.
 * No texts are set -- that happens later by the translator functions.
 */
static inline QComboBox*
createCombo( const QSet< SwapChoice >& s, SwapChoice dflt )
{
    QComboBox* box = new QComboBox;
    for ( SwapChoice c : { SwapChoice::NoSwap,
                           SwapChoice::SmallSwap,
                           SwapChoice::FullSwap,
                           SwapChoice::ReuseSwap,
                           SwapChoice::SwapFile } )
    {
        if ( s.contains( c ) )
        {
            box->addItem( QString(), c );
        }
    }

    int dfltIndex = box->findData( dflt );
    if ( dfltIndex >= 0 )
    {
        box->setCurrentIndex( dfltIndex );
    }

    return box;
}

/**
 * @brief ChoicePage::setupChoices creates PrettyRadioButton objects for the action
 *      choices.
 * @warning This must only run ONCE because it creates signal-slot connections for the
 *      actions. When an action is triggered, it runs action-specific code that may
 *      change the internal state of the PCM, and it updates the bottom preview (or
 *      split) widget.
 *      Synchronous loading ends here.
 */
void
ChoicePage::setupChoices()
{
    // sample os-prober output:
    // /dev/sda2:Windows 7 (loader):Windows:chain
    // /dev/sda6::Arch:linux
    //
    // There are three possibilities we have to consider:
    //  - There are no operating systems present
    //  - There is one operating system present
    //  - There are multiple operating systems present
    //
    // There are three outcomes we have to provide:
    //  1) Wipe+autopartition
    //  2) Resize+autopartition
    //  3) Manual
    //  TBD: upgrade option?

    QSize iconSize( Calamares::defaultIconSize().width() * 2, Calamares::defaultIconSize().height() * 2 );
    m_grp = new QButtonGroup( this );

    // Fill up swap options
    if ( m_config->swapChoices().count() > 1 )
    {
        m_eraseSwapChoiceComboBox = createCombo( m_config->swapChoices(), m_config->swapChoice() );
    }

    if ( m_config->eraseFsTypes().count() > 1 )
    {
        m_eraseFsTypesChoiceComboBox = new QComboBox;
        m_eraseFsTypesChoiceComboBox->addItems( m_config->eraseFsTypes() );
        connect(
            m_eraseFsTypesChoiceComboBox, &QComboBox::currentTextChanged, m_config, &Config::setEraseFsTypeChoice );
        connect( m_config, &Config::eraseModeFilesystemChanged, this, &ChoicePage::onActionChanged );

        // Also offer it for "replace
        m_replaceFsTypesChoiceComboBox = new QComboBox;
        m_replaceFsTypesChoiceComboBox->addItems( m_config->eraseFsTypes() );
        connect( m_replaceFsTypesChoiceComboBox,
                 &QComboBox::currentTextChanged,
                 m_config,
                 &Config::setReplaceFilesystemChoice );
        connect( m_config, &Config::replaceModeFilesystemChanged, this, &ChoicePage::onActionChanged );
    }

    m_itemsLayout->addStretch();

    connect( m_grp,
             &QButtonGroup::idToggled,
             this,
             [ this ]( int id, bool checked )
             {
                 if ( checked )  // An action was picked.
                 {
                     m_config->setInstallChoice( id );
                     updateNextEnabled();

                     Q_EMIT actionChosen();
                 }
                 else  // An action was unpicked, either on its own or because of another selection.
                 {
                     if ( m_grp->checkedButton() == nullptr )  // If no other action is chosen, we must
                     {
                         // set m_choice to NoChoice and reset previews.
                         m_config->setInstallChoice( InstallChoice::NoChoice );
                         updateNextEnabled();

                         Q_EMIT actionChosen();
                     }
                 }
             } );

    m_rightLayout->setStretchFactor( m_itemsLayout, 1 );
    m_rightLayout->setStretchFactor( m_previewBeforeFrame, 0 );
    m_rightLayout->setStretchFactor( m_previewAfterFrame, 0 );

    connect( this, &ChoicePage::actionChosen, this, &ChoicePage::onActionChanged );
    if ( m_eraseSwapChoiceComboBox )
    {
        connect( m_eraseSwapChoiceComboBox,
                 QOverload< int >::of( &QComboBox::currentIndexChanged ),
                 this,
                 &ChoicePage::onEraseSwapChoiceChanged );
    }

    updateSwapChoicesTr();
}

/**
 * @brief ChoicePage::selectedDevice queries the device picker (which may be a combo or
 *      a list view) to get a pointer to the currently selected Device.
 * @return a Device pointer, valid in the current state of the PCM, or nullptr if
 *      something goes wrong.
 */
Device*
ChoicePage::selectedDevice()
{
    Device* const currentDevice
        = m_core->deviceModel()->deviceForIndex( m_core->deviceModel()->index( m_drivesCombo->currentIndex() ) );
    return currentDevice;
}

void
ChoicePage::checkInstallChoiceRadioButton( InstallChoice c )
{
    QSignalBlocker b( m_grp );
    m_grp->setExclusive( false );
    m_grp->setExclusive( true );
}

/**
 * @brief ChoicePage::applyDeviceChoice handler for the selected event of the device
 *      picker. Calls ChoicePage::selectedDevice() to get the current Device*, then
 *      updates the preview widget for the on-disk state (calls ChoicePage::
 *      updateDeviceStatePreview()) and finally sets up the available actions and their
 *      text by calling ChoicePage::setupActions().
 */
void
ChoicePage::applyDeviceChoice()
{
    if ( m_core->isDirty() )
    {
        ScanningDialog::run(
            QtConcurrent::run(
                [ = ]
                {
                    QMutexLocker locker( &m_coreMutex );
                    m_core->revertAllDevices();
                } ),
            [ this ] { continueApplyDeviceChoice(); },
            this );
    }
    else
    {
        continueApplyDeviceChoice();
    }
}

void
ChoicePage::continueApplyDeviceChoice()
{
    updateDeviceStatePreview();

    // Preview setup done. Now we show/hide choices as needed.
    setupActions();

    cDebug() << "Previous device" << m_lastSelectedDeviceIndex << "new device" << m_drivesCombo->currentIndex();
    if ( m_lastSelectedDeviceIndex != m_drivesCombo->currentIndex() )
    {
        m_lastSelectedDeviceIndex = m_drivesCombo->currentIndex();
        m_config->setInstallChoice( m_config->initialInstallChoice() );
        checkInstallChoiceRadioButton( m_config->installChoice() );
    }

    Q_EMIT actionChosen();
    Q_EMIT deviceChosen();
}

void
ChoicePage::onActionChanged()
{
    Device* currd = selectedDevice();
    if ( currd )
    {
        applyActionChoice( m_config->installChoice() );
    }

    updateNextEnabled();
}

void
ChoicePage::onEraseSwapChoiceChanged()
{
    if ( m_eraseSwapChoiceComboBox )
    {
        m_config->setSwapChoice( m_eraseSwapChoiceComboBox->currentData().toInt() );
        onActionChanged();
    }
}

void
ChoicePage::applyActionChoice( InstallChoice choice )
{
    cDebug() << "InstallChoice" << choice << Config::installChoiceNames().find( choice );
    m_beforePartitionBarsView->selectionModel()->disconnect( SIGNAL( currentRowChanged( QModelIndex, QModelIndex ) ) );
    auto priorSelection = m_beforePartitionBarsView->selectionModel()->currentIndex();
    m_beforePartitionBarsView->selectionModel()->clearSelection();
    m_beforePartitionBarsView->selectionModel()->clearCurrentIndex();

    switch ( choice )
    {
    case InstallChoice::Erase:
    {
        if ( m_core->isDirty() )
        {
            ScanningDialog::run(
                QtConcurrent::run(
                    [ = ]
                    {
                        QMutexLocker locker( &m_coreMutex );
                        m_core->revertDevice( selectedDevice() );
                    } ),
                [ = ]
                {
                    Q_EMIT deviceChosen();
                },
                this );
        }
        else
        {
            Q_EMIT deviceChosen();
        }
    }
    break;
    case InstallChoice::Replace:
        if ( m_core->isDirty() )
        {
            ScanningDialog::run(
                QtConcurrent::run(
                    [ = ]
                    {
                        QMutexLocker locker( &m_coreMutex );
                        m_core->revertDevice( selectedDevice() );
                    } ),
                [] {},
                this );
        }
        connect( m_beforePartitionBarsView->selectionModel(),
                 &QItemSelectionModel::currentRowChanged,
                 this,
                 &ChoicePage::onPartitionToReplaceSelected,
                 Qt::UniqueConnection );

        // Maintain the selection for replace
        if ( priorSelection.isValid() )
        {
            m_beforePartitionBarsView->selectionModel()->setCurrentIndex( priorSelection, QItemSelectionModel::Select );
        }
        break;

    case InstallChoice::Alongside:
        if ( m_core->isDirty() )
        {
            ScanningDialog::run(
                QtConcurrent::run(
                    [ = ]
                    {
                        QMutexLocker locker( &m_coreMutex );
                        m_core->revertDevice( selectedDevice() );
                    } ),
                [ this ]
                {
                    // We need to reupdate after reverting because the splitter widget is
                    // not a true view.
                    updateActionChoicePreview( m_config->installChoice() );
                    updateNextEnabled();
                },
                this );
        }

        connect( m_beforePartitionBarsView->selectionModel(),
                 &QItemSelectionModel::currentRowChanged,
                 this,
                 &ChoicePage::doAlongsideSetupSplitter,
                 Qt::UniqueConnection );
        break;
    case InstallChoice::Manual:
        if ( m_core->isDirty() )
        {
            ScanningDialog::run(
                QtConcurrent::run(
                    [ = ]
                    {
                        QMutexLocker locker( &m_coreMutex );
                        m_core->revertDevice( selectedDevice() );
                    } ),
                [] {},
                this );
        }
        break;
    case InstallChoice::NoChoice:
        break;
    }
    updateNextEnabled();
    updateActionChoicePreview( choice );
}

void
ChoicePage::doAlongsideSetupSplitter( const QModelIndex& current, const QModelIndex& previous )
{
    Q_UNUSED( previous )
    if ( !current.isValid() )
    {
        return;
    }

    if ( !m_afterPartitionSplitterWidget )
    {
        return;
    }

    const PartitionModel* modl = qobject_cast< const PartitionModel* >( current.model() );
    if ( !modl )
    {
        return;
    }

    Partition* part = modl->partitionForIndex( current );
    if ( !part )
    {
        cDebug() << "Partition not found for index" << current;
        return;
    }

    double requiredStorageGB
        = Calamares::JobQueue::instance()->globalStorage()->value( "requiredStorageGiB" ).toDouble();

    qint64 requiredStorageB = Calamares::GiBtoBytes( requiredStorageGB + 0.1 + 2.0 );

    m_afterPartitionSplitterWidget->setSplitPartition( part->partitionPath(),
                                                       qRound64( part->used() * 1.1 ),
                                                       part->capacity() - requiredStorageB,
                                                       part->capacity() / 2 );

    if ( m_isEfi )
    {
        setupEfiSystemPartitionSelector();
    }

    cDebug() << "Partition selected for Alongside.";

    updateNextEnabled();
}

void
ChoicePage::onHomeCheckBoxStateChanged()
{
    if ( m_config->installChoice() == InstallChoice::Replace
         && m_beforePartitionBarsView->selectionModel()->currentIndex().isValid() )
    {
        doReplaceSelectedPartition( m_beforePartitionBarsView->selectionModel()->currentIndex() );
    }
}

void
ChoicePage::onLeave()
{
    Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();
    gs->insert( "selectedDisk", selectedDevice() ? selectedDevice()->deviceNode() : QString() );

    if ( m_config->installChoice() == InstallChoice::Alongside )
    {
        if ( m_afterPartitionSplitterWidget->splitPartitionSize() >= 0
             && m_afterPartitionSplitterWidget->newPartitionSize() >= 0 )
        {
            doAlongsideApply();
        }
    }

    if ( m_isEfi
         && ( m_config->installChoice() == InstallChoice::Alongside
              || m_config->installChoice() == InstallChoice::Replace ) )
    {
        QList< Partition* > efiSystemPartitions = m_core->efiSystemPartitions();
        if ( efiSystemPartitions.count() == 1 )
        {
            PartitionInfo::setMountPoint(
                efiSystemPartitions.first(),
                Calamares::JobQueue::instance()->globalStorage()->value( "efiSystemPartition" ).toString() );
        }
        else if ( efiSystemPartitions.count() > 1 && m_efiComboBox )
        {
            PartitionInfo::setMountPoint(
                efiSystemPartitions.at( m_efiComboBox->currentIndex() ),
                Calamares::JobQueue::instance()->globalStorage()->value( "efiSystemPartition" ).toString() );
        }
        else
        {
            cError() << "cannot set up EFI system partition.\nESP count:" << efiSystemPartitions.count()
                     << "\nm_efiComboBox:" << m_efiComboBox;
        }
    }
    else  // installPath is then passed to the bootloader module for MBR setup
    {
        if ( !m_isEfi )
        {
            if ( m_bootloaderComboBox.isNull() )
            {
                auto d_p = selectedDevice();
                if ( d_p )
                {
                    m_core->setBootLoaderInstallPath( d_p->deviceNode() );
                }
                else
                {
                    cWarning() << "No device selected for bootloader.";
                }
            }
            else
            {
                QVariant var = m_bootloaderComboBox->currentData( BootLoaderModel::BootLoaderPathRole );
                if ( !var.isValid() )
                {
                    return;
                }
                m_core->setBootLoaderInstallPath( var.toString() );
            }
        }
    }
}

void
ChoicePage::doAlongsideApply()
{
    Q_ASSERT( m_afterPartitionSplitterWidget->splitPartitionSize() >= 0 );
    Q_ASSERT( m_afterPartitionSplitterWidget->newPartitionSize() >= 0 );

    QMutexLocker locker( &m_coreMutex );

    QString path = m_beforePartitionBarsView->selectionModel()
                       ->currentIndex()
                       .data( PartitionModel::PartitionPathRole )
                       .toString();

    DeviceModel* dm = m_core->deviceModel();
    for ( int i = 0; i < dm->rowCount(); ++i )
    {
        Device* dev = dm->deviceForIndex( dm->index( i ) );
        Partition* candidate = findPartitionByPath( { dev }, path );
        if ( candidate )
        {
            qint64 firstSector = candidate->firstSector();
            qint64 newLastSector
                = firstSector + m_afterPartitionSplitterWidget->splitPartitionSize() / dev->logicalSize();

            m_core->resizePartition( dev, candidate, firstSector, newLastSector );
            m_core->dumpQueue();

            break;
        }
    }
}

void
ChoicePage::onPartitionToReplaceSelected( const QModelIndex& current, const QModelIndex& previous )
{
    Q_UNUSED( previous )
    if ( !current.isValid() )
    {
        return;
    }

    // Reset state on selection regardless of whether this will be used.
    m_reuseHomeCheckBox->setChecked( false );

    doReplaceSelectedPartition( current );
}

void
ChoicePage::doReplaceSelectedPartition( const QModelIndex& current )
{
    if ( !current.isValid() )
    {
        return;
    }

    // This will be deleted by the second lambda, below.
    QString* homePartitionPath = new QString();

    ScanningDialog::run(
        QtConcurrent::run(
            [ this, current, homePartitionPath ]( bool doReuseHomePartition )
            {
                QMutexLocker locker( &m_coreMutex );

                if ( m_core->isDirty() )
                {
                    m_core->revertDevice( selectedDevice() );
                }

                // if the partition is unallocated(free space), we don't replace it but create new one
                // with the same first and last sector
                Partition* selectedPartition
                    = static_cast< Partition* >( current.data( PartitionModel::PartitionPtrRole ).value< void* >() );
                if ( isPartitionFreeSpace( selectedPartition ) )
                {
                    //NOTE: if the selected partition is free space, we don't deal with
                    //      a separate /home partition at all because there's no existing
                    //      rootfs to read it from.
                    PartitionRole newRoles = PartitionRole( PartitionRole::Primary );

                    if ( selectedPartition->parent() )
                    {
                        Partition* parent = dynamic_cast< Partition* >( selectedPartition->parent() );
                        if ( parent && parent->roles().has( PartitionRole::Extended ) )
                        {
                            newRoles = PartitionRole( PartitionRole::Logical );
                        }
                    }
                }
                else
                {
                    // We can't use the PartitionPtrRole because we need to make changes to the
                    // main DeviceModel, not the immutable copy.
                    QString partPath = current.data( PartitionModel::PartitionPathRole ).toString();
                    selectedPartition = findPartitionByPath( { selectedDevice() }, partPath );
                    if ( selectedPartition )
                    {
                        // Find out is the selected partition has a rootfs. If yes, then make the
                        // m_reuseHomeCheckBox visible and set its text to something meaningful.
                        homePartitionPath->clear();
                        for ( const OsproberEntry& osproberEntry : m_core->osproberEntries() )
                        {
                            if ( osproberEntry.path == partPath )
                            {
                                *homePartitionPath = osproberEntry.homePath;
                            }
                        }
                        if ( homePartitionPath->isEmpty() )
                        {
                            doReuseHomePartition = false;
                        }

                        Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();

                        Partition* homePartition = findPartitionByPath( { selectedDevice() }, *homePartitionPath );

                        if ( homePartition && doReuseHomePartition )
                        {
                            PartitionInfo::setMountPoint( homePartition, "/home" );
                            gs->insert( "reuseHome", true );
                        }
                        else
                        {
                            gs->insert( "reuseHome", false );
                        }
                    }
                }
            },
            m_reuseHomeCheckBox->isChecked() ),
        [ this, homePartitionPath ]
        {
            m_reuseHomeCheckBox->setVisible( !homePartitionPath->isEmpty() );
            if ( !homePartitionPath->isEmpty() )
            {
                m_reuseHomeCheckBox->setText( tr( "Reuse %1 as home partition for %2", "@label" )
                                                  .arg( *homePartitionPath )
                                                  .arg( Calamares::Branding::instance()->shortProductName() ) );
            }
            delete homePartitionPath;

            if ( m_isEfi )
            {
                setupEfiSystemPartitionSelector();
            }

            updateNextEnabled();
            if ( !m_bootloaderComboBox.isNull() && m_bootloaderComboBox->currentIndex() < 0 )
            {
                m_bootloaderComboBox->setCurrentIndex( m_lastSelectedDeviceIndex );
            }
        },
        this );
}

/**
 * From src/modules/partition/tests/PartitionJobTests.cpp
 */
static ::Partition*
firstFreePartition( PartitionNode* parent )
{
    for ( auto child : parent->children() )
    {
        if ( Calamares::Partition::isPartitionFreeSpace( child ) )
        {
            return child;
        }
    }
    return nullptr;
}

/**
 * @brief clear and then rebuild the contents of the preview widget
 *
 * The preview widget for the current disk is completely re-constructed
 * based on the on-disk state. This also triggers a rescan in the
 * PCM to get a Device* copy that's unaffected by subsequent PCM changes.
 */
void
ChoicePage::updateDeviceStatePreview()
{
    //FIXME: this needs to be made async because the rescan can block the UI thread for
    //       a while. --Teo 10/2015
    Device* currentDevice = selectedDevice();
    Q_ASSERT( currentDevice );
    QMutexLocker locker( &m_previewsMutex );

    cDebug() << "Updating partitioning state widgets.";
    qDeleteAll( m_previewBeforeFrame->children() );

    auto layout = m_previewBeforeFrame->layout();
    if ( layout )
    {
        layout->deleteLater();  // Doesn't like nullptr
    }

    layout = new QVBoxLayout;
    m_previewBeforeFrame->setLayout( layout );
    Calamares::unmarginLayout( layout );
    layout->setSpacing( 6 );

    PartitionBarsView::NestedPartitionsMode mode
        = Calamares::JobQueue::instance()->globalStorage()->value( "drawNestedPartitions" ).toBool()
        ? PartitionBarsView::DrawNestedPartitions
        : PartitionBarsView::NoNestedPartitions;
    m_beforePartitionBarsView = new PartitionBarsView( m_previewBeforeFrame );
    m_beforePartitionBarsView->setNestedPartitionsMode( mode );
    m_beforePartitionLabelsView = new PartitionLabelsView( m_previewBeforeFrame );
    m_beforePartitionLabelsView->setExtendedPartitionHidden( mode == PartitionBarsView::NoNestedPartitions );

    Device* deviceBefore = m_core->immutableDeviceCopy( currentDevice );

    PartitionModel* model = new PartitionModel( m_beforePartitionBarsView );
    model->init( deviceBefore, m_core->osproberEntries() );

    m_beforePartitionBarsView->setModel( model );
    m_beforePartitionLabelsView->setModel( model );

    // Make the bars and labels view use the same selectionModel.
    auto sm = m_beforePartitionLabelsView->selectionModel();
    m_beforePartitionLabelsView->setSelectionModel( m_beforePartitionBarsView->selectionModel() );
    if ( sm )
    {
        sm->deleteLater();
    }

    switch ( m_config->installChoice() )
    {
    case InstallChoice::Replace:
    case InstallChoice::Alongside:
        m_beforePartitionBarsView->setSelectionMode( QAbstractItemView::SingleSelection );
        m_beforePartitionLabelsView->setSelectionMode( QAbstractItemView::SingleSelection );
        break;
    case InstallChoice::NoChoice:
    case InstallChoice::Erase:
    case InstallChoice::Manual:
        m_beforePartitionBarsView->setSelectionMode( QAbstractItemView::NoSelection );
        m_beforePartitionLabelsView->setSelectionMode( QAbstractItemView::NoSelection );
    }

    layout->addWidget( m_beforePartitionBarsView );
    layout->addWidget( m_beforePartitionLabelsView );
}

/**
 * @brief rebuild the contents of the preview for the PCM-proposed state.
 *
 * No rescans here, this should be immediate.
 *
 * @param choice the chosen partitioning action.
 */
void
ChoicePage::updateActionChoicePreview( InstallChoice choice )
{
    cDebug() << "Updating partitioning action choice preview.";
    Device* currentDevice = selectedDevice();
    Q_ASSERT( currentDevice );

    QMutexLocker locker( &m_previewsMutex );

    cDebug() << "Updating partitioning preview widgets.";
    qDeleteAll( m_previewAfterFrame->children() );

    auto oldlayout = m_previewAfterFrame->layout();
    if ( oldlayout )
    {
        oldlayout->deleteLater();
    }

    QVBoxLayout* layout = new QVBoxLayout;
    m_previewAfterFrame->setLayout( layout );
    Calamares::unmarginLayout( layout );
    layout->setSpacing( 6 );

    PartitionBarsView::NestedPartitionsMode mode
        = Calamares::JobQueue::instance()->globalStorage()->value( "drawNestedPartitions" ).toBool()
        ? PartitionBarsView::DrawNestedPartitions
        : PartitionBarsView::NoNestedPartitions;

    m_reuseHomeCheckBox->hide();
    Calamares::JobQueue::instance()->globalStorage()->insert( "reuseHome", false );

    switch ( choice )
    {
    case InstallChoice::Alongside:
    {
        m_previewBeforeLabel->setText( tr( "Current:", "@label" ) );
        m_selectLabel->setText( tr( "<strong>Select a partition to shrink, "
                                    "then drag the bottom bar to resize</strong>" ) );

        m_afterPartitionSplitterWidget = new PartitionSplitterWidget( m_previewAfterFrame );
        m_afterPartitionSplitterWidget->init( selectedDevice(), mode == PartitionBarsView::DrawNestedPartitions );
        layout->addWidget( m_afterPartitionSplitterWidget );

        QLabel* sizeLabel = new QLabel( m_previewAfterFrame );
        layout->addWidget( sizeLabel );
        sizeLabel->setWordWrap( true );

        if ( !m_isEfi )
        {
            layout->addWidget( createBootloaderPanel() );
        }

        connect( m_afterPartitionSplitterWidget,
                 &PartitionSplitterWidget::partitionResized,
                 this,
                 [ this, sizeLabel ]( const QString& path, qint64 size, qint64 sizeNext )
                 {
                     Q_UNUSED( path )
                     sizeLabel->setText(
                         tr( "%1 will be shrunk to %2MiB and a new "
                             "%3MiB partition will be created for %4.",
                             "@info, %1 is partition name, %4 is product name" )
                             .arg( m_beforePartitionBarsView->selectionModel()->currentIndex().data().toString() )
                             .arg( Calamares::BytesToMiB( size ) )
                             .arg( Calamares::BytesToMiB( sizeNext ) )
                             .arg( Calamares::Branding::instance()->shortProductName() ) );
                 } );

        SelectionFilter filter = []( const QModelIndex& index )
        {
            return PartUtils::canBeResized(
                static_cast< Partition* >( index.data( PartitionModel::PartitionPtrRole ).value< void* >() ),
                Logger::Once() );
        };
        m_beforePartitionBarsView->setSelectionFilter( filter );
        m_beforePartitionLabelsView->setSelectionFilter( filter );

        break;
    }
    case InstallChoice::Erase:
    case InstallChoice::Replace:
    {
        Device* targetDevice = selectedDevice();

        qint64 logicalSize = 2048; // SEAPATH default logical sector size
        const PartitionRole role( PartitionRole::Primary );

        Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();
        auto gptPartitions = gs->value( "imageselection.gptPartitions" );

        // Prevent accumulation of preview partitions if user switches
        // back and forth between choices.
        if ( m_core->isDirty() )
        {
            cDebug() << "Device is dirty, reverting to clean state before adding preview partitions";
            m_core->revertDevice( targetDevice );
            targetDevice = selectedDevice();  // Refresh the targetDevice reference
        }

        // Remove directly the partition table without creating jobs (preview only)
        cDebug() << "Removing all existing partitions from preview (no jobs created)";
        QList< Partition* > partitionsToDelete;
        for ( auto* child : targetDevice->partitionTable()->children() )
        {
            Partition* p = dynamic_cast< Partition* >( child );
            if ( p && !isPartitionFreeSpace( p ) )
            {
                partitionsToDelete.append( p );
            }
        }

        // Directly manipulate the partition table for preview (mimics previewCreatePartition)
        targetDevice->partitionTable()->removeUnallocated();
        for ( auto* p : partitionsToDelete )
        {
            cDebug() << "Removing existing partition" << p->partitionPath() << "from preview";
            p->parent()->remove( p );
            delete p;
        }
        targetDevice->partitionTable()->updateUnallocated( *targetDevice );

        for (const auto& partition : gptPartitions.toList())
        {
            auto partitionMap = partition.toMap();
            auto partitionName = partitionMap.value("name").toString();
            auto partitionFirstLBA = partitionMap.value("first_lba").toInt();
            auto partitionLastLBA = partitionMap.value("last_lba").toInt();
            auto partitionSize = partitionMap.value("size_sectors").toInt();

            // For preview purposes, we need a parent node to attach the partition to
            // First try to find free space, otherwise use the partition table itself
            Partition* freePartition = firstFreePartition( targetDevice->partitionTable() );
            PartitionNode* parentNode = freePartition ? static_cast<PartitionNode*>( freePartition->parent() )
                                                      : static_cast<PartitionNode*>( targetDevice->partitionTable() );

            if ( !parentNode )
            {
                cError() << "No parent node (partition table) found for preview partition" << partitionName;
                exit( EXIT_FAILURE );
            }

            // Default to ext4, currently no support for FS preview
            FileSystem* fs = FileSystemFactory::create( FileSystem::Ext4, partitionFirstLBA, partitionLastLBA, logicalSize );
            Partition* previewPartition = new Partition( parentNode,
                                                            *targetDevice,
                                                            role,
                                                            fs,
                                                            partitionFirstLBA,
                                                            partitionLastLBA,
                                                            QString(),
                                                            KPM_PARTITION_FLAG( None ),
                                                            QString(),
                                                            false,
                                                            KPM_PARTITION_FLAG( None ),
                                                            KPM_PARTITION_STATE( New ) );

            cDebug() << "Created preview partition object for" << partitionName;
            PartitionInfo::setFormat( previewPartition, true );
            PartitionInfo::setMountPoint( previewPartition,  partitionName );
            // Insert into preview without creating jobs
            m_core->previewCreatePartition( targetDevice, previewPartition );
        }
        m_previewBeforeLabel->setText( tr( "Current:", "@label" ) );
        m_afterPartitionBarsView = new PartitionBarsView( m_previewAfterFrame );
        m_afterPartitionBarsView->setNestedPartitionsMode( mode );
        m_afterPartitionLabelsView = new PartitionLabelsView( m_previewAfterFrame );
        m_afterPartitionLabelsView->setExtendedPartitionHidden( mode == PartitionBarsView::NoNestedPartitions );
        m_afterPartitionLabelsView->setCustomNewRootLabel(
            Calamares::Branding::instance()->string( Calamares::Branding::BootloaderEntryName ) );

        PartitionModel* model = m_core->partitionModelForDevice( selectedDevice() );

        // The QObject parents tree is meaningful for memory management here,
        // see qDeleteAll above.
        m_afterPartitionBarsView->setModel( model );
        m_afterPartitionLabelsView->setModel( model );
        m_afterPartitionBarsView->setSelectionMode( QAbstractItemView::NoSelection );
        m_afterPartitionLabelsView->setSelectionMode( QAbstractItemView::NoSelection );

        layout->addWidget( m_afterPartitionBarsView );
        layout->addWidget( m_afterPartitionLabelsView );

        if ( !m_isEfi )
        {
            layout->addWidget( createBootloaderPanel() );
        }

        m_previewBeforeLabel->show();
        m_previewBeforeFrame->show();
        m_previewAfterFrame->show();
        m_previewAfterLabel->show();

        if ( m_config->installChoice() == InstallChoice::Erase )
        {
            m_selectLabel->hide();
        }
        else
        {
            SelectionFilter filter = []( const QModelIndex& index )
            {
                return PartUtils::canBeReplaced(
                    static_cast< Partition* >( index.data( PartitionModel::PartitionPtrRole ).value< void* >() ),
                    Logger::Once() );
            };
            m_beforePartitionBarsView->setSelectionFilter( filter );
            m_beforePartitionLabelsView->setSelectionFilter( filter );
        }

        break;
    }
    case InstallChoice::NoChoice:
    case InstallChoice::Manual:
        m_selectLabel->hide();
        m_previewAfterFrame->hide();
        m_previewBeforeLabel->setText( tr( "Current:", "@label" ) );
        m_previewAfterLabel->hide();
        break;
    }

    if ( m_isEfi
         && ( m_config->installChoice() == InstallChoice::Alongside
              || m_config->installChoice() == InstallChoice::Replace ) )
    {
        QHBoxLayout* efiLayout = new QHBoxLayout;
        layout->addLayout( efiLayout );
        m_efiLabel = new QLabel( m_previewAfterFrame );
        efiLayout->addWidget( m_efiLabel );
        m_efiComboBox = new QComboBox( m_previewAfterFrame );
        efiLayout->addWidget( m_efiComboBox );
        m_efiLabel->setBuddy( m_efiComboBox );
        m_efiComboBox->hide();
        efiLayout->addStretch();
    }

    // Also handle selection behavior on beforeFrame.
    QAbstractItemView::SelectionMode previewSelectionMode = QAbstractItemView::NoSelection;
    switch ( m_config->installChoice() )
    {
    case InstallChoice::Replace:
    case InstallChoice::Alongside:
        previewSelectionMode = QAbstractItemView::SingleSelection;
        break;
    case InstallChoice::NoChoice:
    case InstallChoice::Erase:
    case InstallChoice::Manual:
        previewSelectionMode = QAbstractItemView::NoSelection;
    }

    m_beforePartitionBarsView->setSelectionMode( previewSelectionMode );
    m_beforePartitionLabelsView->setSelectionMode( previewSelectionMode );

    updateNextEnabled();
}

void
ChoicePage::setupEfiSystemPartitionSelector()
{
    Q_ASSERT( m_isEfi );

    // Only the already existing ones:
    QList< Partition* > efiSystemPartitions = m_core->efiSystemPartitions();

    if ( efiSystemPartitions.count() == 0 )  //should never happen
    {
        m_efiLabel->setText( tr( "An EFI system partition cannot be found anywhere "
                                 "on this system. Please go back and use manual "
                                 "partitioning to set up %1.",
                                 "@info, %1 is product name" )
                                 .arg( Calamares::Branding::instance()->shortProductName() ) );
        updateNextEnabled();
    }
    else if ( efiSystemPartitions.count() == 1 )  //probably most usual situation
    {
        m_efiLabel->setText( tr( "The EFI system partition at %1 will be used for "
                                 "starting %2.",
                                 "@info, %1 is partition path, %2 is product name" )
                                 .arg( efiSystemPartitions.first()->partitionPath() )
                                 .arg( Calamares::Branding::instance()->shortProductName() ) );
    }
    else
    {
        m_efiLabel->setText( tr( "EFI system partition:", "@label" ) );
        for ( int i = 0; i < efiSystemPartitions.count(); ++i )
        {
            Partition* efiPartition = efiSystemPartitions.at( i );
            m_efiComboBox->addItem( efiPartition->partitionPath(), i );

            // We pick an ESP on the currently selected device, if possible
            if ( efiPartition->devicePath() == selectedDevice()->deviceNode() && efiPartition->number() == 1 )
            {
                m_efiComboBox->setCurrentIndex( i );
            }
        }
    }
}

static inline void
force_uncheck( QButtonGroup* grp, PrettyRadioButton* button )
{
    button->hide();
    grp->setExclusive( false );
    button->setChecked( false );
    grp->setExclusive( true );
}

static inline QDebug&
operator<<( QDebug& s, PartitionIterator& it )
{
    s << ( ( *it ) ? ( *it )->deviceNode() : QString( "<null device>" ) );
    return s;
}

QString
describePartitionTypes( const QStringList& types )
{
    if ( types.empty() )
    {
        return QCoreApplication::translate(
            ChoicePage::staticMetaObject.className(), "any", "any partition-table type" );
    }
    if ( types.size() == 1 )
    {
        return types.first();
    }
    if ( types.size() == 2 )
    {
        return QCoreApplication::translate(
                   ChoicePage::staticMetaObject.className(), "%1 or %2", "partition-table types" )
            .arg( types.at( 0 ), types.at( 1 ) );
    }
    // More than two, rather unlikely
    return types.join( ", " );
}

/**
 * @brief ChoicePage::setupActions happens every time a new Device* is selected in the
 *      device picker. Sets up the text and visibility of the partitioning actions based
 *      on the currently selected Device*, bootloader and os-prober output.
 */
void
ChoicePage::setupActions()
{
    Logger::Once o;

    Device* currentDevice = selectedDevice();
    OsproberEntryList osproberEntriesForCurrentDevice = getOsproberEntriesForDevice( currentDevice );

    cDebug() << o << "Setting up actions for" << currentDevice->deviceNode() << "with"
             << osproberEntriesForCurrentDevice.count() << "entries.";

    if ( currentDevice->partitionTable() )
    {
        m_deviceInfoWidget->setPartitionTableType( currentDevice->partitionTable()->type() );
    }
    else
    {
        m_deviceInfoWidget->setPartitionTableType( PartitionTable::unknownTableType );
    }

    bool matchTableType = false;

    PartitionTable::TableType tableType = PartitionTable::unknownTableType;
    if ( currentDevice->partitionTable() )
    {
        tableType = currentDevice->partitionTable()->type();
        matchTableType = m_config->acceptPartitionTableType( tableType );
    }

    m_osproberEntriesCount = osproberEntriesForCurrentDevice.count();
    if ( m_osproberEntriesCount == 0 )
    {
        m_osproberOneEntryName.clear();
        m_grp->setExclusive( false );
        m_grp->setExclusive( true );
    }
    else if ( m_osproberEntriesCount == 1 )
    {
        m_osproberOneEntryName = osproberEntriesForCurrentDevice.first().prettyName;
    }
    else
    {
        // osproberEntriesForCurrentDevice has at least 2 items.
        m_osproberOneEntryName.clear();
    }
    updateActionDescriptionsTr();

    bool isEfi = PartUtils::isEfiSystem();
    bool efiSystemPartitionFound = !m_core->efiSystemPartitions().isEmpty();

    if ( isEfi && !efiSystemPartitionFound )
    {
        cWarning() << "System is EFI but there's no EFI system partition, "
                      "DISABLING alongside and replace features.";
    }

    if ( tableType != PartitionTable::unknownTableType && !matchTableType )
    {
        m_messageLabel->setText( tr( "This storage device already has an operating system on it, "
                                     "but the partition table <strong>%1</strong> is different from the "
                                     "needed <strong>%2</strong>.<br/>" )
                                     .arg( PartitionTable::tableTypeToName( tableType ) )
                                     .arg( describePartitionTypes( m_config->partitionTableTypes() ) ) );
    }
}

OsproberEntryList
ChoicePage::getOsproberEntriesForDevice( Device* device ) const
{
    OsproberEntryList eList;
    for ( const OsproberEntry& entry : m_core->osproberEntries() )
    {
        if ( entry.path.startsWith( device->deviceNode() ) )
        {
            eList.append( entry );
        }
    }
    return eList;
}

bool
ChoicePage::isNextEnabled() const
{
    return m_nextEnabled;
}

bool
ChoicePage::calculateNextEnabled() const
{
    auto sm_p = m_beforePartitionBarsView ? m_beforePartitionBarsView->selectionModel() : nullptr;

    switch ( m_config->installChoice() )
    {
    case InstallChoice::NoChoice:
        cDebug() << "No partitioning choice has been made yet";
        return false;
    case InstallChoice::Replace:
    case InstallChoice::Alongside:
        if ( !( sm_p && sm_p->currentIndex().isValid() ) )
        {
            cDebug() << "No partition selected for alongside or replace";
            return false;
        }
        break;
    case InstallChoice::Erase:
    case InstallChoice::Manual:
        // Nothing to check for these
        break;
    }

    if ( m_isEfi
         && ( m_config->installChoice() == InstallChoice::Alongside
              || m_config->installChoice() == InstallChoice::Replace ) )
    {
        if ( m_core->efiSystemPartitions().count() == 0 )
        {
            cDebug() << "No EFI partition for alongside or replace";
            return false;
        }
    }

    return true;
}

void
ChoicePage::updateNextEnabled()
{
    bool enabled = calculateNextEnabled();

    if ( enabled != m_nextEnabled )
    {
        m_nextEnabled = enabled;
        Q_EMIT nextStatusChanged( enabled );
    }
}

void
ChoicePage::updateSwapChoicesTr()
{
    if ( !m_eraseSwapChoiceComboBox )
    {
        return;
    }

    static_assert( SwapChoice::NoSwap == 0, "Enum values out-of-sync" );
    for ( int index = 0; index < m_eraseSwapChoiceComboBox->count(); ++index )
    {
        bool ok = false;
        int value = 0;

        switch ( value = m_eraseSwapChoiceComboBox->itemData( index ).toInt( &ok ) )
        {
        // case 0:
        case SwapChoice::NoSwap:
            // toInt() returns 0 on failure, so check for ok
            if ( ok )  // It was explicitly set to 0
            {
                m_eraseSwapChoiceComboBox->setItemText( index, tr( "No swap", "@label" ) );
            }
            else
            {
                cWarning() << "Box item" << index << m_eraseSwapChoiceComboBox->itemText( index )
                           << "has non-integer role.";
            }
            break;
        case SwapChoice::ReuseSwap:
            m_eraseSwapChoiceComboBox->setItemText( index, tr( "Reuse swap", "@label" ) );
            break;
        case SwapChoice::SmallSwap:
            m_eraseSwapChoiceComboBox->setItemText( index, tr( "Swap (no Hibernate)", "@label" ) );
            break;
        case SwapChoice::FullSwap:
            m_eraseSwapChoiceComboBox->setItemText( index, tr( "Swap (with Hibernate)", "@label" ) );
            break;
        case SwapChoice::SwapFile:
            m_eraseSwapChoiceComboBox->setItemText( index, tr( "Swap to file", "@label" ) );
            break;
        default:
            cWarning() << "Box item" << index << m_eraseSwapChoiceComboBox->itemText( index ) << "has role" << value;
        }
    }
}

int
ChoicePage::lastSelectedDeviceIndex()
{
    return m_lastSelectedDeviceIndex;
}

void
ChoicePage::setLastSelectedDeviceIndex( int index )
{
    m_lastSelectedDeviceIndex = index;
    m_drivesCombo->setCurrentIndex( m_lastSelectedDeviceIndex );
}

QWidget*
ChoicePage::createBootloaderPanel()
{
    QWidget* panelWidget = new QWidget;

    QHBoxLayout* mainLayout = new QHBoxLayout;
    panelWidget->setLayout( mainLayout );
    mainLayout->setContentsMargins( 0, 0, 0, 0 );
    QLabel* widgetLabel = new QLabel( panelWidget );
    mainLayout->addWidget( widgetLabel );
    widgetLabel->setText( tr( "Bootloader location:", "@label" ) );

    QComboBox* comboForBootloader = new QComboBox( panelWidget );
    comboForBootloader->setModel( m_core->bootLoaderModel() );

    // When the chosen bootloader device changes, we update the choice in the PCM
    connect( comboForBootloader,
             QOverload< int >::of( &QComboBox::currentIndexChanged ),
             this,
             [ this ]( int newIndex )
             {
                 QComboBox* bootloaderCombo = qobject_cast< QComboBox* >( sender() );
                 if ( bootloaderCombo )
                 {
                     QVariant var = bootloaderCombo->itemData( newIndex, BootLoaderModel::BootLoaderPathRole );
                     if ( !var.isValid() )
                     {
                         return;
                     }
                     m_core->setBootLoaderInstallPath( var.toString() );
                 }
             } );
    m_bootloaderComboBox = comboForBootloader;

    connect( m_core->bootLoaderModel(),
             &QAbstractItemModel::modelReset,
             [ this ]()
             {
                 if ( !m_bootloaderComboBox.isNull() )
                 {
                     Calamares::restoreSelectedBootLoader( *m_bootloaderComboBox, m_core->bootLoaderInstallPath() );
                 }
             } );
    connect(
        m_core,
        &PartitionCoreModule::deviceReverted,
        this,
        [ this ]( Device* )
        {
            if ( !m_bootloaderComboBox.isNull() )
            {
                if ( m_bootloaderComboBox->model() != m_core->bootLoaderModel() )
                {
                    m_bootloaderComboBox->setModel( m_core->bootLoaderModel() );
                }

                m_bootloaderComboBox->setCurrentIndex( m_lastSelectedDeviceIndex );
            }
        },
        Qt::QueuedConnection );
    // ^ Must be Queued so it's sure to run when the widget is already visible.

    mainLayout->addWidget( m_bootloaderComboBox );
    widgetLabel->setBuddy( m_bootloaderComboBox );
    mainLayout->addStretch();

    return panelWidget;
}

void
ChoicePage::updateActionDescriptionsTr()
{
    if ( m_osproberEntriesCount == 0 )
    {
        cDebug() << "Setting texts for 0 osprober entries";
        m_messageLabel->setText( tr( "This storage device does not seem to have an operating system on it. "
                                     "What would you like to do?<br/>"
                                     "You will be able to review and confirm your choices "
                                     "before any change is made to the storage device." ) );

    }
    if ( m_osproberEntriesCount == 1 )
    {
        if ( !m_osproberOneEntryName.isEmpty() )
        {
            cDebug() << "Setting texts for 1 non-empty osprober entry";
            m_messageLabel->setText( tr( "This storage device has %1 on it. "
                                         "What would you like to do?<br/>"
                                         "You will be able to review and confirm your choices "
                                         "before any change is made to the storage device." )
                                         .arg( m_osproberOneEntryName ) );
        }
        else
        {
            cDebug() << "Setting texts for 1 empty osprober entry";
            m_messageLabel->setText( tr( "This storage device already has an operating system on it. "
                                         "What would you like to do?<br/>"
                                         "You will be able to review and confirm your choices "
                                         "before any change is made to the storage device." ) );
        }
    }
    if ( m_osproberEntriesCount >= 2 )
    {
        cDebug() << "Setting texts for >= 2 osprober entries";

        m_messageLabel->setText( tr( "This storage device has multiple operating systems on it. "
                                     "What would you like to do?<br/>"
                                     "You will be able to review and confirm your choices "
                                     "before any change is made to the storage device." ) );
    }
    if ( m_osproberEntriesCount < 0 )
    {
        cWarning() << "Invalid osprober count, labels and buttons not updated.";
    }
}
