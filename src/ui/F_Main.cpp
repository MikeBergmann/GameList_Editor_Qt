#include "F_Main.h"

#include "F_About.h"
#include "F_AddRoms.h"
#include "F_ConfigureNetwork.h"
#include "F_Help.h"
#include "F_LinkMedia.h"
#include "F_MoreInfos.h"
#include "GameEditPanel.h"
#include "GamelistModel.h"
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
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMessageLogger>
#include <QPixmap>
#include <QProgressDialog>
#include <QSaveFile>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {

// Cbx_Filter, in the order the indices of FilterSpec::index are numbered.
const char* const Cst_FilterNames[] = {
   "All",
   "Missing Picture",
   "Missing Video",
   "Missing Date",
   "Missing Number of Players",
   "Missing Rating",
   "Missing Developer",
   "Missing Publisher",
   "Missing Description",
   "Missing Genre",
   "Missing Region",
   "Kid Game",
   "Hidden",
   "Favorite",
   "Orphan",
   "Same Region",
   "Same Date",
   "Same or Earlier Date",
   "Same or Later Date",
   "Same Players",
   "Same Rating",
   "Same or Lower Rating",
   "Same or Higher Rating",
   "Same Publisher",
   "Same Developer",
   "Same Genre",
   "Same Folder",
   "Same Name",
   "Same ROM",
   "Duplicate Names",
   "Duplicate ROMs",
};

// Filters 15-28 compare each game against the selected one, so re-filtering has
// to follow the selection. Everything else is independent of it.
bool dependsOnSelection( int aFilterIndex )
{
   return aFilterIndex >= 15 && aFilterIndex <= 28;
}

// The entries of Actions that operate on a whole system or on a selection. They
// exist here because the menu is this unit's; their handlers are step 4.4's.
// An empty item is a separator.
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
   Mnu_System = actions->addMenu( QStringLiteral( "System" ) );
   Mnu_AddRoms = Mnu_System->addAction( QStringLiteral( "Add missing ROMs..." ), this,
                                        &Frm_Editor::addMissingRoms );
   Mnu_AddRoms->setEnabled( false );
   Mnu_LinkMedia = Mnu_System->addAction( QStringLiteral( "Link existing media..." ), this,
                                          &Frm_Editor::linkExistingMedia );
   Mnu_LinkMedia->setEnabled( false );
   Mnu_RenameMedia = Mnu_System->addAction( QStringLiteral( "Rename media after ROMs..." ), this,
                                            &Frm_Editor::renameMediaAfterRoms );
   Mnu_RenameMedia->setEnabled( false );
   Mnu_ShrinkImages = Mnu_System->addAction( QStringLiteral( "Shrink large pictures..." ), this,
                                             &Frm_Editor::shrinkLargeImages );
   Mnu_ShrinkImages->setEnabled( false );
   Mnu_SetSystemType = Mnu_System->addAction( QStringLiteral( "Set system type..." ), this,
                                              &Frm_Editor::setSystemType );
   Mnu_SetSystemType->setEnabled( false );
   Mnu_System->addSeparator();
   for ( const char* text : { "Convert all text to lowercase", "Convert all text to uppercase",
                              "Remove region from games names" } )
      Mnu_System->addAction( QLatin1String( text ) )->setEnabled( false );
   Mnu_System->addSeparator();
   Mnu_DeleteOrphans = Mnu_System->addAction( QStringLiteral( "Delete orphans from gamelist..." ),
                                              this, &Frm_Editor::deleteOrphans );
   Mnu_DeleteDuplicates = Mnu_System->addAction(
      QStringLiteral( "Delete duplicates from gamelist..." ), this, &Frm_Editor::deleteDuplicates );
   Mnu_System->addSeparator();
   Mnu_ExportList = Mnu_System->addAction( QStringLiteral( "Export games list to txt file..." ),
                                           this, &Frm_Editor::exportGamesList );
   Mnu_DeleteOrphans->setEnabled( false );
   Mnu_DeleteDuplicates->setEnabled( false );
   Mnu_ExportList->setEnabled( false );
   Mnu_Game = addPendingMenu( actions, QStringLiteral( "Game" ),
                              { QStringLiteral( "Convert text to lowercase" ),
                                QStringLiteral( "Convert text to uppercase" ) } );
   Mnu_Selection = addPendingMenu( actions, QStringLiteral( "Selection" ),
                                   { QStringLiteral( "Add to Favorites" ),
                                     QStringLiteral( "Remove from Favorites" ), QString(),
                                     QStringLiteral( "Add to Hidden" ),
                                     QStringLiteral( "Remove from Hidden" ), QString(),
                                     QStringLiteral( "Advanced name editor" ) } );

   QMenu* options = menuBar()->addMenu( QStringLiteral( "Options" ) );
   QMenu* general = options->addMenu( QStringLiteral( "General" ) );

   const auto addToggle = [general]( const QString& aText, bool* aTarget ) {
      QAction* action = general->addAction( aText );
      action->setCheckable( true );
      QObject::connect( action, &QAction::toggled, [aTarget]( bool aChecked ) { *aTarget = aChecked; } );
      return action;
   };

   Mnu_GodMode = addToggle( QStringLiteral( "Enable God Mode" ), &FGodMode );
   Mnu_DeleteWoPrompt = addToggle( QStringLiteral( "Delete without prompt" ), &FDelWoPrompt );
   Mnu_AutoHash = addToggle( QStringLiteral( "Auto Hash" ), &FAutoHash );
   Mnu_ShowTips = addToggle( QStringLiteral( "Show tips at start" ), &FShowTips );
   Mnu_Genesis = addToggle( QStringLiteral( "Use Genesis logo" ), &FGenesisLogo );
   Mnu_AutoplayVideo = addToggle( QStringLiteral( "Autoplay video" ), &FAutoplayVideo );

   // God mode is what makes deleting possible at all, so it gates its own
   // "don't ask me" - and turning it off has to clear that, not just grey it.
   connect( Mnu_GodMode, &QAction::toggled, this, [this]( bool aChecked ) {
      Mnu_DeleteWoPrompt->setEnabled( aChecked );
      if ( !aChecked )
         Mnu_DeleteWoPrompt->setChecked( false );
   } );

   // The Megadrive is the Genesis in North America, and the logo differs. The
   // Delphi rewrote the combo item and then set ItemIndex to the loop counter,
   // which jumped the selection to that system (or past the end of the list).
   connect( Mnu_Genesis, &QAction::toggled, this, [this] {
      for ( int item = 0; item < Cbx_Systems->count(); ++item ) {
         const SystemEntry system = Cbx_Systems->itemData( item ).value<SystemEntry>();
         if ( system.kind != skMegaDrive )
            continue;

         Cbx_Systems->setItemText( item, systemDisplayName( system ) );
         break;
      }

      if ( Cbx_Systems->currentIndex() >= 0 )
         LoadSystemLogo( Cbx_Systems->currentData().value<SystemEntry>().kind );
   } );

   options->addSeparator();
   QMenu* network = options->addMenu( QStringLiteral( "Network" ) );
   network->addAction( QStringLiteral( "Configure..." ), this,
                       [this] { Frm_Network( this ).Execute(); } );

   QMenu* help = menuBar()->addMenu( QStringLiteral( "Help" ) );
   help->addAction( QStringLiteral( "Help" ), QKeySequence::HelpContents, this, [this] {
      // False: opening Help from the menu must not offer the opt-out, and so
      // cannot change the setting. See Frm_Help::Execute.
      Frm_Help( this ).Execute( FShowTips, false );
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

   QWidget* leftHost = new QWidget( splitter );
   QVBoxLayout* left = new QVBoxLayout( leftHost );
   left->setContentsMargins( 0, 0, 0, 0 );

   Img_System = new QLabel( central );
   Img_System->setAlignment( Qt::AlignCenter );
   Img_System->setMinimumHeight( 70 );
   // Ignored: the label's hints would follow the logo and the logo would size
   // the left column. The column is sized by the controls; the logo fits it.
   Img_System->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Fixed );
   Img_System->installEventFilter( this );
   left->addWidget( Img_System );

   QFormLayout* choices = new QFormLayout;
   left->addLayout( choices );

   Cbx_Systems = new QComboBox( central );
   Cbx_Systems->setObjectName( QStringLiteral( "Cbx_Systems" ) );
   Cbx_Systems->setEnabled( false );
   choices->addRow( QStringLiteral( "Select your system" ), Cbx_Systems );

   Cbx_Filter = new QComboBox( central );
   Cbx_Filter->setObjectName( QStringLiteral( "Cbx_Filter" ) );
   for ( const char* name : Cst_FilterNames )
      Cbx_Filter->addItem( QLatin1String( name ) );
   Cbx_Filter->setEnabled( false );
   choices->addRow( QStringLiteral( "Select your filter" ), Cbx_Filter );

   Edt_Search = new QLineEdit( central );
   Edt_Search->setObjectName( QStringLiteral( "Edt_Search" ) );
   Edt_Search->setClearButtonEnabled( true );
   Edt_Search->setEnabled( false );
   choices->addRow( QStringLiteral( "Search" ), Edt_Search );

   // The object names are the Delphi component names, so that findChild can
   // reach these from a test the way FindComponent would have.
   Chk_ListByRom = new QCheckBox( QStringLiteral( "List by Rom name" ), central );
   Chk_ListByRom->setObjectName( QStringLiteral( "Chk_ListByRom" ) );
   Chk_FullRomName = new QCheckBox( QStringLiteral( "Show full Rom name" ), central );
   Chk_FullRomName->setObjectName( QStringLiteral( "Chk_FullRomName" ) );
   Chk_FullRomName->setEnabled( false );
   left->addWidget( Chk_ListByRom );
   left->addWidget( Chk_FullRomName );

   Lbx_Games = new QListView( central );
   Lbx_Games->setObjectName( QStringLiteral( "Lbx_Games" ) );
   Lbx_Games->setModel( FProxy );
   Lbx_Games->setSelectionMode( QAbstractItemView::ExtendedSelection );
   Lbx_Games->setUniformItemSizes( true );
   Lbx_Games->setMinimumWidth( 280 );
   left->addWidget( Lbx_Games, 1 );

   Lbl_NbGamesFound = new QLabel( central );
   Lbl_NbGamesFound->setObjectName( QStringLiteral( "Lbl_NbGamesFound" ) );
   left->addWidget( Lbl_NbGamesFound );

   // Tbs_Main gets the editor, Tbs_Scrape gets ScrapePanel. Object names match
   // the Delphi TTabSheets.
   Pgc_Editor = new QTabWidget( central );
   Pgc_Editor->setObjectName( QStringLiteral( "Pgc_Editor" ) );

   FEditPanel = new GameEditPanel( Pgc_Editor );
   Pgc_Editor->addTab( FEditPanel, QStringLiteral( "Main" ) );

   FScrapePanel = new ScrapePanel( Pgc_Editor );
   Pgc_Editor->addTab( FScrapePanel, QStringLiteral( "Scrape" ) );

   splitter->addWidget( Pgc_Editor );
   splitter->setStretchFactor( 1, 1 );

   setCentralWidget( central );

   connect( Cbx_Systems, &QComboBox::currentIndexChanged, this, &Frm_Editor::Cbx_SystemsChange );
   connect( Cbx_Filter, &QComboBox::currentIndexChanged, this, &Frm_Editor::refreshFilter );
   connect( Edt_Search, &QLineEdit::textChanged, this, &Frm_Editor::refreshFilter );
   connect( Chk_ListByRom, &QCheckBox::toggled, this, [this]( bool aChecked ) {
      Chk_FullRomName->setEnabled( aChecked );
      refreshFilter();
   } );
   connect( Chk_FullRomName, &QCheckBox::toggled, this, &Frm_Editor::refreshFilter );

   connect( Lbx_Games->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] {
      if ( dependsOnSelection( Cbx_Filter->currentIndex() ) )
         refreshFilter();
   } );

   // The editor follows the selection, and the list follows the editor: a saved
   // name is a new row label, and a saved field can move the game out of the
   // current filter.
   connect( Lbx_Games->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
      if ( FRestoringSelection )
         return;

      if ( !confirmLeaveEdits() ) {
         restoreSelection( FEditPanel->selection() );
         return;
      }

      const QVector<int> selected = selectedGameIndexes();
      FEditPanel->setSelection( selected );
      FScrapePanel->setSelection( selected.size() == 1 ? selected.first() : -1 );
   } );
   connect( FEditPanel, &GameEditPanel::gamesChanged, this, [this] {
      FModel->refresh();
      updateCount();
   } );
   connect( FEditPanel, &GameEditPanel::moreInfosRequested, this, &Frm_Editor::showMoreInfos );

   const auto syncGodMode = [this] { FEditPanel->setGodMode( FGodMode, FDelWoPrompt ); };
   connect( Mnu_GodMode, &QAction::toggled, this, syncGodMode );
   connect( Mnu_DeleteWoPrompt, &QAction::toggled, this, syncGodMode );
   connect( Mnu_AutoplayVideo, &QAction::toggled, FEditPanel, &GameEditPanel::setAutoplay );

   // Rows are indices into the Gamelist, and removing one renumbered the rest:
   // reset the model, then land on the neighbour the user was next to.
   connect( FEditPanel, &GameEditPanel::gameDeleted, this, [this] {
      const int row = qMax( 0, Lbx_Games->currentIndex().row() );

      FModel->setGamelist( &FGamelist );
      FScrapePanel->setGamelist( &FGamelist );
      refreshFilter();

      if ( FProxy->rowCount() > 0 )
         Lbx_Games->setCurrentIndex( FProxy->index( qMin( row, FProxy->rowCount() - 1 ), 0 ) );
   } );

   connect( Pgc_Editor, &QTabWidget::currentChanged, this, [this]( int aIndex ) {
      if ( Pgc_Editor->widget( aIndex ) == FScrapePanel )
         FScrapePanel->activate();
   } );

   connect( FScrapePanel, &ScrapePanel::gamesChanged, this, [this] {
      // Reload the edit panel first: refresh() can move the selection, and
      // confirmLeaveEdits() would find the stale widgets dirty against the
      // values just saved.
      FEditPanel->setSelection( selectedGameIndexes() );
      FModel->refresh();
      updateCount();
      Pgc_Editor->setCurrentWidget( FEditPanel );
   } );
}

// Defaults live here now, as the second argument to value(): the ini that used
// to ship them is gone, and so is its migration.
void Frm_Editor::LoadFromIni()
{
   QSettings settings;
   settings.beginGroup( QLatin1String( Cst_IniOptions ) );

   Mnu_GodMode->setChecked( settings.value( QLatin1String( Cst_IniGodMode ), false ).toBool() );
   Mnu_AutoHash->setChecked( settings.value( QLatin1String( Cst_IniAutoHash ), false ).toBool() );
   Mnu_DeleteWoPrompt->setChecked(
      settings.value( QLatin1String( Cst_IniDelWoPrompt ), false ).toBool() );
   Mnu_DeleteWoPrompt->setEnabled( Mnu_GodMode->isChecked() );
   Mnu_ShowTips->setChecked( settings.value( QLatin1String( Cst_ShowTips ), true ).toBool() );
   Mnu_Genesis->setChecked( settings.value( QLatin1String( Cst_IniGenesisLogo ), false ).toBool() );
   Mnu_AutoplayVideo->setChecked(
      settings.value( QLatin1String( Cst_IniAutoplayVideo ), false ).toBool() );
}

void Frm_Editor::SaveToIni()
{
   QSettings settings;
   settings.beginGroup( QLatin1String( Cst_IniOptions ) );

   settings.setValue( QLatin1String( Cst_IniGodMode ), FGodMode );
   settings.setValue( QLatin1String( Cst_IniAutoHash ), FAutoHash );
   // Without god mode there is no delete to skip the prompt for.
   settings.setValue( QLatin1String( Cst_IniDelWoPrompt ), FGodMode && FDelWoPrompt );
   settings.setValue( QLatin1String( Cst_ShowTips ), FShowTips );
   settings.setValue( QLatin1String( Cst_IniGenesisLogo ), FGenesisLogo );
   settings.setValue( QLatin1String( Cst_IniAutoplayVideo ), FAutoplayVideo );
}

void Frm_Editor::showTipsAtStart()
{
   if ( FShowTips )
      Mnu_ShowTips->setChecked( Frm_Help( this ).Execute( true, true ) );
}

void Frm_Editor::closeEvent( QCloseEvent* aEvent )
{
   // FormCloseQuery (4106) never asked, so quitting mid-edit lost the edit.
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

   FRestoringSelection = true;

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

   FRestoringSelection = false;
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

   const QVector<SystemEntry> systems = [this] {
      QVector<SystemEntry> found = scanSystems( FRootPath );

      // A folder the tables do not know can be mapped to a system by hand.
      QSettings settings;
      settings.beginGroup( QLatin1String( Cst_IniSystemMap ) );
      for ( SystemEntry& system : found )
         if ( system.kind == skOther && settings.contains( system.folderName ) )
            system.kind = systemKindFromFolder( settings.value( system.folderName ).toString() );
      return found;
   }();

   // Filling the combo would otherwise load the first system halfway through.
   const QSignalBlocker blocked( Cbx_Systems );
   Cbx_Systems->clear();
   for ( const SystemEntry& system : systems )
      Cbx_Systems->addItem( systemDisplayName( system ), QVariant::fromValue( system ) );

   const bool found = !systems.isEmpty();
   Cbx_Systems->setEnabled( found );
   Cbx_Filter->setEnabled( found );
   Edt_Search->setEnabled( found );
   Mnu_Reload->setEnabled( found );

   if ( !found ) {
      Mnu_AddRoms->setEnabled( false );
      Mnu_LinkMedia->setEnabled( false );
      Mnu_RenameMedia->setEnabled( false );
      Mnu_ShrinkImages->setEnabled( false );
      Mnu_SetSystemType->setEnabled( false );
      FModel->setGamelist( nullptr );
      FEditPanel->setGamelist( nullptr );
      FScrapePanel->setGamelist( nullptr );
      updateCount();
      QMessageBox::information(
         this, QStringLiteral( "Information" ),
         QStringLiteral( "Oops !! It looks like you selected the wrong folder !\n"
                         "Please select the root folder where your systems folders are stored." ) );
      return;
   }

   Cbx_SystemsChange();
}

void Frm_Editor::Cbx_SystemsChange()
{
   if ( !confirmLeaveEdits() ) {
      // Put the combo back on the system the edit belongs to.
      const QSignalBlocker blocked( Cbx_Systems );
      for ( int item = 0; item < Cbx_Systems->count(); ++item ) {
         if ( Cbx_Systems->itemData( item ).value<SystemEntry>().gamelistPath ==
              FGamelist.system().gamelistPath )
            Cbx_Systems->setCurrentIndex( item );
      }
      return;
   }

   FModel->setGamelist( nullptr );
   FEditPanel->setGamelist( nullptr );
   FScrapePanel->setGamelist( nullptr );
   Mnu_AddRoms->setEnabled( false );
   Mnu_LinkMedia->setEnabled( false );
   Mnu_RenameMedia->setEnabled( false );
   Mnu_ShrinkImages->setEnabled( false );
   Mnu_SetSystemType->setEnabled( false );
   Mnu_DeleteOrphans->setEnabled( false );
   Mnu_DeleteDuplicates->setEnabled( false );
   Mnu_ExportList->setEnabled( false );
   const QVariant systemData = Cbx_Systems->currentData();
   if ( !systemData.canConvert<SystemEntry>() ) {
      updateCount();
      return;
   }

   const SystemEntry system = systemData.value<SystemEntry>();
   LoadSystemLogo( system.kind );

   // 964 blanked the search box, whose OnChange then ran the whole list rebuild
   // a second time. Blocked, because the filter is applied below anyway.
   {
      const QSignalBlocker blocked( Edt_Search );
      Edt_Search->clear();
   }

   QString error;
   if ( !FGamelist.load( system, &error ) ) {
      updateCount();
      QMessageBox::warning( this, QStringLiteral( "GameList Editor Qt" ), error );
      return;
   }

   FModel->setGamelist( &FGamelist );
   FEditPanel->setGamelist( &FGamelist );
   FScrapePanel->setGamelist( &FGamelist );
   Mnu_AddRoms->setEnabled( true );
   Mnu_LinkMedia->setEnabled( true );
   Mnu_RenameMedia->setEnabled( true );
   Mnu_ShrinkImages->setEnabled( true );
   Mnu_SetSystemType->setEnabled( true );
   Mnu_DeleteOrphans->setEnabled( true );
   Mnu_DeleteDuplicates->setEnabled( true );
   Mnu_ExportList->setEnabled( true );
   refreshFilter();

   if ( FProxy->rowCount() > 0 )
      Lbx_Games->setCurrentIndex( FProxy->index( 0, 0 ) );
}

void Frm_Editor::addMissingRoms()
{
   if ( !confirmLeaveEdits() )
      return;

   const QStringList chosen =
      Frm_AddRoms( this ).Execute( FGamelist.systemDir(), FGamelist.unlistedRoms() );
   if ( chosen.isEmpty() )
      return;

   const int firstNew = FGamelist.count();
   QString error;
   const int added = FGamelist.addGames( chosen, &error );
   if ( added < 0 ) {
      QMessageBox::warning( this, QStringLiteral( "GameList Editor Qt" ), error );
      return;
   }
   if ( added == 0 )
      return;

   // Rows are indices into the Gamelist; the model only knows how to reset for
   // added games. The panels drop their selection with it.
   FModel->setGamelist( &FGamelist );
   FEditPanel->setGamelist( &FGamelist );
   FScrapePanel->setGamelist( &FGamelist );
   refreshFilter();

   // A filter can hide the new game, so there may be no row to land on.
   const QModelIndex first = FProxy->mapFromSource( FModel->index( firstNew ) );
   if ( first.isValid() )
      Lbx_Games->setCurrentIndex( first );
}

void Frm_Editor::linkExistingMedia()
{
   if ( !confirmLeaveEdits() )
      return;

   const QVector<MediaLink> candidates = FGamelist.unlinkedMedia();
   if ( candidates.isEmpty() ) {
      QMessageBox::information( this, QStringLiteral( "GameList Editor Qt" ),
                                QStringLiteral( "No unlinked picture or video found." ) );
      return;
   }

   const QVector<MediaLink> chosen = Frm_LinkMedia( this ).Execute( FGamelist, candidates );
   if ( chosen.isEmpty() )
      return;

   QString error;
   if ( FGamelist.linkMedia( chosen, &error ) < 0 ) {
      QMessageBox::warning( this, QStringLiteral( "GameList Editor Qt" ), error );
      return;
   }

   // Same refresh as after a scrape: the games are the same, only their media changed.
   FEditPanel->setSelection( selectedGameIndexes() );
   FModel->refresh();
   updateCount();
}

void Frm_Editor::renameMediaAfterRoms()
{
   if ( !confirmLeaveEdits() )
      return;

   // Strict is the superset: if it finds nothing, no checkbox state can.
   if ( FGamelist.misnamedMedia( true ).isEmpty() ) {
      QMessageBox::information( this, QStringLiteral( "GameList Editor Qt" ),
                                QStringLiteral( "Every picture and video is named after its ROM." ) );
      return;
   }

   const QVector<MediaRename> chosen = Frm_LinkMedia( this ).ExecuteRename( FGamelist );
   if ( chosen.isEmpty() )
      return;

   QString error;
   if ( FGamelist.renameMedia( chosen, &error ) < 0 ) {
      QMessageBox::warning( this, QStringLiteral( "GameList Editor Qt" ), error );
      return;
   }

   FEditPanel->setSelection( selectedGameIndexes() );
   FModel->refresh();
   updateCount();
}

void Frm_Editor::shrinkLargeImages()
{
   if ( !confirmLeaveEdits() )
      return;

   bool ok = false;
   const double megabytes = QInputDialog::getDouble(
      this, QStringLiteral( "Shrink large pictures" ),
      QStringLiteral( "Shrink pictures larger than (MB):" ), 1.0, 0.1, 100.0, 1, &ok );
   if ( !ok )
      return;
   const qint64 maxBytes = qRound64( megabytes * 1024 * 1024 );

   const QVector<OversizedImage> candidates = FGamelist.oversizedImages( maxBytes );
   if ( candidates.isEmpty() ) {
      QMessageBox::information( this, QStringLiteral( "GameList Editor Qt" ),
                                QStringLiteral( "No picture is over that size." ) );
      return;
   }

   const QVector<OversizedImage> chosen =
      Frm_LinkMedia( this ).ExecuteShrink( FGamelist, maxBytes, candidates );
   if ( chosen.isEmpty() )
      return;

   QProgressDialog progress( QStringLiteral( "Shrinking pictures..." ), QStringLiteral( "Cancel" ), 0,
                             static_cast<int>( chosen.size() ), this );
   progress.setWindowModality( Qt::WindowModal );
   progress.setMinimumDuration( 0 );

   int shrunk = 0;
   qint64 saved = 0;
   QStringList problems;
   for ( int row = 0; row < chosen.size(); ++row ) {
      progress.setValue( row );
      if ( progress.wasCanceled() )
         break;

      QString error;
      const qint64 size = shrinkImageFile( chosen.at( row ).path, maxBytes, &error );
      if ( size < 0 ) {
         problems << error;
         continue;
      }
      ++shrunk;
      saved += chosen.at( row ).size - size;
   }
   progress.setValue( static_cast<int>( chosen.size() ) );

   QString text = QStringLiteral( "%1 picture(s) shrunk, %2 saved." )
                     .arg( shrunk )
                     .arg( QLocale().formattedDataSize( saved, 1, QLocale::DataSizeTraditionalFormat ) );
   if ( !problems.isEmpty() )
      text += QStringLiteral( "\n\nNot shrunk:\n%1" ).arg( problems.join( QLatin1Char( '\n' ) ) );
   QMessageBox::information( this, QStringLiteral( "GameList Editor Qt" ), text );

   // The links are unchanged, but the preview may be showing a file that just changed.
   FEditPanel->setSelection( selectedGameIndexes() );
}

void Frm_Editor::deleteOrphans()
{
   if ( confirmLeaveEdits() )
      confirmAndRemove( FGamelist.orphanIndexes(), QStringLiteral( "whose ROM is missing" ) );
}

void Frm_Editor::deleteDuplicates()
{
   if ( confirmLeaveEdits() )
      confirmAndRemove( FGamelist.duplicateIndexes(),
                        QStringLiteral( "that list a ROM already listed above them (the first stays)" ) );
}

void Frm_Editor::confirmAndRemove( const QVector<int>& aIndexes, const QString& aWhat )
{
   if ( aIndexes.isEmpty() ) {
      QMessageBox::information( this, QStringLiteral( "GameList Editor Qt" ),
                                QStringLiteral( "Nothing to delete." ) );
      return;
   }

   const auto answer = QMessageBox::question(
      this, QStringLiteral( "GameList Editor Qt" ),
      QStringLiteral( "Remove %1 game(s) %2 from the gamelist?\n\nNo ROM, picture or video file is deleted." )
         .arg( aIndexes.size() )
         .arg( aWhat ),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No );
   if ( answer != QMessageBox::Yes )
      return;

   QString error;
   if ( !FGamelist.removeGames( aIndexes, &error ) ) {
      QMessageBox::warning( this, QStringLiteral( "GameList Editor Qt" ), error );
      return;
   }

   // Indices shifted: same reset as after adding games.
   FModel->setGamelist( &FGamelist );
   FEditPanel->setGamelist( &FGamelist );
   FScrapePanel->setGamelist( &FGamelist );
   refreshFilter();
   if ( FProxy->rowCount() > 0 )
      Lbx_Games->setCurrentIndex( FProxy->index( 0, 0 ) );
}

void Frm_Editor::exportGamesList()
{
   const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral( "Export games list" ),
      QDir( FGamelist.systemDir() ).filePath( FGamelist.system().folderName + QStringLiteral( ".txt" ) ),
      QStringLiteral( "Text files (*.txt)" ) );
   if ( path.isEmpty() )
      return;

   // What the list shows, in the order it shows it: filter, search and the
   // ROM / name toggles all apply.
   QByteArray content;
   for ( int row = 0; row < FProxy->rowCount(); ++row )
      content += FProxy->index( row, 0 ).data().toString().toUtf8() + '\n';

   QSaveFile file( path );
   if ( !file.open( QIODevice::WriteOnly ) || file.write( content ) != content.size() ||
        !file.commit() ) {
      QMessageBox::warning( this, QStringLiteral( "GameList Editor Qt" ),
                            QStringLiteral( "Cannot write %1: %2" ).arg( path, file.errorString() ) );
      return;
   }

   QMessageBox::information( this, QStringLiteral( "GameList Editor Qt" ),
                             QStringLiteral( "%1 game(s) exported." ).arg( FProxy->rowCount() ) );
}

void Frm_Editor::LoadSystemLogo( SystemKind aKind )
{
   const SystemKind kind = ( aKind == skMegaDrive && FGenesisLogo ) ? skGenesis : aKind;

   // LoadSystemLogo had no FileExists guard and no except, so a missing logo
   // raised. QPixmap says so in its return value instead.
   if ( !FSystemLogo.load( QLatin1String( Cst_LogoPicsFolder ) +
                           QLatin1String( Cst_SystemKindImageNames[kind] ) ) ) {
      FSystemLogo = QPixmap();
      Img_System->clear();
      return;
   }

   scaleSystemLogo();
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

QString Frm_Editor::systemDisplayName( const SystemEntry& aSystem ) const
{
   // skOther is every folder no table knows, so its own name is all there is.
   if ( aSystem.kind == skOther )
      return aSystem.folderName;

   QString name = QLatin1String( Cst_SystemKindStr[aSystem.kind] );
   if ( aSystem.kind == skMegaDrive && FGenesisLogo )
      name = QLatin1String( Cst_SystemKindStr[skGenesis] );

   // A mapped folder ("amiga500" treated as Amiga) keeps its own name visible.
   if ( aSystem.folderName != QLatin1String( Cst_SystemKindFolderNames[aSystem.kind] ) )
      name += QStringLiteral( " (%1)" ).arg( aSystem.folderName );
   return name;
}

void Frm_Editor::setSystemType()
{
   const int item = Cbx_Systems->currentIndex();
   if ( item < 0 )
      return;

   SystemEntry system = Cbx_Systems->itemData( item ).value<SystemEntry>();

   // The mapping is only consulted for folders the tables do not know, so for a
   // known one it would be saved and then silently ignored.
   if ( systemKindFromFolder( system.folderName ) != skOther ) {
      QMessageBox::information( this, QStringLiteral( "GameList Editor Qt" ),
                                QStringLiteral( "This folder name is already a known system." ) );
      return;
   }

   // The table names repeat (two "Colecovision", two "PC Engine"), so each entry
   // carries its ES folder name, which is unique.
   QVector<QPair<QString, int>> choices;
   for ( int kind = 0; kind < skOther; ++kind )
      choices.append( { QStringLiteral( "%1 (%2)" )
                           .arg( QLatin1String( Cst_SystemKindStr[kind] ),
                                 QLatin1String( Cst_SystemKindFolderNames[kind] ) ),
                        kind } );
   std::sort( choices.begin(), choices.end(), []( const auto& a, const auto& b ) {
      return a.first.compare( b.first, Qt::CaseInsensitive ) < 0;
   } );

   QStringList labels{ QStringLiteral( "Other (no mapping)" ) };
   int current = 0;
   for ( int i = 0; i < choices.size(); ++i ) {
      labels.append( choices[i].first );
      if ( choices[i].second == system.kind )
         current = i + 1;
   }

   bool ok = false;
   const QString chosen = QInputDialog::getItem(
      this, QStringLiteral( "Set system type" ),
      QStringLiteral( "Treat the folder \"%1\" as:" ).arg( system.folderName ), labels, current,
      false, &ok );
   if ( !ok )
      return;

   const int picked = static_cast<int>( labels.indexOf( chosen ) );
   const SystemKind kind = picked <= 0 ? skOther : static_cast<SystemKind>( choices[picked - 1].second );

   QSettings settings;
   settings.beginGroup( QLatin1String( Cst_IniSystemMap ) );
   if ( kind == skOther )
      settings.remove( system.folderName );
   else
      settings.setValue( system.folderName, QLatin1String( Cst_SystemKindFolderNames[kind] ) );

   system.kind = kind;
   Cbx_Systems->setItemData( item, QVariant::fromValue( system ) );
   Cbx_Systems->setItemText( item, systemDisplayName( system ) );
   FGamelist.setSystemKind( kind );
   LoadSystemLogo( kind );
}

void Frm_Editor::refreshFilter()
{
   if ( FIsLoading )
      return;

   FIsLoading = true;

   FilterSpec filter;
   filter.index = qMax( 0, Cbx_Filter->currentIndex() );
   filter.search = Edt_Search->text();
   filter.listByRom = Chk_ListByRom->isChecked();
   filter.fullRomName = Chk_FullRomName->isChecked();
   filter.reference = currentGameIndex();

   FModel->setFilter( filter );
   updateCount();

   FIsLoading = false;
}

void Frm_Editor::updateCount()
{
   const int shown = FProxy->rowCount();

   Lbl_NbGamesFound->setText(
      Cbx_Filter->currentIndex() <= 0
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
         FAutoHash ||
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
