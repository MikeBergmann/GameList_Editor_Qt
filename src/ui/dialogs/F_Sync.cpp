#include "F_Sync.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

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
      media += game.copyImage + game.copyVideo;
      romFiles += static_cast<int>( game.rom.files.size() );
   }

   QStringList parts;
   if ( added )
      parts << QStringLiteral( "%1 new" ).arg( added );
   if ( updated )
      parts << QStringLiteral( "%1 updated" ).arg( updated );
   if ( romFiles )
      parts << QStringLiteral( "%1 ROM files (%2)" )
                  .arg( romFiles )
                  .arg( QLocale().formattedDataSize( aSystem.romBytes, 1, QLocale::DataSizeTraditionalFormat ) );
   if ( media )
      parts << QStringLiteral( "%1 media files" ).arg( media );

   QString text = QStringLiteral( "%1: %2" ).arg( systemName( aSystem ), parts.isEmpty() ? QStringLiteral( "nothing to do" ) : parts.join( QStringLiteral( ", " ) ) );
   if ( aSystem.createTarget )
      text += QStringLiteral( " (new gamelist)" );
   return text;
}

QString notes( const SyncPlan& aPlan )
{
   QStringList lines;
   for ( const SystemPlan& system : aPlan.systems ) {
      const QString name = systemName( system );
      if ( !system.targetOnly.isEmpty() )
         lines << QStringLiteral( "%1: only on the target, kept: %2" ).arg( name, system.targetOnly.join( QStringLiteral( ", " ) ) );
      for ( const QString& problem : system.problems )
         lines << QStringLiteral( "%1: %2" ).arg( name, problem );
   }
   return lines.join( QLatin1Char( '\n' ) );
}

}  // namespace

Frm_Sync::Frm_Sync( QWidget* aParent ) : QDialog( aParent )
{
   setWindowTitle( QStringLiteral( "Sync to other folder" ) );
   resize( 640, 480 );

   QLabel* intro = new QLabel(
      QStringLiteral( "Stop EmulationStation on the target device before you start: it rewrites its "
                      "gamelists on exit and would undo this.\nGames only on the target are kept. "
                      "Favorites, hidden flags and play counts stay as they are on each device." ),
      this );
   intro->setWordWrap( true );

   Lst_Systems = new QListWidget( this );
   Lst_Systems->setObjectName( QStringLiteral( "Lst_Systems" ) );

   Txt_Notes = new QPlainTextEdit( this );
   Txt_Notes->setObjectName( QStringLiteral( "Txt_Notes" ) );
   Txt_Notes->setReadOnly( true );

   QDialogButtonBox* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
   Btn_Start = buttons->button( QDialogButtonBox::Ok );
   Btn_Start->setText( QStringLiteral( "Start" ) );
   connect( buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
   connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
   connect( Lst_Systems, &QListWidget::itemChanged, this, &Frm_Sync::updateStart );

   QVBoxLayout* layout = new QVBoxLayout( this );
   layout->addWidget( intro );
   layout->addWidget( Lst_Systems, 1 );
   layout->addWidget( new QLabel( QStringLiteral( "Left alone and problems:" ), this ) );
   layout->addWidget( Txt_Notes, 1 );
   layout->addWidget( buttons );
}

void Frm_Sync::updateStart()
{
   bool any = false;
   for ( int row = 0; row < Lst_Systems->count(); ++row )
      any = any || Lst_Systems->item( row )->checkState() == Qt::Checked;
   Btn_Start->setEnabled( any );
}

SyncPlan Frm_Sync::Execute( const SyncPlan& aPlan )
{
   Lst_Systems->clear();
   for ( int row = 0; row < aPlan.systems.size(); ++row ) {
      const SystemPlan& system = aPlan.systems.at( row );
      QListWidgetItem* item = new QListWidgetItem( describe( system ), Lst_Systems );
      item->setData( Qt::UserRole, row );
      if ( system.isEmpty() ) {
         item->setFlags( item->flags() & ~( Qt::ItemIsEnabled | Qt::ItemIsSelectable ) );
         item->setCheckState( Qt::Unchecked );
      } else {
         item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
         item->setCheckState( Qt::Checked );
      }
   }
   Txt_Notes->setPlainText( notes( aPlan ) );
   updateStart();

   if ( exec() != QDialog::Accepted )
      return {};

   SyncPlan chosen;
   for ( int row = 0; row < Lst_Systems->count(); ++row ) {
      const QListWidgetItem* item = Lst_Systems->item( row );
      if ( item->checkState() == Qt::Checked )
         chosen.systems.append( aPlan.systems.at( item->data( Qt::UserRole ).toInt() ) );
   }
   return chosen;
}
