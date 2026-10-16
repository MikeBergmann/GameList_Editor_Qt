#include "SystemActions.h"

#include "F_AddRoms.h"
#include "F_LinkMedia.h"
#include "Gamelist.h"
#include "MediaOps.h"

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

   QString error;
   if ( !aGamelist.removeGames( aIndexes, &error ) ) {
      QMessageBox::warning( aParent, QStringLiteral( "GameList Editor Qt" ), error );
      return ViewUpdate::None;
   }

   return ViewUpdate::Reset;
}

}  // namespace

ViewUpdate addMissingRoms( QWidget* aParent, Gamelist& aGamelist )
{
   const QStringList chosen =
      Frm_AddRoms( aParent ).Execute( aGamelist.systemDir(), aGamelist.unlistedRoms() );
   if ( chosen.isEmpty() )
      return ViewUpdate::None;

   QString error;
   const int added = aGamelist.addGames( chosen, &error );
   if ( added < 0 ) {
      QMessageBox::warning( aParent, QStringLiteral( "GameList Editor Qt" ), error );
      return ViewUpdate::None;
   }

   // Rows are indices into the Gamelist; the model only knows how to reset for added games.
   return added == 0 ? ViewUpdate::None : ViewUpdate::Reset;
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

   QString error;
   if ( aGamelist.linkMedia( chosen, &error ) < 0 ) {
      QMessageBox::warning( aParent, QStringLiteral( "GameList Editor Qt" ), error );
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

   QString error;
   if ( aGamelist.renameMedia( chosen, &error ) < 0 ) {
      QMessageBox::warning( aParent, QStringLiteral( "GameList Editor Qt" ), error );
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
   QMessageBox::information( aParent, QStringLiteral( "GameList Editor Qt" ), text );

   // The links are unchanged, but the editor may be showing a picture that just changed.
   return ViewUpdate::Refresh;
}

ViewUpdate deleteOrphans( QWidget* aParent, Gamelist& aGamelist )
{
   return confirmAndRemove( aParent, aGamelist, aGamelist.orphanIndexes(),
                            QStringLiteral( "whose ROM is missing" ) );
}

ViewUpdate deleteDuplicates( QWidget* aParent, Gamelist& aGamelist )
{
   return confirmAndRemove(
      aParent, aGamelist, aGamelist.duplicateIndexes(),
      QStringLiteral( "that list a ROM already listed above them (the first stays)" ) );
}
