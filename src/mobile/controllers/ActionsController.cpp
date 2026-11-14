#include "ActionsController.h"

#include "GamesController.h"
#include "LibraryController.h"
#include "MediaOps.h"
#include "PathUtil.h"
#include "Resources.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QThread>

#include <algorithm>
#include <memory>

namespace {

QString arrow()
{
   return QString( QChar( 0x2192 ) );
}

QString kindName( bool aIsImage )
{
   return aIsImage ? QStringLiteral( "image" ) : QStringLiteral( "video" );
}

QString sizeText( qint64 aBytes )
{
   return QLocale().formattedDataSize( aBytes, 1, QLocale::DataSizeTraditionalFormat );
}

QVariantMap item( const QString& aText, bool aEnabled = true )
{
   return { { QStringLiteral( "text" ), aText }, { QStringLiteral( "enabled" ), aEnabled } };
}

}  // namespace

ActionsController::ActionsController( LibraryController* aLibrary, GamesController* aGames,
                                      QObject* aParent )
   : QObject( aParent ), FLibrary( aLibrary ), FGames( aGames )
{
}

ActionsController::~ActionsController()
{
   FCancel = true;
   if ( FThread )
      FThread->wait();
}

void ActionsController::fail( const QString& aError )
{
   FError = aError;
   FReport.clear();
   emit reportChanged();
}

void ActionsController::succeed( const QString& aReport )
{
   FError.clear();
   FReport = aReport;
   clearItems();
   emit reportChanged();
}

void ActionsController::clearItems()
{
   FPaths.clear();
   FLinks.clear();
   FRenames.clear();
   FImages.clear();
   FIndexes.clear();
   FItems.clear();
   emit itemsChanged();
}

QString ActionsController::relative( const QString& aPath ) const
{
   const Gamelist* list = FLibrary->gamelist();
   return list ? QDir( list->romDir() ).relativeFilePath( aPath ) : aPath;
}

void ActionsController::setChoice( bool aOn )
{
   if ( aOn == FChoice )
      return;
   FChoice = aOn;
   if ( FAction == RenameMedia && FLibrary->gamelist() )
      fillItems();
   else
      emit itemsChanged();
}

void ActionsController::fillItems()
{
   const Gamelist& list = *FLibrary->gamelist();
   FPaths.clear();
   FLinks.clear();
   FRenames.clear();
   FImages.clear();
   FIndexes.clear();
   FItems.clear();

   switch ( FAction ) {
   case AddRoms:
      FPaths = unlistedRoms( list );
      for ( const QString& path : std::as_const( FPaths ) )
         FItems.append( item( relative( path ) ) );
      break;
   case LinkMedia:
      FLinks = unlinkedMedia( list );
      for ( const MediaLink& link : std::as_const( FLinks ) )
         FItems.append( item( QStringLiteral( "%1: %2 %3 %4" )
                                 .arg( list.at( link.index ).romName(), kindName( link.isImage ),
                                       arrow(), relative( link.path ) ) ) );
      break;
   case RenameMedia:
      FRenames = misnamedMedia( list, FChoice );
      for ( const MediaRename& rename : std::as_const( FRenames ) ) {
         QString text = QStringLiteral( "%1: %2 %3 %4 %5" )
                           .arg( list.at( rename.index ).romName(), kindName( rename.isImage ),
                                 relative( rename.path ), arrow(),
                                 QFileInfo( rename.newPath ).fileName() );
         if ( !rename.problem.isEmpty() )
            text += QStringLiteral( " (not renamed: %1)" ).arg( rename.problem );
         FItems.append( item( text, rename.problem.isEmpty() ) );
      }
      break;
   case ShrinkImages:
      FImages = oversizedImages( list, FMaxBytes );
      for ( const OversizedImage& image : std::as_const( FImages ) )
         FItems.append( item( QStringLiteral( "%1: %2 (%3)" )
                                 .arg( list.at( image.index ).romName(), relative( image.path ),
                                       sizeText( image.size ) ) ) );
      break;
   case CleanBackups:
      FPaths = gamelistBackups( list ).mid( FKeep );
      for ( const QString& path : std::as_const( FPaths ) )
         FItems.append( item( QFileInfo( path ).fileName() ) );
      break;
   case DeleteOrphans:
   case DeleteDuplicates:
      FIndexes = FAction == DeleteOrphans ? orphanIndexes( list ) : duplicateIndexes( list );
      for ( const int index : std::as_const( FIndexes ) )
         FItems.append( item( QStringLiteral( "%1 (%2)" ).arg( list.at( index ).romName(), list.at( index ).name ) ) );
      break;
   }
   emit itemsChanged();
}

bool ActionsController::find( int aAction, double aValue )
{
   if ( FBusy )
      return false;
   FReport.clear();
   FError.clear();
   FChoice = false;
   FChoiceLabel.clear();

   const Gamelist* list = FLibrary->gamelist();
   if ( !list ) {
      clearItems();
      fail( QStringLiteral( "No system is loaded." ) );
      return false;
   }
   if ( aAction < AddRoms || aAction > CleanBackups ) {
      clearItems();
      fail( QStringLiteral( "Unknown action." ) );
      return false;
   }
   FAction = static_cast<Action>( aAction );

   if ( ( FAction == LinkMedia || FAction == RenameMedia ) && list->conventionalMedia() ) {
      clearItems();
      fail( QStringLiteral( "ES-DE finds media by the ROM's name, so there is nothing to do here." ) );
      return false;
   }

   const QString removeNote =
      QStringLiteral( "Only the gamelist changes: no ROM, picture or video file is deleted. " );
   switch ( FAction ) {
   case AddRoms:
      FTitle = QStringLiteral( "Add missing ROMs" );
      FIntro = QStringLiteral( "ROMs in the system folder that the gamelist does not list." );
      FApplyLabel = QStringLiteral( "Add" );
      FEmptyText = QStringLiteral( "Every ROM in this folder is already in the gamelist." );
      break;
   case LinkMedia:
      FTitle = QStringLiteral( "Link existing media" );
      FIntro = QStringLiteral( "Pictures and videos found for games that do not link them. Untick "
                               "what you do not want." );
      FApplyLabel = QStringLiteral( "Link" );
      FEmptyText = QStringLiteral( "No unlinked picture or video found." );
      break;
   case RenameMedia:
      FTitle = QStringLiteral( "Rename media after their ROM" );
      FIntro = QStringLiteral( "Pictures and videos whose file name is not their ROM's. Untick what "
                               "you do not want renamed." );
      FApplyLabel = QStringLiteral( "Rename" );
      FEmptyText = QStringLiteral( "Every picture and video is named after its ROM." );
      // The strict list is the superset: if it is empty, no setting of the choice finds anything.
      if ( !misnamedMedia( *list, true ).isEmpty() )
         FChoiceLabel = QStringLiteral( "Also rename files ending in -image or -video (x-image.png %1 x.png)" ).arg( arrow() );
      break;
   case ShrinkImages:
      FMaxBytes = qRound64( aValue * 1024 * 1024 );
      FTitle = QStringLiteral( "Shrink large pictures" );
      FIntro = QStringLiteral( "Pictures over %1. They are re-encoded in place, in the same format, "
                              "to just under that size. The originals are overwritten. Untick what "
                              "you want left alone." ).arg( sizeText( FMaxBytes ) );
      FApplyLabel = QStringLiteral( "Shrink" );
      FEmptyText = QStringLiteral( "No picture is over that size." );
      break;
   case CleanBackups:
      FKeep = std::max( 0, qRound( aValue ) );
      FTitle = QStringLiteral( "Clean up backups" );
      FIntro = QStringLiteral( "Older gamelist backups, newest %1 kept. Untick what you want to keep." ).arg( FKeep );
      FApplyLabel = QStringLiteral( "Delete" );
      FEmptyText = QStringLiteral( "There are no gamelist backups beyond the newest %1." ).arg( FKeep );
      break;
   case DeleteOrphans:
      FTitle = QStringLiteral( "Delete orphans" );
      FIntro = removeNote + QStringLiteral( "Games whose ROM is missing." );
      FApplyLabel = QStringLiteral( "Delete" );
      FEmptyText = QStringLiteral( "No game is missing its ROM." );
      break;
   case DeleteDuplicates:
      FTitle = QStringLiteral( "Delete duplicates" );
      FIntro = removeNote + QStringLiteral( "Games that list a ROM already listed above them; the first stays." );
      FApplyLabel = QStringLiteral( "Delete" );
      FEmptyText = QStringLiteral( "No ROM is listed twice." );
      break;
   }

   fillItems();
   return true;
}

bool ActionsController::reloadSystem()
{
   // The games model counts rows without being told about added or removed ones; a reload resets it.
   if ( FLibrary->selectSystem( FLibrary->current() ) )
      return true;
   fail( FLibrary->error() );
   return false;
}

void ActionsController::redraw()
{
   if ( FGames )
      FGames->refresh();
}

void ActionsController::apply( const QVariantList& aRows )
{
   if ( FBusy )
      return;
   Gamelist* list = FLibrary->gamelist();
   if ( !list ) {
      fail( QStringLiteral( "No system is loaded." ) );
      emit finished( false );
      return;
   }

   QVector<int> rows;
   for ( const QVariant& row : aRows ) {
      const int at = row.toInt();
      if ( at >= 0 && at < FItems.size() && FItems.at( at ).toMap().value( QStringLiteral( "enabled" ) ).toBool()
           && !rows.contains( at ) )
         rows.append( at );
   }

   const auto done = [this]( bool aOk, const QString& aText ) {
      if ( aOk )
         succeed( aText );
      else
         fail( aText );
      emit finished( aOk );
   };

   switch ( FAction ) {
   case AddRoms: {
      QStringList chosen;
      for ( const int at : rows )
         chosen.append( FPaths.at( at ) );
      const Result<int> added = list->addGames( chosen );
      if ( !added ) {
         done( false, added.error() );
         return;
      }
      const int count = added.value();
      if ( !reloadSystem() ) {
         emit finished( false );
         return;
      }
      done( true, QStringLiteral( "%1 game(s) added." ).arg( count ) );
      return;
   }
   case LinkMedia: {
      QVector<MediaLink> chosen;
      for ( const int at : rows )
         chosen.append( FLinks.at( at ) );
      const Result<int> linked = list->linkMedia( chosen );
      if ( !linked ) {
         done( false, linked.error() );
         return;
      }
      redraw();
      done( true, QStringLiteral( "%1 link(s) set." ).arg( linked.value() ) );
      return;
   }
   case RenameMedia: {
      QVector<MediaRename> chosen;
      for ( const int at : rows )
         chosen.append( FRenames.at( at ) );
      const Result<int> renamed = list->renameMedia( chosen );
      if ( !renamed ) {
         done( false, renamed.error() );
         return;
      }
      redraw();
      done( true, QStringLiteral( "%1 file(s) renamed." ).arg( renamed.value() ) );
      return;
   }
   case ShrinkImages: {
      QVector<OversizedImage> chosen;
      for ( const int at : rows )
         chosen.append( FImages.at( at ) );
      applyShrink( chosen );
      return;
   }
   case CleanBackups: {
      int removed = 0;
      QStringList stuck;
      for ( const int at : rows ) {
         if ( discardFile( FPaths.at( at ) ) )
            ++removed;
         else
            stuck << FPaths.at( at );
      }
      if ( !stuck.isEmpty() ) {
         done( false, QStringLiteral( "%1 backup(s) deleted. Could not delete:\n%2" )
                         .arg( removed ).arg( stuck.join( QLatin1Char( '\n' ) ) ) );
         return;
      }
      done( true, QStringLiteral( "%1 backup(s) deleted." ).arg( removed ) );
      return;
   }
   case DeleteOrphans:
   case DeleteDuplicates: {
      QVector<int> chosen;
      for ( const int at : rows )
         chosen.append( FIndexes.at( at ) );
      if ( const Result<> removed = list->removeGames( chosen ); !removed ) {
         done( false, removed.error() );
         return;
      }
      if ( !reloadSystem() ) {
         emit finished( false );
         return;
      }
      done( true, QStringLiteral( "%1 game(s) removed from the gamelist." ).arg( chosen.size() ) );
      return;
   }
   }
}

bool ActionsController::exportList()
{
   const Gamelist* list = FLibrary->gamelist();
   const auto* model = FGames ? qobject_cast<QAbstractItemModel*>( FGames->model() ) : nullptr;
   if ( !list || !model ) {
      fail( QStringLiteral( "No system is loaded." ) );
      return false;
   }

   QByteArray content;
   for ( int row = 0; row < model->rowCount(); ++row )
      content += model->index( row, 0 ).data().toString().toUtf8() + '\n';

   const QString path = QDir( list->romDir() ).filePath( list->system().folderName + Cst_TxtExtension );
   if ( const Result<> written = writeFileAtomically( path, content ); !written ) {
      fail( written.error() );
      return false;
   }
   succeed( QStringLiteral( "%1 game(s) exported to %2." ).arg( model->rowCount() ).arg( path ) );
   return true;
}

// shrinkImages only touches picture files, never the Gamelist, so it can run on a worker.
void ActionsController::applyShrink( const QVector<OversizedImage>& aChosen )
{
   FBusy = true;
   FCancel = false;
   FProgress = 0;
   FTotal = static_cast<int>( aChosen.size() );
   emit busyChanged();

   const qint64 maxBytes = FMaxBytes;
   const auto report = std::make_shared<ShrinkReport>();
   FThread = QThread::create( [this, aChosen, maxBytes, report] {
      *report = shrinkImages( aChosen, maxBytes, [this]( int aDone ) {
         QMetaObject::invokeMethod( this, [this, aDone] {
            FProgress = aDone;
            emit busyChanged();
         }, Qt::QueuedConnection );
         return !FCancel.load();
      } );
   } );
   FThread->setParent( this );
   connect( FThread, &QThread::finished, this, [this, report] {
      FThread->deleteLater();
      FThread = nullptr;
      FBusy = false;
      FProgress = FTotal;
      emit busyChanged();

      QString text = QStringLiteral( "%1 picture(s) shrunk, %2 saved." )
                        .arg( report->shrunk )
                        .arg( sizeText( report->saved ) );
      if ( !report->problems.isEmpty() )
         text += QStringLiteral( "\n\nNot shrunk:\n%1" ).arg( report->problems.join( QLatin1Char( '\n' ) ) );
      redraw();
      succeed( text );
      emit finished( true );
   } );
   FThread->start();
}
