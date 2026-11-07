#include "F_Sync.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

QString size( qint64 aBytes )
{
   return QLocale().formattedDataSize( aBytes, 1, QLocale::DataSizeTraditionalFormat );
}

QString systemName( const SystemPlan& aSystem )
{
   const QString source = aSystem.pair.source.folderName;
   const QString target = aSystem.pair.target.folderName;
   return source == target ? source : QStringLiteral( "%1 %2 %3" ).arg( source, QString( QChar( 0x2192 ) ), target );
}

QString describe( const SystemPlan& aSystem )
{
   int added = 0;
   int updated = 0;
   int media = 0;
   int romFiles = 0;
   for ( const SyncGame& game : aSystem.games ) {
      ++( game.targetIndex < 0 ? added : updated );
      media += !game.imageSource.isEmpty() + !game.videoSource.isEmpty();
      romFiles += static_cast<int>( game.rom.files.size() );
   }

   QStringList parts;
   if ( added )
      parts << QStringLiteral( "%1 new" ).arg( added );
   if ( updated )
      parts << QStringLiteral( "%1 updated" ).arg( updated );
   if ( romFiles )
      parts << QStringLiteral( "%1 ROM files (%2)" ).arg( romFiles ).arg( size( aSystem.romBytes ) );
   if ( media )
      parts << QStringLiteral( "%1 media files" ).arg( media );

   QString text = QStringLiteral( "%1: %2" ).arg( systemName( aSystem ), parts.isEmpty() ? QStringLiteral( "nothing to do" ) : parts.join( QStringLiteral( ", " ) ) );
   if ( aSystem.createTarget )
      text += QStringLiteral( " (new gamelist)" );
   return text;
}

// The names of the fields in aFields, in the editor's order.
QString fieldNames( GameFieldSet aFields )
{
   static const struct
   {
      GameField field;
      const char* name;
   } Cst_Names[] = { { gfName, "name" },           { gfGenre, "genre" },
                     { gfRating, "rating" },       { gfPlayers, "players" },
                     { gfDeveloper, "developer" }, { gfPublisher, "publisher" },
                     { gfReleaseDate, "release date" }, { gfDescription, "description" },
                     { gfRegion, "region" } };

   QStringList names;
   for ( const auto& entry : Cst_Names ) {
      if ( aFields.testFlag( entry.field ) )
         names << QLatin1String( entry.name );
   }
   return names.join( QStringLiteral( ", " ) );
}

QString gameLine( const SyncGame& aGame )
{
   QStringList parts;
   if ( aGame.fields )
      parts << QStringLiteral( "fields: %1" ).arg( fieldNames( aGame.fields ) );
   if ( !aGame.rom.files.isEmpty() )
      parts << QStringLiteral( "ROM: %1 file(s), %2" ).arg( aGame.rom.files.size() ).arg( size( aGame.rom.bytes ) );
   if ( !aGame.imageSource.isEmpty() )
      parts << QStringLiteral( "cover: %1" ).arg( QFileInfo( aGame.imageSource ).fileName() );
   if ( !aGame.videoSource.isEmpty() )
      parts << QStringLiteral( "video: %1" ).arg( QFileInfo( aGame.videoSource ).fileName() );
   return QStringLiteral( "   %1%2" ).arg( aGame.name, parts.isEmpty() ? QString() : QStringLiteral( "  [%1]" ).arg( parts.join( QStringLiteral( "; " ) ) ) );
}

// The problems stay in the review; the long lists of kept games are a count there.
QString notes( const SyncPlan& aPlan )
{
   QStringList lines;
   for ( const SystemPlan& system : aPlan.systems ) {
      const QString name = systemName( system );
      if ( !system.targetOnly.isEmpty() )
         lines << QStringLiteral( "%1: %2 game(s) only on the target, kept." ).arg( name ).arg( system.targetOnly.size() );
      for ( const QString& problem : system.problems )
         lines << QStringLiteral( "%1: %2" ).arg( name, problem );
   }
   return lines.join( QLatin1Char( '\n' ) );
}

}  // namespace

QString syncDetails( const SystemPlan& aSystem )
{
   QStringList added;
   QStringList updated;
   for ( const SyncGame& game : aSystem.games )
      ( game.targetIndex < 0 ? added : updated ) << gameLine( game );

   const auto section = [&]( const QString& aTitle, const QStringList& aLines ) {
      return aLines.isEmpty() ? QString() : QStringLiteral( "\n%1 (%2):\n%3\n" ).arg( aTitle ).arg( aLines.size() ).arg( aLines.join( QLatin1Char( '\n' ) ) );
   };
   const auto indented = []( const QStringList& aLines ) {
      QStringList result;
      for ( const QString& line : aLines )
         result << QStringLiteral( "   " ) + line;
      return result;
   };

   QString text = QStringLiteral( "%1\nTarget gamelist: %2%3\nROM files are %4.\n" )
                     .arg( systemName( aSystem ), aSystem.pair.target.gamelistPath,
                           aSystem.createTarget ? QStringLiteral( " (will be created)" ) : QString(),
                           aSystem.copyRoms ? QStringLiteral( "copied" ) : QStringLiteral( "not copied: the ROM folder is shared" ) );
   text += section( QStringLiteral( "New games" ), added );
   text += section( QStringLiteral( "Updated games" ), updated );
   text += QStringLiteral( "\nUnchanged: %1\n" ).arg( aSystem.unchanged );
   text += section( QStringLiteral( "Only on the target, kept" ), indented( aSystem.targetOnly ) );
   text += section( QStringLiteral( "Problems" ), indented( aSystem.problems ) );
   return text;
}

Frm_Sync::Frm_Sync( QWidget* aParent ) : QDialog( aParent )
{
   setWindowTitle( QStringLiteral( "Sync to other folder" ) );
   resize( 680, 480 );

   QLabel* intro = new QLabel(
      QStringLiteral( "Stop EmulationStation on the target device before you start: it rewrites its "
                      "gamelists on exit and would undo this.\nGames only on the target are kept. "
                      "Favorites, hidden flags and play counts stay as they are on each device." ),
      this );
   intro->setWordWrap( true );

   Lst_Systems = new QTreeWidget( this );
   Lst_Systems->setObjectName( QStringLiteral( "Lst_Systems" ) );
   Lst_Systems->setColumnCount( 2 );
   Lst_Systems->setHeaderHidden( true );
   Lst_Systems->setRootIsDecorated( false );
   Lst_Systems->header()->setStretchLastSection( false );
   Lst_Systems->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
   Lst_Systems->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );

   Txt_Notes = new QPlainTextEdit( this );
   Txt_Notes->setObjectName( QStringLiteral( "Txt_Notes" ) );
   Txt_Notes->setReadOnly( true );

   QDialogButtonBox* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
   Btn_Start = buttons->button( QDialogButtonBox::Ok );
   Btn_Start->setText( QStringLiteral( "Start" ) );
   connect( buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
   connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
   connect( Lst_Systems, &QTreeWidget::itemChanged, this, &Frm_Sync::updateStart );

   QVBoxLayout* layout = new QVBoxLayout( this );
   layout->addWidget( intro );
   layout->addWidget( Lst_Systems, 1 );
   layout->addWidget( new QLabel( QStringLiteral( "Problems and kept games:" ), this ) );
   layout->addWidget( Txt_Notes, 1 );
   layout->addWidget( buttons );
}

void Frm_Sync::updateStart()
{
   bool any = false;
   for ( int row = 0; row < Lst_Systems->topLevelItemCount(); ++row )
      any = any || Lst_Systems->topLevelItem( row )->checkState( 0 ) == Qt::Checked;
   Btn_Start->setEnabled( any );
}

void Frm_Sync::showDetails( const SystemPlan& aSystem )
{
   QDialog window( this );
   window.setWindowTitle( QStringLiteral( "Details: %1" ).arg( systemName( aSystem ) ) );
   window.resize( 640, 480 );

   QPlainTextEdit* text = new QPlainTextEdit( syncDetails( aSystem ), &window );
   text->setObjectName( QStringLiteral( "Txt_Details" ) );
   text->setReadOnly( true );
   text->setLineWrapMode( QPlainTextEdit::NoWrap );

   QDialogButtonBox* buttons = new QDialogButtonBox( QDialogButtonBox::Close, &window );
   connect( buttons, &QDialogButtonBox::rejected, &window, &QDialog::reject );

   QVBoxLayout* layout = new QVBoxLayout( &window );
   layout->addWidget( text, 1 );
   layout->addWidget( buttons );
   window.exec();
}

SyncPlan Frm_Sync::Execute( const SyncPlan& aPlan )
{
   Lst_Systems->clear();
   for ( int row = 0; row < aPlan.systems.size(); ++row ) {
      const SystemPlan& system = aPlan.systems.at( row );
      QTreeWidgetItem* item = new QTreeWidgetItem( Lst_Systems );
      item->setText( 0, describe( system ) );
      item->setData( 0, Qt::UserRole, row );
      if ( system.isEmpty() ) {
         item->setFlags( item->flags() & ~( Qt::ItemIsEnabled | Qt::ItemIsSelectable ) );
         item->setCheckState( 0, Qt::Unchecked );
      } else {
         item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
         item->setCheckState( 0, Qt::Checked );
      }

      // Not through the item, so a system with nothing to do can still be looked at.
      QPushButton* details = new QPushButton( QStringLiteral( "Details..." ), Lst_Systems );
      details->setObjectName( QStringLiteral( "Btn_Details" ) );
      connect( details, &QPushButton::clicked, this, [this, &aPlan, row] { showDetails( aPlan.systems.at( row ) ); } );
      Lst_Systems->setItemWidget( item, 1, details );
   }
   Txt_Notes->setPlainText( notes( aPlan ) );
   updateStart();

   if ( exec() != QDialog::Accepted )
      return {};

   SyncPlan chosen;
   for ( int row = 0; row < Lst_Systems->topLevelItemCount(); ++row ) {
      const QTreeWidgetItem* item = Lst_Systems->topLevelItem( row );
      if ( item->checkState( 0 ) == Qt::Checked )
         chosen.systems.append( aPlan.systems.at( item->data( 0, Qt::UserRole ).toInt() ) );
   }
   return chosen;
}
