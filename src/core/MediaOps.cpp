#include "MediaOps.h"

#include "PathUtil.h"

#include <algorithm>

#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QSet>

#include <cmath>

namespace {

// Media already on disk is looked for in the gamelist's own folder, then in these:
// ES writes ./media/..., older setups and ScummVM keep plain ./images.
const QStringList Cst_ImageSearchFolders = { QStringLiteral( "./media/images/" ),
                                             QStringLiteral( "./images/" ) };
const QStringList Cst_VideoSearchFolders = { QStringLiteral( "./media/videos/" ),
                                             QStringLiteral( "./videos/" ) };

// What scrapers append to the ROM name, in order of preference: the plain name first.
const QStringList Cst_ImageStems = { QString(), QStringLiteral( "-image" ) };
const QStringList Cst_VideoStems = { QString(), QStringLiteral( "-video" ) };

// Kind of file by MIME type, so no extension list to keep.
const QString Cst_ImageMime = QStringLiteral( "image/" );
const QString Cst_VideoMime = QStringLiteral( "video/" );

// The files of aDir whose MIME type starts with aMimePrefix, by lower-cased name
// without the extension ("x.scummvm.png" is "x.scummvm") -> the real name. Judged
// by file name alone: nothing is opened. Of several files with the same name
// (Sonic.png, Sonic.jpg) the first in name order wins.
QHash<QString, QString> mediaIndex( const QDir& aDir, const QString& aMimePrefix )
{
   const QMimeDatabase mimes;
   QHash<QString, QString> index;
   for ( const QString& name : aDir.entryList( QDir::Files ) ) {
      if ( !mimes.mimeTypeForFile( name, QMimeDatabase::MatchExtension )
              .name()
              .startsWith( aMimePrefix ) )
         continue;

      const QString key = QFileInfo( name ).completeBaseName().toLower();
      if ( !index.contains( key ) )
         index.insert( key, name );
   }
   return index;
}

// The file name for the first base+stem that aIndex holds, or empty.
QString findMedia( const QHash<QString, QString>& aIndex, const QStringList& aBases,
                   const QStringList& aStems )
{
   for ( const QString& base : aBases ) {
      for ( const QString& stem : aStems ) {
         const QString name = aIndex.value( base + stem );
         if ( !name.isEmpty() )
            return name;
      }
   }
   return {};
}

}  // namespace

QVector<int> orphanIndexes( const Gamelist& aList )
{
   QVector<int> found;
   for ( int index = 0; index < aList.count(); ++index ) {
      if ( aList.at( index ).isOrphan )
         found.append( index );
   }
   return found;
}

QVector<int> duplicateIndexes( const Gamelist& aList )
{
   QHash<QString, QStringList> seen;
   QVector<int> found;
   for ( int index = 0; index < aList.count(); ++index ) {
      const QString& path = aList.at( index ).physicalRomPath;
      if ( path.isEmpty() )
         continue;

      QStringList& bucket = seen[QFileInfo( path ).fileName().toLower()];
      const bool repeated = std::any_of( bucket.cbegin(), bucket.cend(), [&]( const QString& aSeen ) {
         return sameFileOnDisk( aSeen, path, aList.foldsCase() );
      } );
      if ( repeated )
         found.append( index );
      else
         bucket.append( path );
   }
   return found;
}

QStringList unlistedRoms( const Gamelist& aList )
{
   QSet<QString> listed;
   for ( int index = 0; index < aList.count(); ++index ) {
      if ( !aList.at( index ).physicalRomPath.isEmpty() )
         listed.insert( fileKey( aList.at( index ).physicalRomPath, aList.foldsCase() ) );
   }

   // Media is not a ROM: skip the folders the gamelist keeps its art in (when
   // they are subfolders at all) and the usual non-ROM extensions.
   QSet<QString> mediaDirs;
   for ( const QString& folder : { aList.imageFolder(), aList.videoFolder() } ) {
      const QString top = QDir::cleanPath( folder ).split( QLatin1Char( '/' ) ).value( 0 );
      if ( !top.isEmpty() && top != QLatin1String( "." ) )
         mediaDirs.insert( top.toLower() );
   }

   // ponytail: a denylist, since the per-system ROM extensions live in
   // EmulationStation's config, not here. A stray odd file shows up in the
   // list and gets unticked; Browse covers anything this hides.
   static const QSet<QString> notRoms = {
      QStringLiteral( "xml" ),  QStringLiteral( "txt" ),   QStringLiteral( "png" ),
      QStringLiteral( "jpg" ),  QStringLiteral( "jpeg" ),  QStringLiteral( "gif" ),
      QStringLiteral( "bmp" ),  QStringLiteral( "mp4" ),   QStringLiteral( "avi" ),
      QStringLiteral( "mkv" ),  QStringLiteral( "pdf" ),   QStringLiteral( "srm" ),
      QStringLiteral( "sav" ),  QStringLiteral( "state" ), QStringLiteral( "cfg" ),
      QStringLiteral( "ini" ),  QStringLiteral( "nfo" ),   QStringLiteral( "bak" ),
   };

   const QDir systemDir( aList.systemDir() );
   QStringList found;
   QDirIterator files( aList.systemDir(), QDir::Files, QDirIterator::Subdirectories );
   while ( files.hasNext() ) {
      const QString path = files.next();
      const QString relative = systemDir.relativeFilePath( path );

      if ( notRoms.contains( QFileInfo( path ).suffix().toLower() ) ||
           mediaDirs.contains( relative.section( QLatin1Char( '/' ), 0, 0 ).toLower() ) ||
           listed.contains( fileKey( path, aList.foldsCase() ) ) )
         continue;

      found << path;
   }

   found.sort( Qt::CaseInsensitive );
   return found;
}

QVector<MediaLink> unlinkedMedia( const Gamelist& aList )
{
   struct Kind
   {
      bool isImage;
      const QString& mime;
      QStringList folders;  // the gamelist's own first, then the usual ones
      const QStringList& stems;
   };
   const Kind kinds[] = {
      { true, Cst_ImageMime, QStringList( aList.imageFolder() ) + Cst_ImageSearchFolders,
        Cst_ImageStems },
      { false, Cst_VideoMime, QStringList( aList.videoFolder() ) + Cst_VideoSearchFolders,
        Cst_VideoStems },
   };

   QVector<MediaLink> found;
   for ( const Kind& kind : kinds ) {
      // One index per folder: the match is case-blind but the link keeps the
      // file's real case, which a case-sensitive FS needs.
      struct Folder
      {
         QDir dir;
         QHash<QString, QString> index;
      };
      QVector<Folder> folders;
      QStringList seen;
      for ( const QString& link : kind.folders ) {
         // An empty folder (a link with no '/') would be QDir(""): the working directory.
         const QString path = physicalPath( aList.systemDir(), link );
         if ( path.isEmpty() || seen.contains( path ) )
            continue;
         seen << path;

         Folder folder{ QDir( path ), {} };
         folder.index = mediaIndex( folder.dir, kind.mime );
         if ( !folder.index.isEmpty() )
            folders.append( folder );
      }
      if ( folders.isEmpty() )
         continue;

      for ( int index = 0; index < aList.count(); ++index ) {
         const Game& game = aList.at( index );
         if ( !( kind.isImage ? game.missingImage : game.missingVideo ) || game.romPath.isEmpty() )
            continue;

         // "Sonic" for Sonic.zip, and "Sonic.zip" itself: ES names the media of a
         // ScummVM "game" folder, wetlands-us.scummvm, after the whole name.
         const QStringList bases = { game.romNameWoExt().toLower(), game.romName().toLower() };
         for ( const Folder& folder : folders ) {
            const QString name = findMedia( folder.index, bases, kind.stems );
            if ( !name.isEmpty() ) {
               found.append( { index, kind.isImage, folder.dir.filePath( name ) } );
               break;
            }
         }
      }
   }

   return found;
}

QVector<MediaRename> misnamedMedia( const Gamelist& aList, bool aStrict )
{
   QVector<MediaRename> found;
   QSet<QString> claimed;  // lower-cased new paths, so two games cannot take one name

   for ( int index = 0; index < aList.count(); ++index ) {
      const Game& game = aList.at( index );
      if ( game.romPath.isEmpty() )
         continue;

      for ( const bool isImage : { true, false } ) {
         if ( isImage ? game.missingImage : game.missingVideo )
            continue;

         const QString path = isImage ? game.physicalImagePath : game.physicalVideoPath;
         const QFileInfo info( path );
         const QString stem = info.completeBaseName();

         bool named = false;
         const QStringList lenient = isImage ? Cst_ImageStems : Cst_VideoStems;
         for ( const QString& base : { game.romNameWoExt(), game.romName() } ) {
            for ( const QString& suffix : aStrict ? QStringList( QString() ) : lenient )
               named = named || stem == base + suffix;
         }
         if ( named )
            continue;

         MediaRename rename{ index, isImage, path, {}, {} };
         rename.newPath = info.dir().filePath(
            info.suffix().isEmpty() ? game.romNameWoExt()
                                    : game.romNameWoExt() + QLatin1Char( '.' ) + info.suffix() );

         // ponytail: a case-only rename on a case-folding filesystem is offered but
         // QFile::rename may refuse it; the whole batch then rolls back. Upgrade: two
         // steps through a temporary name.
         if ( aList.fileShared( index, path, isImage ) ) {
            rename.problem = QStringLiteral( "shared with another game" );
         } else if ( QFileInfo::exists( rename.newPath ) &&
                     QFileInfo( rename.newPath ).canonicalFilePath() != info.canonicalFilePath() ) {
            rename.problem = QStringLiteral( "a file with that name exists" );
         } else if ( claimed.contains( rename.newPath.toLower() ) ) {
            rename.problem = QStringLiteral( "another game is renamed to that name" );
         } else {
            claimed.insert( rename.newPath.toLower() );
         }

         found.append( rename );
      }
   }

   return found;
}

QVector<OversizedImage> oversizedImages( const Gamelist& aList, qint64 aMaxBytes )
{
   QVector<OversizedImage> found;
   QSet<QString> seen;

   for ( int index = 0; index < aList.count(); ++index ) {
      const QString& path = aList.at( index ).physicalImagePath;
      if ( path.isEmpty() || seen.contains( path ) )
         continue;
      seen.insert( path );

      const qint64 size = QFileInfo( path ).size();
      if ( size > aMaxBytes )
         found.append( { index, path, size } );
   }

   return found;
}

Result<qint64> shrinkImageFile( const QString& aPath, qint64 aMaxBytes )
{
   const auto failed = [&]( const QString& aMessage ) {
      return Error{ QStringLiteral( "%1: %2" ).arg( QFileInfo( aPath ).fileName(), aMessage ) };
   };

   // The reader is scoped so its file handle is closed before QSaveFile renames over
   // aPath: Windows refuses to replace a file that is still open.
   QByteArray format;
   QImage picture;
   {
      QImageReader reader( aPath );
      reader.setAutoTransform( false );
      format = reader.format();
      if ( reader.imageCount() > 1 )
         return failed( QStringLiteral( "animated pictures are left alone." ) );

      picture = reader.read();
      if ( picture.isNull() )
         return failed( reader.errorString() );
   }

   const qint64 original = QFileInfo( aPath ).size();
   if ( original <= aMaxBytes )
      return original;

   // Size follows the pixel count, so aim at the area ratio and correct from what
   // the encoder really produced. Always scaled from the original: scaling the
   // scaled copy over and over would blur it. 0.95 aims a little under the limit.
   double factor = std::sqrt( static_cast<double>( aMaxBytes ) / static_cast<double>( original ) ) * 0.95;
   QByteArray encoded;
   for ( int pass = 0; pass < 8; ++pass ) {
      const QSize size( qMax( 1, qRound( picture.width() * factor ) ),
                        qMax( 1, qRound( picture.height() * factor ) ) );
      const QImage scaled = picture.scaled( size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );

      encoded.clear();
      QBuffer buffer( &encoded );
      buffer.open( QIODevice::WriteOnly );
      QImageWriter writer( &buffer, format );
      if ( !writer.write( scaled ) )
         return failed( writer.errorString() );

      if ( encoded.size() <= aMaxBytes )
         break;

      factor *= std::sqrt( static_cast<double>( aMaxBytes ) / static_cast<double>( encoded.size() ) ) * 0.95;
      encoded.clear();
   }

   if ( encoded.isEmpty() )
      return failed( QStringLiteral( "cannot be made that small." ) );

   QSaveFile file( aPath );
   if ( !file.open( QIODevice::WriteOnly ) || file.write( encoded ) != encoded.size() ||
        !file.commit() )
      return failed( file.errorString() );

   return qint64( encoded.size() );
}
