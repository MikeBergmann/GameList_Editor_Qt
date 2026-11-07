#include "SystemActions.h"

#include "F_AddRoms.h"
#include "F_LinkMedia.h"
#include "F_Sync.h"
#include "Gamelist.h"
#include "MediaOps.h"
#include "PathUtil.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLocale>
#include <QMessageBox>
#include <QProgressDialog>

namespace {

// Drops the games from the gamelist only; no ROM, picture or video file is touched.
ViewUpdate confirmAndRemove( QWidget* aParent, Gamelist& aGamelist, const QVector<int>& aIndexes,
                             const QString& aWhat )
{
   if ( aIndexes.isEmpty() ) {
      QMessageBox::information( aParent, QStringLiteral( "GameList Editor Qt" ),
                                QStringLiteral( "Nothing to delete." ) );
      return ViewUpdate::None;
   }

   const auto answer = QMessageBox::question(
      aParent, QStringLiteral( "GameList Editor Qt" ),
      QStringLiteral( "Remove %1 game(s) %2 from the gamelist?\n\nNo ROM, picture or video file is deleted." )
         .arg( aIndexes.size() )
         .arg( aWhat ),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No );
   if ( answer != QMessageBox::Yes )
      return ViewUpdate::None;

   if ( const Result<> removed = aGamelist.removeGames( aIndexes ); !removed ) {
      QMessageBox::warning( aParent, QStringLiteral( "GameList Editor Qt" ), removed.error() );
      return ViewUpdate::None;
   }

   return ViewUpdate::Reset;
}

}  // namespace

ViewUpdate addMissingRoms( QWidget* aParent, Gamelist& aGamelist )
{
   const QStringList chosen =
      Frm_AddRoms( aParent ).Execute( aGamelist.romDir(), unlistedRoms( aGamelist ) );
   if ( chosen.isEmpty() )
      return ViewUpdate::None;

   const Result<int> added = aGamelist.addGames( chosen );
   if ( !added ) {
      QMessageBox::warning( aParent, QStringLiteral( "GameList Editor Qt" ), added.error() );
      return ViewUpdate::None;
   }

   // Rows are indices into the Gamelist; the model only knows how to reset for added games.
   return added.value() == 0 ? ViewUpdate::None : ViewUpdate::Reset;
}

// ES-DE names media after the ROM, so linking and renaming have nothing to do.
static bool esdeNotice( QWidget* aParent, const Gamelist& aGamelist )
{
   if ( aGamelist.conventionalMedia() )
      QMessageBox::information( aParent, QStringLiteral( "GameList Editor Qt" ),
                                QStringLiteral( "ES-DE finds media by the ROM's name, so there is nothing to do here." ) );
   return aGamelist.conventionalMedia();
}

ViewUpdate linkExistingMedia( QWidget* aParent, Gamelist& aGamelist )
{
   if ( esdeNotice( aParent, aGamelist ) )
      return ViewUpdate::None;

   const QVector<MediaLink> candidates = unlinkedMedia( aGamelist );
   if ( candidates.isEmpty() ) {
      QMessageBox::information( aParent, QStringLiteral( "GameList Editor Qt" ),
                                QStringLiteral( "No unlinked picture or video found." ) );
      return ViewUpdate::None;
   }

   const QVector<MediaLink> chosen = Frm_LinkMedia( aParent ).Execute( aGamelist, candidates );
   if ( chosen.isEmpty() )
      return ViewUpdate::None;

   if ( const Result<int> linked = aGamelist.linkMedia( chosen ); !linked ) {
      QMessageBox::warning( aParent, QStringLiteral( "GameList Editor Qt" ), linked.error() );
      return ViewUpdate::None;
   }

   // Same games, only their media changed.
   return ViewUpdate::Refresh;
}

ViewUpdate renameMediaAfterRoms( QWidget* aParent, Gamelist& aGamelist )
{
   if ( esdeNotice( aParent, aGamelist ) )
      return ViewUpdate::None;

   // Strict is the superset: if it finds nothing, no checkbox state can.
   if ( misnamedMedia( aGamelist, true ).isEmpty() ) {
      QMessageBox::information( aParent, QStringLiteral( "GameList Editor Qt" ),
                                QStringLiteral( "Every picture and video is named after its ROM." ) );
      return ViewUpdate::None;
   }

   const QVector<MediaRename> chosen = Frm_LinkMedia( aParent ).ExecuteRename( aGamelist );
   if ( chosen.isEmpty() )
      return ViewUpdate::None;

   if ( const Result<int> renamed = aGamelist.renameMedia( chosen ); !renamed ) {
      QMessageBox::warning( aParent, QStringLiteral( "GameList Editor Qt" ), renamed.error() );
      return ViewUpdate::None;
   }

   return ViewUpdate::Refresh;
}

ViewUpdate shrinkLargeImages( QWidget* aParent, Gamelist& aGamelist )
{
   bool ok = false;
   const double megabytes = QInputDialog::getDouble(
      aParent, QStringLiteral( "Shrink large pictures" ),
      QStringLiteral( "Shrink pictures larger than (MB):" ), 1.0, 0.1, 100.0, 1, &ok );
   if ( !ok )
      return ViewUpdate::None;
   const qint64 maxBytes = qRound64( megabytes * 1024 * 1024 );

   const QVector<OversizedImage> candidates = oversizedImages( aGamelist, maxBytes );
   if ( candidates.isEmpty() ) {
      QMessageBox::information( aParent, QStringLiteral( "GameList Editor Qt" ),
                                QStringLiteral( "No picture is over that size." ) );
      return ViewUpdate::None;
   }

   const QVector<OversizedImage> chosen =
      Frm_LinkMedia( aParent ).ExecuteShrink( aGamelist, maxBytes, candidates );
   if ( chosen.isEmpty() )
      return ViewUpdate::None;

   QProgressDialog progress( QStringLiteral( "Shrinking pictures..." ), QStringLiteral( "Cancel" ), 0,
                             static_cast<int>( chosen.size() ), aParent );
   progress.setWindowModality( Qt::WindowModal );
   progress.setMinimumDuration( 0 );

   const ShrinkReport report = shrinkImages( chosen, maxBytes, [&progress]( int aDone ) {
      progress.setValue( aDone );
      return !progress.wasCanceled();
   } );
   progress.setValue( static_cast<int>( chosen.size() ) );

   QString text = QStringLiteral( "%1 picture(s) shrunk, %2 saved." )
                     .arg( report.shrunk )
                     .arg( QLocale().formattedDataSize( report.saved, 1, QLocale::DataSizeTraditionalFormat ) );
   if ( !report.problems.isEmpty() )
      text += QStringLiteral( "\n\nNot shrunk:\n%1" ).arg( report.problems.join( QLatin1Char( '\n' ) ) );
   QMessageBox::information( aParent, QStringLiteral( "GameList Editor Qt" ), text );

   // The links are unchanged, but the editor may be showing a picture that just changed.
   return ViewUpdate::Refresh;
}

std::optional<SystemKind> chooseSystemKind( QWidget* aParent, const SystemEntry& aSystem )
{
   // The mapping is only consulted for folders the tables do not know, so for a
   // known one it would be saved and then silently ignored.
   if ( systemKindFromFolder( aSystem.folderName ) != skOther ) {
      QMessageBox::information( aParent, QStringLiteral( "GameList Editor Qt" ),
                                QStringLiteral( "This folder name is already a known system." ) );
      return std::nullopt;
   }

   const QVector<SystemKindChoice> choices = systemKindChoices();
   QStringList labels{ QStringLiteral( "Other (no mapping)" ) };
   int current = 0;
   for ( int i = 0; i < choices.size(); ++i ) {
      labels.append( choices[i].label );
      if ( choices[i].kind == aSystem.kind )
         current = i + 1;
   }

   bool ok = false;
   const QString chosen = QInputDialog::getItem(
      aParent, QStringLiteral( "Set system type" ),
      QStringLiteral( "Treat the folder \"%1\" as:" ).arg( aSystem.folderName ), labels, current,
      false, &ok );
   if ( !ok )
      return std::nullopt;

   const int picked = static_cast<int>( labels.indexOf( chosen ) );
   const SystemKind kind = picked <= 0 ? skOther : choices[picked - 1].kind;
   setSystemMapping( aSystem.folderName, kind );
   return kind;
}

void cleanUpBackups( QWidget* aParent, const Gamelist& aGamelist )
{
   const int found = static_cast<int>( gamelistBackups( aGamelist ).size() );
   if ( found == 0 ) {
      QMessageBox::information( aParent, QStringLiteral( "GameList Editor Qt" ),
                                QStringLiteral( "This system has no gamelist backups." ) );
      return;
   }

   bool ok = false;
   const int keep = QInputDialog::getInt(
      aParent, QStringLiteral( "Clean up backups" ),
      QStringLiteral( "%1 backup(s) found. Keep the newest:" ).arg( found ), std::min( found, 3 ), 0,
      found, 1, &ok );
   if ( !ok )
      return;

   const Result<int> removed = deleteOldBackups( aGamelist, keep );
   if ( !removed ) {
      QMessageBox::warning( aParent, QStringLiteral( "GameList Editor Qt" ), removed.error() );
      return;
   }
   QMessageBox::information( aParent, QStringLiteral( "GameList Editor Qt" ),
                             QStringLiteral( "%1 backup(s) deleted." ).arg( removed.value() ) );
}

void exportGamesList( QWidget* aParent, const Gamelist& aGamelist, const QStringList& aNames )
{
   const QString path = QFileDialog::getSaveFileName(
      aParent, QStringLiteral( "Export games list" ),
      QDir( aGamelist.romDir() ).filePath( aGamelist.system().folderName + QStringLiteral( ".txt" ) ),
      QStringLiteral( "Text files (*.txt)" ) );
   if ( path.isEmpty() )
      return;

   QByteArray content;
   for ( const QString& name : aNames )
      content += name.toUtf8() + '\n';

   if ( const Result<> written = writeFileAtomically( path, content ); !written ) {
      QMessageBox::warning( aParent, QStringLiteral( "GameList Editor Qt" ), written.error() );
      return;
   }

   QMessageBox::information( aParent, QStringLiteral( "GameList Editor Qt" ),
                             QStringLiteral( "%1 game(s) exported." ).arg( aNames.size() ) );
}

ViewUpdate deleteOrphans( QWidget* aParent, Gamelist& aGamelist )
{
   return confirmAndRemove( aParent, aGamelist, orphanIndexes( aGamelist ),
                            QStringLiteral( "whose ROM is missing" ) );
}

ViewUpdate deleteDuplicates( QWidget* aParent, Gamelist& aGamelist )
{
   return confirmAndRemove(
      aParent, aGamelist, duplicateIndexes( aGamelist ),
      QStringLiteral( "that list a ROM already listed above them (the first stays)" ) );
}

namespace {

void showSyncReport( QWidget* aParent, const SyncReport& aReport )
{
   QStringList summary;
   QStringList details;
   bool failed = false;
   for ( const SystemReport& system : aReport.systems ) {
      if ( system.ok() ) {
         summary << QStringLiteral( "%1: %2 added, %3 updated, %4 media files copied." )
                       .arg( system.name )
                       .arg( system.added )
                       .arg( system.updated )
                       .arg( system.mediaCopied );
      } else {
         failed = true;
         summary << QStringLiteral( "%1: stopped. %2" ).arg( system.name, system.error );
      }
      for ( const QString& line : system.skipped )
         details << QStringLiteral( "%1: %2" ).arg( system.name, line );
   }
   if ( aReport.canceled )
      summary << QStringLiteral( "Canceled. Run the sync again to finish." );

   QMessageBox box( failed || aReport.canceled ? QMessageBox::Warning : QMessageBox::Information,
                    QStringLiteral( "GameList Editor Qt" ), summary.join( QLatin1Char( '\n' ) ), QMessageBox::Ok,
                    aParent );
   if ( !details.isEmpty() )
      box.setDetailedText( details.join( QLatin1Char( '\n' ) ) );
   box.exec();
}

}  // namespace

void syncToOtherFolder( QWidget* aParent, const QString& aMasterRoot )
{
   const QString title = QStringLiteral( "GameList Editor Qt" );
   const QString targetRoot = QFileDialog::getExistingDirectory(
      aParent, QStringLiteral( "Sync to folder" ), QFileInfo( aMasterRoot ).absolutePath() );
   if ( targetRoot.isEmpty() )
      return;

   if ( sameLocation( targetRoot, aMasterRoot ) ) {
      QMessageBox::warning( aParent, title, QStringLiteral( "That is the folder you are syncing from." ) );
      return;
   }

   QVector<SystemEntry> master = scanSystems( aMasterRoot );
   applySystemMap( master );

   // ponytail: planning runs on the UI thread, repainting every 50 ms. Upgrade: plan in a worker.
   QProgressDialog analysing( QStringLiteral( "Reading the gamelists..." ), QStringLiteral( "Cancel" ), 0, 0, aParent );
   analysing.setWindowTitle( QStringLiteral( "Analysing" ) );
   analysing.setWindowModality( Qt::WindowModal );
   analysing.setMinimumDuration( 0 );
   analysing.setMinimumWidth( 420 );

   QElapsedTimer clock;
   clock.start();
   qint64 lastPaint = -1000;
   const SyncPlan plan = planSync( master, targetRoot, [&]( const QString& aMessage, int aDone, int aTotal ) {
      if ( clock.elapsed() - lastPaint >= 50 ) {
         lastPaint = clock.elapsed();
         analysing.setMaximum( aTotal );
         analysing.setValue( aDone );
         analysing.setLabelText( QStringLiteral( "System %1 of %2\n%3" ).arg( aDone + 1 ).arg( aTotal ).arg( aMessage ) );
         QCoreApplication::processEvents();
      }
      return !analysing.wasCanceled();
   } );
   analysing.close();
   if ( plan.canceled )
      return;

   if ( plan.systems.isEmpty() ) {
      QMessageBox::information( aParent, title, QStringLiteral( "Nothing to sync: the target is up to date." ) );
      return;
   }

   const SyncPlan chosen = Frm_Sync( aParent ).Execute( plan );
   if ( chosen.systems.isEmpty() )
      return;

   const int steps = syncStepCount( chosen );

   QProgressDialog progress( QStringLiteral( "Syncing..." ), QStringLiteral( "Cancel" ), 0, steps, aParent );
   progress.setWindowTitle( QStringLiteral( "Syncing" ) );
   progress.setWindowModality( Qt::WindowModal );
   progress.setMinimumDuration( 0 );
   progress.setMinimumWidth( 420 );
   progress.setAutoClose( false );
   progress.setAutoReset( false );
   progress.setValue( 0 );  // a QProgressDialog shows only once it gets a value

   int done = 0;
   const SyncReport report = applySync( chosen, [&]( const QString& aMessage ) {
      progress.setValue( qMin( done++, steps ) );
      progress.setLabelText( QStringLiteral( "Step %1 of %2\n%3" ).arg( qMin( done, steps ) ).arg( steps ).arg( aMessage ) );
      QCoreApplication::processEvents();
      return !progress.wasCanceled();
   } );
   progress.close();

   showSyncReport( aParent, report );
}
