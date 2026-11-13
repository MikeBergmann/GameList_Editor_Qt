#include "ActionsController.h"

#include "LibraryController.h"
#include "MediaOps.h"

#include <QDir>

ActionsController::ActionsController( LibraryController* aLibrary, QObject* aParent )
   : QObject( aParent ), FLibrary( aLibrary )
{
}

void ActionsController::fail( const QString& aError )
{
   FError = aError;
   emit romsChanged();
}

bool ActionsController::findMissingRoms()
{
   FPaths.clear();
   FNames.clear();
   FAdded = 0;
   FError.clear();

   const Gamelist* list = FLibrary->gamelist();
   if ( !list ) {
      fail( QStringLiteral( "No system is loaded." ) );
      return false;
   }

   FPaths = unlistedRoms( *list );
   const QDir folder( list->romDir() );
   for ( const QString& path : std::as_const( FPaths ) )
      FNames.append( folder.relativeFilePath( path ) );

   emit romsChanged();
   return true;
}

bool ActionsController::addRoms( const QVariantList& aRows )
{
   Gamelist* list = FLibrary->gamelist();
   if ( !list ) {
      fail( QStringLiteral( "No system is loaded." ) );
      return false;
   }

   QStringList chosen;
   for ( const QVariant& row : aRows )
      if ( const int at = row.toInt(); at >= 0 && at < FPaths.size() )
         chosen.append( FPaths.at( at ) );

   const Result<int> added = list->addGames( chosen );
   if ( !added ) {
      fail( added.error() );
      return false;
   }

   FAdded = added.value();
   FError.clear();
   FPaths.clear();
   FNames.clear();
   // The games model counts rows without being told about new ones; a reload resets it.
   if ( !FLibrary->selectSystem( FLibrary->current() ) ) {
      fail( FLibrary->error() );
      return false;
   }

   emit romsChanged();
   return true;
}
