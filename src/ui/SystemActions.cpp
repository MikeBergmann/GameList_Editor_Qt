#include "SystemActions.h"

#include "F_AddRoms.h"
#include "F_LinkMedia.h"
#include "Gamelist.h"
#include "MediaOps.h"
#include "PathUtil.h"

#include <algorithm>

#include <QDir>
#include <QFileDialog>
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
      Frm_AddRoms( aParent ).Execute( aGamelist.systemDir(), unlistedRoms( aGamelist ) );
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

ViewUpdate linkExistingMedia( QWidget* aParent, Gamelist& aGamelist )
{
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

   int shrunk = 0;
   qint64 saved = 0;
   QStringList problems;
   for ( int row = 0; row < chosen.size(); ++row ) {
      progress.setValue( row );
      if ( progress.wasCanceled() )
         break;

      const Result<qint64> size = shrinkImageFile( chosen.at( row ).path, maxBytes );
      if ( !size ) {
         problems << size.error();
         continue;
      }
      ++shrunk;
      saved += chosen.at( row ).size - size.value();
   }
   progress.setValue( static_cast<int>( chosen.size() ) );

   QString text = QStringLiteral( "%1 picture(s) shrunk, %2 saved." )
                     .arg( shrunk )
                     .arg( QLocale().formattedDataSize( saved, 1, QLocale::DataSizeTraditionalFormat ) );
   if ( !problems.isEmpty() )
      text += QStringLiteral( "\n\nNot shrunk:\n%1" ).arg( problems.join( QLatin1Char( '\n' ) ) );
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
      QDir( aGamelist.systemDir() ).filePath( aGamelist.system().folderName + QStringLiteral( ".txt" ) ),
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
