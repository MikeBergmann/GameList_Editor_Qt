#include "Systems.h"

#include "PathUtil.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

QVector<SystemEntry> scanSystems( const QString& aRootPath )
{
   QVector<SystemEntry> systems;

   // Folders by attribute, not by a '*.*' name filter, which needs a literal dot and
   // so would miss folder names like "snes". Hidden folders are left out.
   const QDir root( aRootPath );
   const QStringList folders = root.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );

   for ( const QString& folder : folders ) {
      const QString gamelist = resolvePath(
         root.filePath( folder + QLatin1Char( '/' ) + QLatin1String( Cst_GameListFileName ) ) );
      if ( !QFileInfo::exists( gamelist ) )
         continue;

      systems.append( { folder, systemKindFromFolder( folder ), gamelist } );
   }

   return systems;
}
