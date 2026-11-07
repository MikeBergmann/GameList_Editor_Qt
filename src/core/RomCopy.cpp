#include "RomCopy.h"

#include "PathUtil.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStorageInfo>

namespace {

// The file names a disc description refers to, relative to its own folder.
QStringList referencedFiles( const QString& aPath )
{
   const QString suffix = QFileInfo( aPath ).suffix().toLower();
   QFile file( aPath );
   if ( !file.open( QIODevice::ReadOnly ) )
      return {};

   const QStringList lines = QString::fromUtf8( file.readAll() ).split( QLatin1Char( '\n' ) );
   QStringList names;

   // FILE "name.bin" BINARY / <track> <lba> <type> <sector> "name.bin" <offset>
   static const QRegularExpression cue( QStringLiteral( R"re(^\s*FILE\s+(?:"([^"]+)"|(\S+)))re" ),
                                        QRegularExpression::CaseInsensitiveOption );
   static const QRegularExpression gdi(
      QStringLiteral( R"re(^\s*\d+\s+\d+\s+\d+\s+\d+\s+(?:"([^"]+)"|(\S+)))re" ) );

   for ( QString line : lines ) {
      line = line.trimmed();
      if ( line.startsWith( QChar( 0xFEFF ) ) )
         line.remove( 0, 1 );
      if ( line.isEmpty() )
         continue;

      if ( suffix == QLatin1String( "m3u" ) ) {
         if ( !line.startsWith( QLatin1Char( '#' ) ) )
            names << line;
         continue;
      }

      const QRegularExpressionMatch match = ( suffix == QLatin1String( "cue" ) ? cue : gdi ).match( line );
      if ( match.hasMatch() )
         names << ( match.captured( 1 ).isEmpty() ? match.captured( 2 ) : match.captured( 1 ) );
   }

   for ( QString& name : names )
      name.replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) );
   return names;
}

bool isDiscDescription( const QString& aPath )
{
   const QString suffix = QFileInfo( aPath ).suffix().toLower();
   return suffix == QLatin1String( "cue" ) || suffix == QLatin1String( "m3u" ) || suffix == QLatin1String( "gdi" );
}

}  // namespace

QStringList discTracks( const QString& aPath )
{
   QStringList tracks;
   if ( !isDiscDescription( aPath ) )
      return tracks;

   const QString folder = QFileInfo( aPath ).path();
   for ( const QString& name : referencedFiles( aPath ) )
      tracks << physicalPath( folder, name );
   return tracks;
}

QStringList romParts( const QString& aRomPath )
{
   QStringList parts{ aRomPath };
   for ( int next = 0; next < parts.size(); ++next ) {
      for ( const QString& track : discTracks( parts.at( next ) ) ) {
         if ( !parts.contains( track ) )
            parts << track;
      }
   }
   return parts;
}

RomCopyPlan planRomCopy( const QString& aSourceDir, const QString& aTargetDir, const QString& aRomLink )
{
   RomCopyPlan plan;
   const QDir source( aSourceDir );
   QSet<QString> seen;
   QStringList queue{ physicalPath( aSourceDir, aRomLink ) };

   while ( !queue.isEmpty() ) {
      const QString path = queue.takeFirst();
      if ( seen.contains( path ) )
         continue;
      seen.insert( path );

      const QFileInfo info( path );
      if ( !info.exists() ) {
         plan.problems << QStringLiteral( "%1 is missing." ).arg( path );
         continue;
      }

      if ( info.isDir() ) {
         plan.problems << QStringLiteral( "%1 is a folder ROM; folder ROMs are not copied." ).arg( path );
         continue;
      }

      // Another drive yields an absolute path, not "../".
      const QString relative = source.relativeFilePath( path );
      if ( relative.startsWith( QLatin1String( "../" ) ) || QDir::isAbsolutePath( relative ) ) {
         plan.problems << QStringLiteral( "%1 is outside the ROM folder." ).arg( path );
         continue;
      }

      const QString target = physicalPath( aTargetDir, relative );
      const QFileInfo existing( target );
      if ( !existing.exists() || existing.size() != info.size() ) {
         plan.files.append( { path, target, info.size() } );
         plan.bytes += info.size();
      }

      if ( isDiscDescription( path ) ) {
         for ( const QString& track : discTracks( path ) )
            queue << track;
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
