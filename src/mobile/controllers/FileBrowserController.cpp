#include "FileBrowserController.h"

#include <QDir>
#include <QVariantMap>

namespace {

QVariantMap entry( const QString& aName, const QString& aPath )
{
   return { { QStringLiteral( "name" ), aName }, { QStringLiteral( "path" ), aPath } };
}

}  // namespace

FileBrowserController::FileBrowserController( QString aStorageDir, QObject* aParent )
   : QObject( aParent ), FStorageDir( QDir::cleanPath( aStorageDir ) )
{
   open( {} );
}

QVariantList FileBrowserController::roots() const
{
   QVariantList result;

   const QString internal = FStorageDir + QStringLiteral( "/emulated/0" );
   if ( QDir( internal ).exists() )
      result.append( entry( QStringLiteral( "Internal storage" ), internal ) );

   // "self" is a link to the internal storage.
   const QDir storage( FStorageDir );
   for ( const QString& name : storage.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::IgnoreCase ) )
      if ( name != QLatin1String( "emulated" ) && name != QLatin1String( "self" ) )
         result.append( entry( name, storage.filePath( name ) ) );

   return result;
}

void FileBrowserController::open( const QString& aPath )
{
   const QDir dir( aPath );
   if ( aPath.isEmpty() || !dir.exists() ) {
      FPath.clear();
      FEntries = roots();
   } else {
      FPath = QDir::cleanPath( dir.absolutePath() );
      FEntries.clear();
      // Hidden folders too: frontends keep their data in ".emulationstation" and the like.
      const QDir::Filters filters = QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden;
      for ( const QString& name : dir.entryList( filters, QDir::IgnoreCase ) )
         FEntries.append( entry( name, dir.filePath( name ) ) );
   }

   emit changed();
}

void FileBrowserController::up()
{
   if ( atRoots() )
      return;

   for ( const QVariant& root : roots() )
      if ( root.toMap().value( QStringLiteral( "path" ) ) == FPath ) {
         open( {} );
         return;
      }

   QDir dir( FPath );
   open( dir.cdUp() ? dir.path() : QString() );
}
