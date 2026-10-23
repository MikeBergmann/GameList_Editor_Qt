#include "F_Main.h"

#include "F_About.h"
#include "F_ConfigureNetwork.h"
#include "F_Help.h"
#include "F_MoreInfos.h"
#include "GameEditPanel.h"
#include "GamelistFilter.h"
#include "GamelistModel.h"
#include "PathUtil.h"
#include "Resources.h"
#include "ScrapePanel.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>

#include <QScopedValueRollback>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

// The category of the combo's current item. It is stored as the item's data rather
// than read from the position, so reordering the list cannot change what an item means.
// No selection (an empty combo) reads as All.
FilterCategory currentCategory( const QComboBox* aFilterCombo )
{
   const QVariant data = aFilterCombo->currentData();
   return data.isValid() ? static_cast<FilterCategory>( data.toInt() ) : FilterCategory::All;
}

// A submenu of placeholder entries with no handler yet, shown disabled. An empty item is a
// separator.
QMenu* addPendingMenu( QMenu* aParent, const QString& aTitle, const QStringList& aItems )
{
   QMenu* menu = aParent->addMenu( aTitle );
   for ( const QString& item : aItems )
      if ( item.isEmpty() )
         menu->addSeparator();
      else
         menu->addAction( item );
   menu->setEnabled( false );
   return menu;
}

}  // namespace

Frm_Editor::Frm_Editor( QWidget* aParent ) : QMainWindow( aParent )
{
   setWindowTitle( QStringLiteral( "GameList Editor Qt" ) );

   FModel = new GamelistModel( this );
   FProxy = new GamelistFilter( this );
   FProxy->setSourceModel( FModel );

   buildMenu();
   buildCentralWidget();
   connectSignals();
   LoadFromIni();

   resize( 1024, 700 );
}

void Frm_Editor::buildMenu()
{
   QMenu* file = menuBar()->addMenu( QStringLiteral( "File" ) );
   file->addAction( QStringLiteral( "Choose folder..." ), QKeySequence::Open, this,
                     [this] { BuildSystemsList(); } );
   Mnu_Reload = file->addAction( QStringLiteral( "Reload Systems" ), QKeySequence::Refresh,
                                  this, [this] { BuildSystemsList( true ); } );
   Mnu_Reload->setEnabled( false );
   file->addSeparator();
   file->addAction( QStringLiteral( "Quit" ), QKeySequence::Quit, this, &Frm_Editor::close );

   QMenu* actions = menuBar()->addMenu( QStringLiteral( "Actions" ) );
   QMenu* system = actions->addMenu( QStringLiteral( "System" ) );
   // Everything here works on the loaded system, so it starts disabled.
   const auto addSystemAction = [this, system]( const QString& aText, auto aHandler ) {
      QAction* action = system->addAction( aText, this, aHandler );
      action->setEnabled( false );
      FSystemActions.append( action );
   };
   // A SystemAction is a free function, so it goes through runAction.
   const auto command = [this]( SystemAction aAction ) { return [this, aAction] { runAction( aAction ); }; };
   addSystemAction( QStringLiteral( "Add missing ROMs..." ), command( &addMissingRoms ) );
   addSystemAction( QStringLiteral( "Link existing media..." ), command( &linkExistingMedia ) );
   addSystemAction( QStringLiteral( "Rename media after ROMs..." ), command( &renameMediaAfterRoms ) );
   addSystemAction( QStringLiteral( "Shrink large pictures..." ), command( &shrinkLargeImages ) );
   addSystemAction( QStringLiteral( "Set system type..." ), &Frm_Editor::setSystemType );
   system->addSeparator();
   // Not implemented yet, so always disabled.
   for ( const char* text : { "Convert all text to lowercase", "Convert all text to uppercase",
                              "Remove region from games names" } )
      system->addAction( QLatin1String( text ) )->setEnabled( false );
   system->addSeparator();
   addSystemAction( QStringLiteral( "Delete orphans from gamelist..." ), command( &deleteOrphans ) );
   addSystemAction( QStringLiteral( "Delete duplicates from gamelist..." ), command( &deleteDuplicates ) );
   system->addSeparator();
   addSystemAction( QStringLiteral( "Export games list to txt file..." ), [this] {
      QStringList names;
      for ( int row = 0; row < FProxy->rowCount(); ++row )
         names.append( FProxy->index( row, 0 ).data().toString() );
      exportGamesList( this, FGamelist, names );
   } );
   addSystemAction( QStringLiteral( "Clean up gamelist backups..." ),
                    [this] { cleanUpBackups( this, FGamelist ); } );
   addPendingMenu( actions, QStringLiteral( "Game" ),
                   { QStringLiteral( "Convert text to lowercase" ),
                     QStringLiteral( "Convert text to uppercase" ) } );
   addPendingMenu( actions, QStringLiteral( "Selection" ),
                   { QStringLiteral( "Add to Favorites" ), QStringLiteral( "Remove from Favorites" ),
                     QString(), QStringLiteral( "Add to Hidden" ),
                     QStringLiteral( "Remove from Hidden" ), QString(),
                     QStringLiteral( "Advanced name editor" ) } );

   QMenu* options = menuBar()->addMenu( QStringLiteral( "Options" ) );
   QMenu* general = options->addMenu( QStringLiteral( "General" ) );

   Mnu_GodMode = addOption( general, QStringLiteral( "Enable God Mode" ), Cst_IniGodMode, false );
   Mnu_DeleteWoPrompt =
      addOption( general, QStringLiteral( "Delete without prompt" ), Cst_IniDelWoPrompt, false );
   Mnu_AutoHash = addOption( general, QStringLiteral( "Auto Hash" ), Cst_IniAutoHash, false );
   Mnu_ShowTips = addOption( general, QStringLiteral( "Show tips at start" ), Cst_ShowTips, true );
   Mnu_Genesis = addOption( general, QStringLiteral( "Use Genesis logo" ), Cst_IniGenesisLogo, false );
   Mnu_AutoplayVideo =
      addOption( general, QStringLiteral( "Autoplay video" ), Cst_IniAutoplayVideo, false );

   options->addSeparator();
   QMenu* network = options->addMenu( QStringLiteral( "Network" ) );
   network->addAction( QStringLiteral( "Configure..." ), this,
                       [this] { Frm_Network( this ).Execute(); } );

   QMenu* help = menuBar()->addMenu( QStringLiteral( "Help" ) );
   help->addAction( QStringLiteral( "Help" ), QKeySequence::HelpContents, this, [this] {
      // False: opening Help from the menu must not offer the opt-out, and so
      // cannot change the setting. See Frm_Help::Execute.
      Frm_Help( this ).Execute( Mnu_ShowTips->isChecked(), false );
   } );
   help->addAction( QStringLiteral( "About" ), this, [this] { Frm_About( this ).exec(); } );
}

void Frm_Editor::buildCentralWidget()
{
   QWidget* central = new QWidget( this );
   QHBoxLayout* columns = new QHBoxLayout( central );

   QSplitter* splitter = new QSplitter( Qt::Horizontal, central );
   splitter->setChildrenCollapsible( false );
   columns->addWidget( splitter );

   splitter->addWidget( buildListColumn( splitter ) );
   splitter->addWidget( buildEditorTabs( splitter ) );
   splitter->setStretchFactor( 1, 1 );

   setCentralWidget( central );
}

QWidget* Frm_Editor::buildListColumn( QWidget* aParent )
{
   QWidget* column = new QWidget( aParent );
   QVBoxLayout* left = new QVBoxLayout( column );
   left->setContentsMargins( 0, 0, 0, 0 );

   Img_System = new QLabel( column );
   Img_System->setObjectName( QStringLiteral( "Img_System" ) );
   Img_System->setAlignment( Qt::AlignCenter );
   Img_System->setMinimumHeight( 70 );
   // Ignored: the label's hints would follow the logo and the logo would size
   // the left column. The column is sized by the controls; the logo fits it.
   Img_System->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Fixed );
   Img_System->installEventFilter( this );
   left->addWidget( Img_System );

   QFormLayout* choices = new QFormLayout;
   left->addLayout( choices );

   Cbx_Systems = new QComboBox( column );
   Cbx_Systems->setObjectName( QStringLiteral( "Cbx_Systems" ) );
   Cbx_Systems->setEnabled( false );
   choices->addRow( QStringLiteral( "Select your system" ), Cbx_Systems );

   Cbx_Filter = new QComboBox( column );
   Cbx_Filter->setObjectName( QStringLiteral( "Cbx_Filter" ) );
   for ( int category = 0; category < static_cast<int>( FilterCategory::Count ); ++category )
      Cbx_Filter->addItem( QLatin1String( filterCategoryName( static_cast<FilterCategory>( category ) ) ),
                           category );
   Cbx_Filter->setEnabled( false );
   choices->addRow( QStringLiteral( "Select your filter" ), Cbx_Filter );

   Edt_Search = new QLineEdit( column );
   Edt_Search->setObjectName( QStringLiteral( "Edt_Search" ) );
   Edt_Search->setClearButtonEnabled( true );
   Edt_Search->setEnabled( false );
   choices->addRow( QStringLiteral( "Search" ), Edt_Search );

   // Object names match the member names so a test can reach these with findChild.
   Chk_ListByRom = new QCheckBox( QStringLiteral( "List by Rom name" ), column );
   Chk_ListByRom->setObjectName( QStringLiteral( "Chk_ListByRom" ) );
   Chk_FullRomName = new QCheckBox( QStringLiteral( "Show full Rom name" ), column );
   Chk_FullRomName->setObjectName( QStringLiteral( "Chk_FullRomName" ) );
   Chk_FullRomName->setEnabled( false );
   left->addWidget( Chk_ListByRom );
   left->addWidget( Chk_FullRomName );

   Lbx_Games = new QListView( column );
   Lbx_Games->setObjectName( QStringLiteral( "Lbx_Games" ) );
   Lbx_Games->setModel( FProxy );
   Lbx_Games->setSelectionMode( QAbstractItemView::ExtendedSelection );
   Lbx_Games->setUniformItemSizes( true );
   Lbx_Games->setMinimumWidth( 280 );
   left->addWidget( Lbx_Games, 1 );

   Lbl_NbGamesFound = new QLabel( column );
   Lbl_NbGamesFound->setObjectName( QStringLiteral( "Lbl_NbGamesFound" ) );
   left->addWidget( Lbl_NbGamesFound );

   return column;
}

QTabWidget* Frm_Editor::buildEditorTabs( QWidget* aParent )
{
   Pgc_Editor = new QTabWidget( aParent );
   Pgc_Editor->setObjectName( QStringLiteral( "Pgc_Editor" ) );

   FEditPanel = new GameEditPanel( Pgc_Editor );
   Pgc_Editor->addTab( FEditPanel, QStringLiteral( "Main" ) );

   FScrapePanel = new ScrapePanel( Pgc_Editor );
   Pgc_Editor->addTab( FScrapePanel, QStringLiteral( "Scrape" ) );

   // Both panels reload the edit tab, which would drop typed edits.
   FEditPanel->setLeaveGuard( [this] { return confirmLeaveEdits(); } );
   FScrapePanel->setLeaveGuard( [this] { return confirmLeaveEdits(); } );

   return Pgc_Editor;
}

// Runs after both builders, so no slot can reach a widget that does not exist yet.
void Frm_Editor::connectSignals()
{
   connect( Cbx_Systems, &QComboBox::currentIndexChanged, this, &Frm_Editor::Cbx_SystemsChange );
   connect( Cbx_Filter, &QComboBox::currentIndexChanged, this, &Frm_Editor::refreshFilter );
   connect( Edt_Search, &QLineEdit::textChanged, this, &Frm_Editor::refreshFilter );
   connect( Chk_ListByRom, &QCheckBox::toggled, this, [this]( bool aChecked ) {
      Chk_FullRomName->setEnabled( aChecked );
      refreshFilter();
   } );
   connect( Chk_FullRomName, &QCheckBox::toggled, this, &Frm_Editor::refreshFilter );

   connect( Lbx_Games->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] {
      if ( filterDependsOnSelection( currentCategory( Cbx_Filter ) ) )
         refreshFilter();
   } );
   connect( Lbx_Games->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &Frm_Editor::onSelectionChanged );

   connect( FEditPanel, &GameEditPanel::gamesChanged, this, [this] {
      FModel->refresh();
      updateCount();
   } );
   connect( FEditPanel, &GameEditPanel::moreInfosRequested, this, &Frm_Editor::showMoreInfos );
   connect( FEditPanel, &GameEditPanel::gameDeleted, this, &Frm_Editor::onGameDeleted );

   connect( Pgc_Editor, &QTabWidget::currentChanged, this, [this]( int aIndex ) {
      if ( Pgc_Editor->widget( aIndex ) == FScrapePanel )
         FScrapePanel->activate();
   } );
   // Disabling the host keeps each child's own enabled state for when the lock lifts.
   connect( FScrapePanel, &ScrapePanel::busyChanged, this, [this]( bool aBusy ) {
      FScrapeBusy = aBusy;
      Lbx_Games->parentWidget()->setEnabled( !aBusy );
      Pgc_Editor->setTabEnabled( Pgc_Editor->indexOf( FEditPanel ), !aBusy );
      menuBar()->setEnabled( !aBusy );
   } );
   connect( FScrapePanel, &ScrapePanel::gamesChanged, this, [this] {
      refreshViews();
      Pgc_Editor->setCurrentWidget( FEditPanel );
   } );

   // God mode is what makes deleting possible at all, so it gates its own
   // "don't ask me" - and turning it off has to clear that, not just grey it.
   Mnu_DeleteWoPrompt->setEnabled( false );
   connect( Mnu_GodMode, &QAction::toggled, this, [this]( bool aChecked ) {
      Mnu_DeleteWoPrompt->setEnabled( aChecked );
      if ( !aChecked )
         Mnu_DeleteWoPrompt->setChecked( false );
   } );

   const auto syncGodMode = [this] {
      FEditPanel->setGodMode( Mnu_GodMode->isChecked(), Mnu_DeleteWoPrompt->isChecked() );
   };
   connect( Mnu_GodMode, &QAction::toggled, this, syncGodMode );
   connect( Mnu_DeleteWoPrompt, &QAction::toggled, this, syncGodMode );
   connect( Mnu_AutoplayVideo, &QAction::toggled, FEditPanel, &GameEditPanel::setAutoplay );
   connect( Mnu_Genesis, &QAction::toggled, this, &Frm_Editor::onGenesisToggled );
}

// The editor follows the selection, and the list follows the editor: a saved
// name is a new row label, and a saved field can move the game out of the
// current filter.
void Frm_Editor::onSelectionChanged()
{
   if ( FRestoringSelection )
      return;

   if ( !confirmLeaveEdits() ) {
      restoreSelection( FEditPanel->selection() );
      return;
   }

   const QVector<int> selected = selectedGameIndexes();
   FEditPanel->setSelection( selected );
   FScrapePanel->setSelection( selected.size() == 1 ? selected.first() : -1 );
}

// Rows are indices into the Gamelist, and removing one renumbered the rest:
// reset the model, then land on the neighbour the user was next to.
void Frm_Editor::onGameDeleted()
{
   const int row = qMax( 0, Lbx_Games->currentIndex().row() );

   bindGamelist( &FGamelist );
   refreshFilter();

   if ( FProxy->rowCount() > 0 )
      Lbx_Games->setCurrentIndex( FProxy->index( qMin( row, FProxy->rowCount() - 1 ), 0 ) );
}

// The Megadrive is the Genesis in North America, and the logo differs. Only the item's
// text and the logo change; the selection stays where it is.
void Frm_Editor::onGenesisToggled()
{
   const int item =
      findSystemItem( []( const SystemEntry& aSystem ) { return aSystem.kind == skMegaDrive; } );
   if ( item >= 0 )
      Cbx_Systems->setItemText( item, systemDisplayName( systemAt( item ), Mnu_Genesis->isChecked() ) );

   if ( Cbx_Systems->currentIndex() >= 0 )
      LoadSystemLogo( systemAt( Cbx_Systems->currentIndex() ).kind );
}

QAction* Frm_Editor::addOption( QMenu* aMenu, const QString& aText, const char* aKey, bool aDefault )
{
   QAction* action = aMenu->addAction( aText );
   action->setCheckable( true );
   FOptions.append( { action, aKey, aDefault } );
   return action;
}

void Frm_Editor::LoadFromIni()
{
   QSettings settings;
   settings.beginGroup( QLatin1String( Cst_IniOptions ) );

   // In menu order, so God mode is set before the option it gates.
   for ( const Option& option : std::as_const( FOptions ) )
      option.action->setChecked( settings.value( QLatin1String( option.key ), option.fallback ).toBool() );
}

void Frm_Editor::SaveToIni()
{
   QSettings settings;
   settings.beginGroup( QLatin1String( Cst_IniOptions ) );

   for ( const Option& option : std::as_const( FOptions ) )
      settings.setValue( QLatin1String( option.key ), option.action->isChecked() );

   // Without god mode there is no delete to skip the prompt for.
   settings.setValue( QLatin1String( Cst_IniDelWoPrompt ),
                      Mnu_GodMode->isChecked() && Mnu_DeleteWoPrompt->isChecked() );
}

void Frm_Editor::showTipsAtStart()
{
   if ( Mnu_ShowTips->isChecked() )
      Mnu_ShowTips->setChecked( Frm_Help( this ).Execute( true, true ) );
}

void Frm_Editor::closeEvent( QCloseEvent* aEvent )
{
   // A scrape or save in flight still holds the Gamelist and would land on a closed window.
   if ( FScrapeBusy ) {
      aEvent->ignore();
      return;
   }

   // Quitting mid-edit must not lose the edit silently.
   if ( !confirmLeaveEdits() ) {
      aEvent->ignore();
      return;
   }

   SaveToIni();
   QMainWindow::closeEvent( aEvent );
}

bool Frm_Editor::confirmLeaveEdits()
{
   if ( !FEditPanel->isDirty() )
      return true;

   const QMessageBox::StandardButton answer =
      QMessageBox::question( this, QStringLiteral( "GameList Editor Qt" ),
                             QStringLiteral(
                                "There are changes you have not saved yet.\nSave them first?" ),
                             QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel );

   if ( answer == QMessageBox::Save )
      return FEditPanel->save();

   if ( answer == QMessageBox::Discard ) {
      // Reloaded, so that the next caller does not ask again.
      FEditPanel->setSelection( FEditPanel->selection() );
      return true;
   }

   return false;
}

void Frm_Editor::restoreSelection( const QVector<int>& aIndices )
{
   QItemSelectionModel* model = Lbx_Games->selectionModel();
   const auto row = [this]( int aIndex ) {
      return FProxy->mapFromSource( FModel->index( aIndex ) );
   };

   QScopedValueRollback<bool> restoring( FRestoringSelection, true );

   // Current first: on a "Same ..." filter it decides which rows exist, so the
   // selection is only mapped once the filter has followed it back.
   model->setCurrentIndex( aIndices.isEmpty() ? QModelIndex() : row( aIndices.first() ),
                           QItemSelectionModel::NoUpdate );

   QItemSelection selection;
   for ( int index : aIndices ) {
      const QModelIndex proxyRow = row( index );
      if ( proxyRow.isValid() )
         selection.select( proxyRow, proxyRow );
   }
   model->select( selection, QItemSelectionModel::ClearAndSelect );
}

void Frm_Editor::BuildSystemsList( bool aReload )
{
   if ( aReload ) {
      openRootFolder( FRootPath );
      return;
   }

   QSettings settings;
   settings.beginGroup( QLatin1String( Cst_IniOptions ) );

   // Start where the user last picked, as long as that folder still exists.
   QString start = FRootPath;
   if ( start.isEmpty() ) {
      const QString last = settings.value( QLatin1String( Cst_IniLastFolder ) ).toString();
      if ( QDir( last ).exists() )
         start = last;
   }

   const QString root = QFileDialog::getExistingDirectory(
      this, QStringLiteral( "Select the folder that holds your system folders" ), start );
   if ( root.isEmpty() )
      return;

   settings.setValue( QLatin1String( Cst_IniLastFolder ), root );

   openRootFolder( root );
}

void Frm_Editor::openRootFolder( const QString& aRootPath )
{
   if ( !confirmLeaveEdits() )
      return;

   FRootPath = aRootPath;

   QVector<SystemEntry> systems = scanSystems( FRootPath );
   applySystemMap( systems );

   // Filling the combo would otherwise load the first system halfway through.
   const QSignalBlocker blocked( Cbx_Systems );
   Cbx_Systems->clear();
   for ( const SystemEntry& system : systems )
      Cbx_Systems->addItem( systemDisplayName( system, Mnu_Genesis->isChecked() ),
                            QVariant::fromValue( system ) );

   const bool found = !systems.isEmpty();
   Cbx_Systems->setEnabled( found );
   Cbx_Filter->setEnabled( found );
   Edt_Search->setEnabled( found );
   Mnu_Reload->setEnabled( found );

   if ( !found ) {
      setSystemActionsEnabled( false );
      bindGamelist( nullptr );
      clearSystemLogo();
      updateCount();
      QMessageBox::information(
         this, QStringLiteral( "Information" ),
         QStringLiteral( "Oops !! It looks like you selected the wrong folder !\n"
                         "Please select the root folder where your systems folders are stored." ) );
      return;
   }

   loadSelectedSystem();
}

void Frm_Editor::Cbx_SystemsChange()
{
   if ( !confirmLeaveEdits() ) {
      // Put the combo back on the system the edit belongs to.
      const QSignalBlocker blocked( Cbx_Systems );
      const QString path = FGamelist.system().gamelistPath;
      const int item =
         findSystemItem( [&path]( const SystemEntry& aSystem ) { return aSystem.gamelistPath == path; } );
      if ( item >= 0 )
         Cbx_Systems->setCurrentIndex( item );
      return;
   }

   loadSelectedSystem();
}

void Frm_Editor::loadSelectedSystem()
{
   bindGamelist( nullptr );
   setSystemActionsEnabled( false );
   const QVariant systemData = Cbx_Systems->currentData();
   if ( !systemData.canConvert<SystemEntry>() ) {
      clearSystemLogo();
      updateCount();
      return;
   }

   const SystemEntry system = systemData.value<SystemEntry>();
   LoadSystemLogo( system.kind );

   // Blocked: clearing the box would re-run the filter, which is applied below anyway.
   {
      const QSignalBlocker blocked( Edt_Search );
      Edt_Search->clear();
   }

   if ( const Result<> loaded = FGamelist.load( system ); !loaded ) {
      updateCount();
      QMessageBox::warning( this, QStringLiteral( "GameList Editor Qt" ), loaded.error() );
      return;
   }

   resetViews();
   setSystemActionsEnabled( true );

   if ( FProxy->rowCount() > 0 )
      Lbx_Games->setCurrentIndex( FProxy->index( 0, 0 ) );
}

SystemEntry Frm_Editor::systemAt( int aItem ) const
{
   return Cbx_Systems->itemData( aItem ).value<SystemEntry>();
}

int Frm_Editor::findSystemItem( const std::function<bool( const SystemEntry& )>& aMatch ) const
{
   for ( int item = 0; item < Cbx_Systems->count(); ++item )
      if ( aMatch( systemAt( item ) ) )
         return item;

   return -1;
}

void Frm_Editor::bindGamelist( Gamelist* aGamelist )
{
   FModel->setGamelist( aGamelist );
   FEditPanel->setGamelist( aGamelist );
   FScrapePanel->setGamelist( aGamelist );
}

void Frm_Editor::runAction( SystemAction aAction )
{
   if ( !confirmLeaveEdits() )
      return;

   const int before = FGamelist.count();
   switch ( aAction( this, FGamelist ) ) {
      case ViewUpdate::None: return;
      case ViewUpdate::Refresh: refreshViews(); return;
      case ViewUpdate::Reset: break;
   }

   resetViews();

   // Added games come after the old ones, so the first new one is the row to show (a filter
   // can hide it, so there may be no row to land on). After a removal, the first row.
   QModelIndex row;
   if ( FGamelist.count() > before )
      row = FProxy->mapFromSource( FModel->index( before ) );
   else if ( FProxy->rowCount() > 0 )
      row = FProxy->index( 0, 0 );

   if ( row.isValid() )
      Lbx_Games->setCurrentIndex( row );
}

void Frm_Editor::resetViews()
{
   bindGamelist( &FGamelist );
   refreshFilter();
}

void Frm_Editor::refreshViews()
{
   // Reload the editor first: refresh() can move the selection, and confirmLeaveEdits()
   // would find the stale widgets dirty against the values just saved.
   FEditPanel->setSelection( selectedGameIndexes() );
   FModel->refresh();
   updateCount();
}

void Frm_Editor::setSystemActionsEnabled( bool aEnabled )
{
   for ( QAction* action : std::as_const( FSystemActions ) )
      action->setEnabled( aEnabled );
}

void Frm_Editor::LoadSystemLogo( SystemKind aKind )
{
   const SystemKind kind = ( aKind == skMegaDrive && Mnu_Genesis->isChecked() ) ? skGenesis : aKind;

   // A missing logo file just clears the label.
   if ( !FSystemLogo.load( QLatin1String( Cst_LogoPicsFolder ) +
                           QLatin1String( Cst_SystemKindImageNames[kind] ) ) ) {
      clearSystemLogo();
      return;
   }

   scaleSystemLogo();
}

void Frm_Editor::clearSystemLogo()
{
   FSystemLogo = QPixmap();
   Img_System->clear();
}

void Frm_Editor::scaleSystemLogo()
{
   if ( FSystemLogo.isNull() )
      return;

   Img_System->setPixmap( FSystemLogo.scaled( QSize( Img_System->width(), 64 ), Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation ) );
}

bool Frm_Editor::eventFilter( QObject* aWatched, QEvent* aEvent )
{
   if ( aWatched == Img_System && aEvent->type() == QEvent::Resize )
      scaleSystemLogo();

   return QMainWindow::eventFilter( aWatched, aEvent );
}

void Frm_Editor::setSystemType()
{
   const int item = Cbx_Systems->currentIndex();
   if ( item < 0 )
      return;

   SystemEntry system = systemAt( item );
   const std::optional<SystemKind> kind = chooseSystemKind( this, system );
   if ( !kind )
      return;

   system.kind = *kind;
   Cbx_Systems->setItemData( item, QVariant::fromValue( system ) );
   Cbx_Systems->setItemText( item, systemDisplayName( system, Mnu_Genesis->isChecked() ) );
   FGamelist.setSystemKind( *kind );
   LoadSystemLogo( *kind );
}

void Frm_Editor::refreshFilter()
{
   if ( FApplyingFilter )
      return;

   QScopedValueRollback<bool> applying( FApplyingFilter, true );

   FilterSpec filter;
   filter.category = currentCategory( Cbx_Filter );
   filter.search = Edt_Search->text();
   filter.listByRom = Chk_ListByRom->isChecked();
   filter.fullRomName = Chk_FullRomName->isChecked();
   filter.reference = currentGameIndex();

   FModel->setFilter( filter );
   updateCount();
}

void Frm_Editor::updateCount()
{
   const int shown = FProxy->rowCount();

   Lbl_NbGamesFound->setText(
      currentCategory( Cbx_Filter ) == FilterCategory::All
         ? QStringLiteral( "%1 game(s) found." ).arg( shown )
         : QStringLiteral( "%1 / %2 game(s) found." ).arg( shown ).arg( FModel->rowCount() ) );
}

int Frm_Editor::currentGameIndex() const
{
   const QModelIndex current = Lbx_Games->currentIndex();
   return current.isValid() ? FProxy->mapToSource( current ).row() : -1;
}

QVector<int> Frm_Editor::selectedGameIndexes() const
{
   QVector<int> indexes;
   for ( const QModelIndex& row : Lbx_Games->selectionModel()->selectedIndexes() )
      indexes.append( FProxy->mapToSource( row ).row() );

   return indexes;
}

void Frm_Editor::showMoreInfos( int aIndex )
{
   if ( aIndex < 0 || aIndex >= FGamelist.count() )
      return;

   const Game& game = FGamelist.at( aIndex );
   if ( game.md5.isEmpty() || game.sha1.isEmpty() || game.crc32.isEmpty() ) {
      const bool compute =
         Mnu_AutoHash->isChecked() ||
         QMessageBox::question(
            this, QStringLiteral( "Information" ),
            QStringLiteral( "The hashes of this game have not been calculated yet.\n"
                            "On a large ROM that can take a moment. Calculate them now?" ) ) ==
            QMessageBox::Yes;

      if ( compute )
         FGamelist.ensureHashes( aIndex );
   }

   Frm_MoreInfos( this ).Execute( FGamelist.at( aIndex ) );
}
