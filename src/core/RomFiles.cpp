#include "RomFiles.h"

#include "PathUtil.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

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

// The files a description names, as physical paths. Empty for any other file.
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

}  // namespace

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

QStringList exclusiveTracks( const QString& aRomPath, const QStringList& aListedRoms,
                             const QString& aRomDir, bool aFoldsCase )
{
   QStringList candidates = romParts( aRomPath );
   candidates.removeFirst();
   if ( candidates.isEmpty() )
      return {};

   QStringList inUse;
   for ( const QString& rom : aListedRoms )
      inUse << romParts( rom );

   QStringList tracks;
   for ( const QString& path : std::as_const( candidates ) ) {
      const bool needed = std::any_of( inUse.cbegin(), inUse.cend(), [&]( const QString& aUsed ) {
         return sameFileOnDisk( aUsed, path, aFoldsCase );
      } );
      if ( isInsideDir( aRomDir, path ) && !needed && QFileInfo( path ).isFile() )
         tracks << path;
   }
   return tracks;
}
