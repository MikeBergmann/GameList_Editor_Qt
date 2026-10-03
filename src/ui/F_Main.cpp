#include "F_Main.h"

#include "GamelistModel.h"
#include "Resources.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMessageLogger>
#include <QPixmap>
#include <QSettings>
#include <QVBoxLayout>

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
QMenu* addPendingMenu( QMenu* aParent, const QString& aTitle, const QStringList& aItems )
{
   QMenu* menu = aParent->addMenu( aTitle );
   for ( const QString& item : aItems )
      menu->addAction( item );
   menu->setEnabled( false );
   return menu;
}

}  // namespace

Frm_Editor::Frm_Editor( QWidget* aParent ) : QMainWindow( aParent )
{
   setWindowTitle( QStringLiteral( "GameList Editor" ) );

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
   Mnu_System = addPendingMenu( actions, QStringLiteral( "System" ),
                                { QStringLiteral( "Convert all text to lowercase" ),
                                  QStringLiteral( "Convert all text to uppercase" ),
                                  QStringLiteral( "Remove region from games names" ),
                                  QStringLiteral( "Delete orphans from gamelist" ),
                                  QStringLiteral( "Delete duplicates from gamelist" ),
                                  QStringLiteral( "Export games list to txt file" ) } );
   Mnu_Game = addPendingMenu( actions, QStringLiteral( "Game" ),
                              { QStringLiteral( "Convert all text to lowercase" ),
                                QStringLiteral( "Convert all text to uppercase" ) } );
   Mnu_Selection = addPendingMenu( actions, QStringLiteral( "Selection" ),
                                   { QStringLiteral( "Add to Hidden" ),
                                     QStringLiteral( "Remove from Hidden" ),
                                     QStringLiteral( "Add to Favorites" ),
                                     QStringLiteral( "Remove from Favorites" ),
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

}

void Frm_Editor::buildCentralWidget()
{
   QWidget* central = new QWidget( this );
   QHBoxLayout* columns = new QHBoxLayout( central );

   QVBoxLayout* left = new QVBoxLayout;
   columns->addLayout( left );

   Img_System = new QLabel( central );
   Img_System->setAlignment( Qt::AlignCenter );
   Img_System->setMinimumHeight( 70 );
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
}


void Frm_Editor::closeEvent( QCloseEvent* aEvent )
{
   SaveToIni();
   QMainWindow::closeEvent( aEvent );
}

void Frm_Editor::BuildSystemsList( bool aReload )
{
   if ( aReload ) {
      openRootFolder( FRootPath );
      return;
   }

   const QString root = QFileDialog::getExistingDirectory(
      this, QStringLiteral( "Select the folder that holds your system folders" ), FRootPath );
   if ( root.isEmpty() )
      return;

   openRootFolder( root );
}

void Frm_Editor::openRootFolder( const QString& aRootPath )
{
   FRootPath = aRootPath;

   const QVector<SystemEntry> systems = scanSystems( FRootPath );

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
      FModel->setGamelist( nullptr );
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
   FModel->setGamelist( nullptr );

   const QVariant data = Cbx_Systems->currentData();
   if ( !data.canConvert<SystemEntry>() ) {
      updateCount();
      return;
   }

   const SystemEntry system = data.value<SystemEntry>();
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
      QMessageBox::warning( this, QStringLiteral( "GameList Editor" ), error );
      return;
   }

   FModel->setGamelist( &FGamelist );
   refreshFilter();

   if ( FProxy->rowCount() > 0 )
      Lbx_Games->setCurrentIndex( FProxy->index( 0, 0 ) );
}

void Frm_Editor::LoadSystemLogo( SystemKind aKind )
{
   const SystemKind kind = ( aKind == skMegaDrive && FGenesisLogo ) ? skGenesis : aKind;

   // LoadSystemLogo had no FileExists guard and no except, so a missing logo
   // raised. QPixmap says so in its return value instead.
   QPixmap logo;
   if ( !logo.load( QLatin1String( Cst_LogoPicsFolder ) +
                     QLatin1String( Cst_SystemKindImageNames[kind] ) ) ) {
      Img_System->clear();
      return;
   }

   Img_System->setPixmap( logo.scaledToHeight( 64, Qt::SmoothTransformation ) );
}

QString Frm_Editor::systemDisplayName( const SystemEntry& aSystem ) const
{
   // skOther is every folder no table knows, so its own name is all there is.
   if ( aSystem.kind == skOther )
      return aSystem.folderName;

   if ( aSystem.kind == skMegaDrive && FGenesisLogo )
      return QLatin1String( Cst_SystemKindStr[skGenesis] );

   return QLatin1String( Cst_SystemKindStr[aSystem.kind] );
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

