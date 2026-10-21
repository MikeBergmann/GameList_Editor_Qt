#include "PathUtil.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringList>

namespace {
bool GTrashEnabled = true;
}

// Gamelist paths are relative to the system folder and start with "./", which
// cleanPath folds away. Always joined with '/', which Qt accepts on every platform.
QString physicalPath( const QString& aSystemDir, const QString& aRelative )
{
   if ( aRelative.isEmpty() )
      return {};

   return resolvePath( QDir::cleanPath( QDir( aSystemDir ).filePath( aRelative ) ) );
}

QString resolvePath( const QString& aPath )
{
   if ( aPath.isEmpty() || QFileInfo::exists( aPath ) )
      return aPath;

   QStringList parts = QDir::cleanPath( aPath ).split( QLatin1Char( '/' ) );
   if ( parts.isEmpty() )
      return aPath;

   // An absolute path splits with an empty first component; that one is "/".
   QString resolved = parts.takeFirst();
   if ( resolved.isEmpty() )
      resolved = QStringLiteral( "/" );
   // "C:" alone is the drive's current directory on Windows, not its root.
   else if ( resolved.size() == 2 && resolved.at( 1 ) == QLatin1Char( ':' ) )
      resolved += QLatin1Char( '/' );

   // ponytail: a miss re-lists the containing directory for each component, so a
   // ROM folder that is absent altogether costs one listing per game. That
   // only happens on the orphan path; cache listings per directory if it shows.
   for ( int index = 0; index < parts.size(); ++index ) {
      const QString& part = parts.at( index );
      QDir dir( resolved );
      if ( QFileInfo::exists( dir.filePath( part ) ) ) {
         resolved = dir.filePath( part );
         continue;
      }

      QString match;
      for ( const QString& name : dir.entryList( QDir::AllEntries | QDir::NoDotAndDotDot ) ) {
         if ( name.compare( part, Qt::CaseInsensitive ) == 0 ) {
            match = name;
            break;
         }
      }

      // Genuinely missing: keep the case found so far and the rest as written,
      // so a file about to be created lands in the existing "Media/", not a new
      // "media/". Still does not exist, so the caller still reports it.
      if ( match.isEmpty() )
         return dir.filePath( parts.mid( index ).join( QLatin1Char( '/' ) ) );

      resolved = dir.filePath( match );
   }

   return resolved;
}

bool discardFile( const QString& aPath )
{
   if ( GTrashEnabled && QFile::moveToTrash( aPath ) )
      return true;

   return QFile::remove( aPath );
}

void setTrashEnabled( bool aEnabled )
{
   GTrashEnabled = aEnabled;
}

Result<> writeFileAtomically( const QString& aPath, const QByteArray& aContent )
{
   QSaveFile file( aPath );
   if ( !file.open( QIODevice::WriteOnly ) || file.write( aContent ) != aContent.size() ||
        !file.commit() )
      return Error{ QStringLiteral( "Cannot write %1: %2" ).arg( aPath, file.errorString() ) };

   return {};
}
