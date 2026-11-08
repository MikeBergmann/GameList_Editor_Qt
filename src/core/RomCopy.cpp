#include "RomCopy.h"

#include "PathUtil.h"
#include "RomFiles.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>

RomCopyPlan planRomCopy( const QString& aSourceDir, const QString& aTargetDir, const QString& aRomLink )
{
   RomCopyPlan plan;
   const QDir source( aSourceDir );

   for ( const QString& path : romParts( physicalPath( aSourceDir, aRomLink ) ) ) {
      const QFileInfo info( path );
      if ( !info.exists() ) {
         plan.problems << QStringLiteral( "%1 is missing." ).arg( path );
         continue;
      }

      if ( info.isDir() ) {
         plan.problems << QStringLiteral( "%1 is a folder ROM; folder ROMs are not copied." ).arg( path );
         continue;
      }

      if ( !isInsideDir( aSourceDir, path ) ) {
         plan.problems << QStringLiteral( "%1 is outside the ROM folder." ).arg( path );
         continue;
      }

      const QString target = physicalPath( aTargetDir, source.relativeFilePath( path ) );
      const QFileInfo existing( target );
      if ( !existing.exists() || existing.size() != info.size() ) {
         plan.files.append( { path, target, info.size() } );
         plan.bytes += info.size();
      }
   }

   return plan;
}

Result<> copyRomFiles( const QVector<RomFile>& aFiles, const RomProgress& aProgress )
{
   for ( const RomFile& file : aFiles ) {
      if ( aProgress && !aProgress( file ) )
         return Error{ QStringLiteral( "Cancelled." ) };

      const QString folder = QFileInfo( file.target ).path();
      if ( !QDir().mkpath( folder ) )
         return Error{ QStringLiteral( "Cannot create %1." ).arg( folder ) };

      const Result<QString> swapped = stageAndSwap(
         file.target, QStringLiteral( "Cannot copy %1 to %2." ).arg( file.source, file.target ),
         [&]( const QString& aStaged ) {
            if ( !QFile::copy( file.source, aStaged ) )
               return false;
            // A ROM unchanged on the master keeps the same time on every device.
            if ( QFile stamped( aStaged ); stamped.open( QIODevice::ReadWrite ) )
               stamped.setFileTime( QFileInfo( file.source ).lastModified(), QFileDevice::FileModificationTime );
            return true;
         } );
      if ( !swapped )
         return swapped;
      if ( !swapped.value().isEmpty() )
         QFile::remove( swapped.value() );
   }

   return {};
}

Result<> checkFreeSpace( const QString& aTargetDir, qint64 aBytes )
{
   QString existing = QDir::cleanPath( aTargetDir );
   while ( !QFileInfo::exists( existing ) ) {
      const QString parent = QFileInfo( existing ).path();
      if ( parent == existing )
         return {};
      existing = parent;
   }

   const QStorageInfo volume( existing );
   if ( !volume.isValid() || volume.bytesAvailable() < 0 || volume.bytesAvailable() >= aBytes )
      return {};

   return Error{ QStringLiteral( "Not enough space on %1: %2 MB needed, %3 MB free." )
                    .arg( volume.rootPath() )
                    .arg( aBytes / 1048576 + ( aBytes % 1048576 ? 1 : 0 ) )
                    .arg( volume.bytesAvailable() / 1048576 ) };
}
