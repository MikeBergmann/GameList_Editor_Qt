#include "Gamelist.h"

#include "PathUtil.h"

#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>

namespace {

// Where new media lands when the system has none yet to copy the layout from:
// the layout EmulationStation itself writes.
constexpr const char* Cst_DefaultImageFolder = "./media/images/";
constexpr const char* Cst_DefaultVideoFolder = "./media/videos/";

QString folderOf( const QString& aLink )
{
   return aLink.left( aLink.lastIndexOf( QLatin1Char( '/' ) ) + 1 );
}

// A missing node reads as empty.
QString childText( const QDomElement& aNode, const char* aName )
{
   return aNode.firstChildElement( QLatin1String( aName ) ).text();
}

// Where a file about to be written should land. The directory follows an existing
// "Media/" when the link says "media/", as far down as it exists. The file name
// stays literal on purpose: resolving it could land on, and overwrite, another
// game's differently-cased file.
QString writeTarget( const QString& aSystemDir, const QString& aLink )
{
   const QFileInfo info( QDir::cleanPath( QDir( aSystemDir ).filePath( aLink ) ) );
   return QDir( resolvePath( info.path() ) ).filePath( info.fileName() );
}

// Evaluated once at load, so filtering does not stat a file per row on every
// keystroke. An empty link counts as missing.
bool fileMissing( const QString& aPath )
{
   return aPath.isEmpty() || !QFileInfo::exists( aPath );
}

// New media is written next to its target under this name, then swapped in, so a
// half-written file never replaces a good one. The old file waits under the other
// name until the gamelist has been saved.
constexpr const char* Cst_StagedSuffix = ".gle-new";
constexpr const char* Cst_BackupSuffix = ".gle-bak";

// Moves aStaged to aTarget. An existing aTarget is moved aside first and its new name
// returned (empty if there was none). On an Error, aTarget is as it was and aStaged
// is gone.
Result<QString> swapIn( const QString& aStaged, const QString& aTarget )
{
   QString backup;

   if ( QFileInfo::exists( aTarget ) ) {
      backup = aTarget + QLatin1String( Cst_BackupSuffix );
      QFile::remove( backup );
      if ( !QFile::rename( aTarget, backup ) ) {
         QFile::remove( aStaged );
         return Error{ QStringLiteral( "Cannot replace %1." ).arg( aTarget ) };
      }
   }

   if ( !QFile::rename( aStaged, aTarget ) ) {
      if ( !backup.isEmpty() )
         QFile::rename( backup, aTarget );
      QFile::remove( aStaged );
      return Error{ QStringLiteral( "Cannot write %1." ).arg( aTarget ) };
   }

   return backup;
}

// Ratings are written with either decimal separator; an unparseable one counts as 0.
double ratingValue( const QString& aRating )
{
   QString text = aRating;
   return text.replace( QLatin1Char( ',' ), QLatin1Char( '.' ) ).toDouble();
}

QDate releaseDateValue( const QString& aDisplayDate )
{
   switch ( aDisplayDate.size() ) {
      case 4: return QDate::fromString( aDisplayDate, QStringLiteral( "yyyy" ) );
      case 7: return QDate::fromString( aDisplayDate, QStringLiteral( "MM/yyyy" ) );
      case 10: return QDate::fromString( aDisplayDate, QStringLiteral( "dd/MM/yyyy" ) );
      default: return {};
   }
}

}  // namespace

QString Gamelist::domText( int aIndex, const char* aName ) const
{
   return childText( FEntries.value( aIndex ).node, aName );
}

Result<> Gamelist::load( const SystemEntry& aSystem )
{
   FSystem = aSystem;
   FSystemDir.clear();
   FDocument = QDomDocument();
   FEntries.clear();
   FImageFolder = QLatin1String( Cst_DefaultImageFolder );
   FVideoFolder = QLatin1String( Cst_DefaultVideoFolder );
   FNameCounts.clear();
   FRomCounts.clear();

   QFile file( aSystem.gamelistPath );
   if ( !file.open( QIODevice::ReadOnly ) )
      return Error{ QStringLiteral( "Cannot read %1: %2" )
                       .arg( aSystem.gamelistPath, file.errorString() ) };

   QString parseError;
   int line = 0;
   int column = 0;
   if ( !FDocument.setContent( &file, &parseError, &line, &column ) ) {
      return Error{ QStringLiteral( "%1 is not valid XML: %2 (line %3, column %4)" )
                       .arg( aSystem.gamelistPath, parseError )
                       .arg( line )
                       .arg( column ) };
   }

   FSystemDir = QFileInfo( aSystem.gamelistPath ).absolutePath();
   bool imageFolderFound = false;
   bool videoFolderFound = false;

   for ( QDomElement node =
            FDocument.documentElement().firstChildElement( QLatin1String( Cst_Game ) );
         !node.isNull(); node = node.nextSiblingElement( QLatin1String( Cst_Game ) ) ) {

      if ( !node.hasChildNodes() )
         continue;

      const Game game = gameFromNode( node );

      if ( !imageFolderFound && !game.imagePath.isEmpty() ) {
         FImageFolder = folderOf( game.imagePath );
         imageFolderFound = true;
      }
      if ( !videoFolderFound && !game.videoPath.isEmpty() ) {
         FVideoFolder = folderOf( game.videoPath );
         videoFolderFound = true;
      }

      FEntries.append( { game, node } );
   }

   if ( FEntries.isEmpty() )
      return Error{ QStringLiteral( "%1 holds no game." ).arg( aSystem.gamelistPath ) };

   countDuplicates();

   return {};
}

Game Gamelist::gameFromNode( const QDomElement& aNode ) const
{
   Game game;
   game.romPath = childText( aNode, Cst_Path );
   game.name = childText( aNode, Cst_Name );
   game.description = childText( aNode, Cst_Description );
   game.imagePath = childText( aNode, Cst_ImageLink );
   game.videoPath = childText( aNode, Cst_VideoLink );
   game.rating = childText( aNode, Cst_Rating );
   game.releaseDate = gamelistDateToDisplay( childText( aNode, Cst_ReleaseDate ) );
   game.developer = childText( aNode, Cst_Developer );
   game.publisher = childText( aNode, Cst_Publisher );
   game.genre = childText( aNode, Cst_Genre );
   game.players = childText( aNode, Cst_Players );
   game.region = childText( aNode, Cst_Region );
   game.playcount = childText( aNode, Cst_Playcount );
   game.lastplayed = childText( aNode, Cst_LastPlayed );
   game.kidGame = gamelistFlag( childText( aNode, Cst_KidGame ) );
   game.hidden = gamelistFlag( childText( aNode, Cst_Hidden ) );
   game.favorite = gamelistFlag( childText( aNode, Cst_Favorite ) );

   game.physicalRomPath = physicalPath( FSystemDir, game.romPath );
   game.physicalImagePath = physicalPath( FSystemDir, game.imagePath );
   game.physicalVideoPath = physicalPath( FSystemDir, game.videoPath );
   game.isOrphan = fileMissing( game.physicalRomPath );
   game.missingImage = fileMissing( game.physicalImagePath );
   game.missingVideo = fileMissing( game.physicalVideoPath );
   return game;
}

QStringList Gamelist::unlistedRoms() const
{
   // Case-folded absolute paths of everything the gamelist already points at.
   const auto key = [this]( const QString& aPath ) {
      return QDir::cleanPath( QDir( FSystemDir ).filePath( aPath ) ).toLower();
   };

   QSet<QString> listed;
   for ( const Entry& entry : FEntries )
      listed.insert( key( entry.game.romPath ) );

   // Media is not a ROM: skip the folders the gamelist keeps its art in (when
   // they are subfolders at all) and the usual non-ROM extensions.
   QSet<QString> mediaDirs;
   for ( const QString& folder : { FImageFolder, FVideoFolder } ) {
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

   const QDir systemDir( FSystemDir );
   QStringList found;
   QDirIterator files( FSystemDir, QDir::Files, QDirIterator::Subdirectories );
   while ( files.hasNext() ) {
      const QString path = files.next();
      const QString relative = systemDir.relativeFilePath( path );

      if ( notRoms.contains( QFileInfo( path ).suffix().toLower() ) ||
           mediaDirs.contains( relative.section( QLatin1Char( '/' ), 0, 0 ).toLower() ) ||
           listed.contains( key( path ) ) )
         continue;

      found << path;
   }

   found.sort( Qt::CaseInsensitive );
   return found;
}

Result<int> Gamelist::addGames( const QStringList& aRomPaths )
{
   const QDir systemDir( FSystemDir );
   QSet<QString> seen;
   for ( const Entry& entry : FEntries )
      seen.insert( QDir::cleanPath( systemDir.filePath( entry.game.romPath ) ).toLower() );

   const int firstNew = static_cast<int>( FEntries.size() );
   QVector<QDomElement> added;

   const auto rollback = [&] {
      for ( QDomElement node : std::as_const( added ) )
         FDocument.documentElement().removeChild( node );
      FEntries.resize( firstNew );
   };

   for ( const QString& path : aRomPaths ) {
      const QFileInfo info( path );
      const QString relative = systemDir.relativeFilePath( info.absoluteFilePath() );

      if ( !info.isFile() || relative.startsWith( QLatin1String( ".." ) ) ) {
         rollback();
         return Error{ QStringLiteral( "%1 is not a file inside %2." ).arg( path, FSystemDir ) };
      }

      // Already listed, or picked twice: nothing to add, and not an error.
      const QString key = QDir::cleanPath( info.absoluteFilePath() ).toLower();
      if ( seen.contains( key ) )
         continue;
      seen.insert( key );

      QDomElement node = FDocument.createElement( QLatin1String( Cst_Game ) );
      FDocument.documentElement().appendChild( node );
      setChildText( node, Cst_Path, QStringLiteral( "./" ) + relative );
      setChildText( node, Cst_Name, info.completeBaseName() );

      added << node;
      FEntries.append( { gameFromNode( node ), node } );
   }

   if ( added.isEmpty() )
      return 0;

   if ( const Result<> saved = save(); !saved ) {
      rollback();
      return Error{ saved.error() };
   }

   countDuplicates();
   return static_cast<int>( added.size() );
}

void Gamelist::countDuplicates()
{
   FNameCounts.clear();
   FRomCounts.clear();

   for ( const Entry& entry : FEntries ) {
      ++FNameCounts[entry.game.name];
      ++FRomCounts[entry.game.romName()];
   }
}

Result<> Gamelist::save()
{
   QSaveFile file( FSystem.gamelistPath );
   if ( !file.open( QIODevice::WriteOnly ) )
      return Error{ QStringLiteral( "Cannot write %1: %2" )
                       .arg( FSystem.gamelistPath, file.errorString() ) };

   // The whole document is re-indented; three spaces is what EmulationStation writes.
   const QByteArray content = FDocument.toByteArray( 3 );
   if ( file.write( content ) != content.size() ) {
      file.cancelWriting();
      return Error{ QStringLiteral( "Cannot write %1: %2" )
                       .arg( FSystem.gamelistPath, file.errorString() ) };
   }

   if ( !file.commit() )
      return Error{ QStringLiteral( "Cannot write %1: %2" )
                       .arg( FSystem.gamelistPath, file.errorString() ) };

   return {};
}

QDomElement Gamelist::ensureChild( QDomElement aNode, const char* aName )
{
   QDomElement child = aNode.firstChildElement( QLatin1String( aName ) );
   if ( child.isNull() ) {
      child = FDocument.createElement( QLatin1String( aName ) );
      aNode.appendChild( child );
   }

   return child;
}

void Gamelist::setChildText( QDomElement aNode, const char* aName, const QString& aText )
{
   QDomElement child = ensureChild( aNode, aName );

   // Replace whatever was in there rather than editing the first text node:
   // <desc> is a CDATA section in some gamelists, and may hold several nodes.
   while ( !child.firstChild().isNull() )
      child.removeChild( child.firstChild() );

   if ( !aText.isEmpty() )
      child.appendChild( FDocument.createTextNode( aText ) );
}

void Gamelist::setFields( const QVector<int>& aTargets, const GameFields& aValues,
                          GameFieldSet aWhich )
{
   if ( !aWhich )
      return;

   for ( int index : aTargets ) {
      if ( index < 0 || index >= FEntries.size() )
         continue;

      Game& game = FEntries[index].game;
      QDomElement node = FEntries[index].node;

      const auto write = [&]( GameField aField, const char* aName, const QString& aValue,
                              QString& aStored ) {
         if ( !( aWhich & aField ) )
            return;
         setChildText( node, aName, aValue );
         aStored = aValue;
      };

      write( gfName, Cst_Name, aValues.name, game.name );
      write( gfGenre, Cst_Genre, aValues.genre, game.genre );
      write( gfRating, Cst_Rating, aValues.rating, game.rating );
      write( gfPlayers, Cst_Players, aValues.players, game.players );
      write( gfDeveloper, Cst_Developer, aValues.developer, game.developer );
      write( gfPublisher, Cst_Publisher, aValues.publisher, game.publisher );
      write( gfDescription, Cst_Description, aValues.description, game.description );
      write( gfRegion, Cst_Region, aValues.region, game.region );

      if ( aWhich & gfReleaseDate ) {
         // A date in none of the three accepted forms is stored blank and comes
         // back blank.
         const QString stored = displayDateToGamelist( aValues.releaseDate );
         setChildText( node, Cst_ReleaseDate, stored );
         game.releaseDate = stored.isEmpty() ? QString() : aValues.releaseDate;
      }

      const auto writeFlag = [&]( GameField aField, const char* aName, int aValue, int& aStored ) {
         if ( !( aWhich & aField ) )
            return;
         setChildText( node, aName, QLatin1String( aValue == 0 ? Cst_False : Cst_True ) );
         aStored = aValue;
      };

      writeFlag( gfKidGame, Cst_KidGame, aValues.kidGame, game.kidGame );
      writeFlag( gfHidden, Cst_Hidden, aValues.hidden, game.hidden );
      writeFlag( gfFavorite, Cst_Favorite, aValues.favorite, game.favorite );
   }

   if ( aWhich & gfName )
      countDuplicates();
}

Result<Gamelist::MediaWrite> Gamelist::applyImage( int aIndex, const QImage& aPicture )
{
   Game& game = FEntries[aIndex].game;
   const QString link = FImageFolder + game.romNameWoExt() + QLatin1String( Cst_ImageSuffixPng );
   const QString target = writeTarget( FSystemDir, link );

   if ( !QDir().mkpath( QFileInfo( target ).path() ) )
      return Error{ QStringLiteral( "Cannot create %1." ).arg( QFileInfo( target ).path() ) };

   // The suffix would pick the format, but the staged name has a foreign one: always PNG.
   const QString staged = target + QLatin1String( Cst_StagedSuffix );
   QFile::remove( staged );
   if ( !aPicture.save( staged, "PNG" ) ) {
      QFile::remove( staged );
      return Error{ QStringLiteral( "Cannot write %1." ).arg( target ) };
   }

   MediaWrite write;
   write.index = aIndex;
   write.isImage = true;
   write.target = target;
   write.before = game;
   const Result<QString> backup = swapIn( staged, target );
   if ( !backup )
      return Error{ backup.error() };
   write.backup = backup.value();

   setChildText( FEntries[aIndex].node, Cst_ImageLink, link );
   game.imagePath = link;
   game.physicalImagePath = target;
   game.missingImage = false;

   return write;
}

void Gamelist::restoreLink( int aIndex, bool aIsImage, const Game& aBefore )
{
   // ponytail: a game that had no <image>/<video> element ends up with an empty one;
   // EmulationStation reads that as unset. Upgrade: remember the node and remove it.
   FEntries[aIndex].game = aBefore;
   setChildText( FEntries[aIndex].node, aIsImage ? Cst_ImageLink : Cst_VideoLink,
                 aIsImage ? aBefore.imagePath : aBefore.videoPath );
}

void Gamelist::undoMedia( const MediaWrite& aWrite )
{
   if ( !aWrite.target.isEmpty() ) {
      QFile::remove( aWrite.target );
      if ( !aWrite.backup.isEmpty() )
         QFile::rename( aWrite.backup, aWrite.target );
   }
   restoreLink( aWrite.index, aWrite.isImage, aWrite.before );
}

Result<> Gamelist::commitMedia( const QVector<MediaWrite>& aWrites )
{
   if ( const Result<> saved = save(); !saved ) {
      for ( auto it = aWrites.crbegin(); it != aWrites.crend(); ++it )
         undoMedia( *it );
      return saved;
   }

   for ( const MediaWrite& write : aWrites ) {
      if ( !write.backup.isEmpty() )
         QFile::remove( write.backup );
   }
   return {};
}

Result<> Gamelist::setImage( int aIndex, const QString& aSourcePath )
{
   if ( aIndex < 0 || aIndex >= FEntries.size() )
      return Error{ QStringLiteral( "No game selected." ) };

   QImage picture;
   if ( !picture.load( aSourcePath ) ) {
      return Error{
         QStringLiteral( "%1 is not a picture this build can read." ).arg( aSourcePath ) };
   }

   return setImage( aIndex, picture );
}

Result<> Gamelist::setImage( int aIndex, const QImage& aPicture )
{
   if ( aIndex < 0 || aIndex >= FEntries.size() )
      return Error{ QStringLiteral( "No game selected." ) };

   const Result<MediaWrite> write = applyImage( aIndex, aPicture );
   if ( !write )
      return Error{ write.error() };

   return commitMedia( { write.value() } );
}

Result<int> Gamelist::setDefaultImageForMissing( const QString& aSourcePath )
{
   QImage picture;
   if ( !picture.load( aSourcePath ) )
      return Error{
         QStringLiteral( "%1 is not a picture this build can read." ).arg( aSourcePath ) };

   // All or nothing: half a batch of pictures with no way back is worse than none.
   QVector<MediaWrite> writes;
   for ( int index = 0; index < FEntries.size(); ++index ) {
      if ( !FEntries.at( index ).game.missingImage )
         continue;

      const Result<MediaWrite> write = applyImage( index, picture );
      if ( !write ) {
         for ( auto it = writes.crbegin(); it != writes.crend(); ++it )
            undoMedia( *it );
         return Error{ write.error() };
      }
      writes.append( write.value() );
   }

   if ( writes.isEmpty() )
      return 0;

   if ( const Result<> committed = commitMedia( writes ); !committed )
      return Error{ committed.error() };

   return static_cast<int>( writes.size() );
}

Result<int> Gamelist::linkMedia( const QVector<MediaLink>& aLinks )
{
   const QVector<Entry> before = FEntries;
   const QDir systemDir( FSystemDir );

   int linked = 0;
   for ( const MediaLink& link : aLinks ) {
      if ( link.index < 0 || link.index >= FEntries.size() )
         continue;

      Game& game = FEntries[link.index].game;
      const QString text = QStringLiteral( "./" ) + systemDir.relativeFilePath( link.path );
      setChildText( FEntries[link.index].node, link.isImage ? Cst_ImageLink : Cst_VideoLink, text );
      ( link.isImage ? game.imagePath : game.videoPath ) = text;
      ( link.isImage ? game.physicalImagePath : game.physicalVideoPath ) = link.path;
      ( link.isImage ? game.missingImage : game.missingVideo ) = false;
      ++linked;
   }

   if ( linked == 0 )
      return 0;

   if ( const Result<> saved = save(); !saved ) {
      for ( const MediaLink& link : aLinks ) {
         if ( link.index < 0 || link.index >= FEntries.size() )
            continue;
         const Game& old = before.at( link.index ).game;
         setChildText( FEntries[link.index].node, link.isImage ? Cst_ImageLink : Cst_VideoLink,
                       link.isImage ? old.imagePath : old.videoPath );
      }
      FEntries = before;
      return Error{ saved.error() };
   }

   return linked;
}

Result<int> Gamelist::renameMedia( const QVector<MediaRename>& aRenames )
{
   const QVector<Entry> before = FEntries;

   QVector<const MediaRename*> done;
   auto undo = [&]() {
      for ( auto it = done.crbegin(); it != done.crend(); ++it )
         QFile::rename( ( *it )->newPath, ( *it )->path );
      for ( const MediaRename* rename : done ) {
         const Game& old = before.at( rename->index ).game;
         setChildText( FEntries[rename->index].node, rename->isImage ? Cst_ImageLink : Cst_VideoLink,
                       rename->isImage ? old.imagePath : old.videoPath );
      }
      FEntries = before;
   };

   for ( const MediaRename& rename : aRenames ) {
      if ( rename.index < 0 || rename.index >= FEntries.size() || !rename.problem.isEmpty() )
         continue;

      if ( !QFile::rename( rename.path, rename.newPath ) ) {
         undo();
         return Error{
            QStringLiteral( "Cannot rename %1 to %2." ).arg( rename.path, rename.newPath ) };
      }
      done << &rename;

      // Only the file name changes: the folder part of the link stays as written.
      Game& game = FEntries[rename.index].game;
      const QString oldLink = rename.isImage ? game.imagePath : game.videoPath;
      const QString text = folderOf( oldLink ) + QFileInfo( rename.newPath ).fileName();
      setChildText( FEntries[rename.index].node, rename.isImage ? Cst_ImageLink : Cst_VideoLink, text );
      ( rename.isImage ? game.imagePath : game.videoPath ) = text;
      ( rename.isImage ? game.physicalImagePath : game.physicalVideoPath ) = rename.newPath;
   }

   if ( done.isEmpty() )
      return 0;

   if ( const Result<> saved = save(); !saved ) {
      undo();
      return Error{ saved.error() };
   }

   return static_cast<int>( done.size() );
}

Result<> Gamelist::setVideo( int aIndex, const QString& aSourcePath )
{
   if ( aIndex < 0 || aIndex >= FEntries.size() )
      return Error{ QStringLiteral( "No game selected." ) };

   Game& game = FEntries[aIndex].game;
   const QString link = FVideoFolder + game.romNameWoExt() + QLatin1String( Cst_VideoSuffixMp4 );
   const QString target = writeTarget( FSystemDir, link );

   if ( !QDir().mkpath( QFileInfo( target ).path() ) )
      return Error{ QStringLiteral( "Cannot create %1." ).arg( QFileInfo( target ).path() ) };

   // Choosing the game's own video would otherwise remove the file and then copy
   // from it, i.e. a deletion; the copy is skipped when source and target are one file.
   const QString source = QFileInfo( aSourcePath ).canonicalFilePath();
   if ( source.isEmpty() )
      return Error{ QStringLiteral( "%1 cannot be read." ).arg( aSourcePath ) };

   MediaWrite write;
   write.index = aIndex;
   write.isImage = false;
   write.before = game;

   if ( source != QFileInfo( target ).canonicalFilePath() ) {
      // Copy beside the target and swap, so a failed copy cannot cost the old video.
      const QString staged = target + QLatin1String( Cst_StagedSuffix );
      QFile::remove( staged );
      if ( !QFile::copy( source, staged ) ) {
         QFile::remove( staged );
         return Error{ QStringLiteral( "Cannot copy %1 to %2." ).arg( source, target ) };
      }

      const Result<QString> backup = swapIn( staged, target );
      if ( !backup )
         return Error{ backup.error() };
      write.backup = backup.value();
      write.target = target;
   }

   setChildText( FEntries[aIndex].node, Cst_VideoLink, link );
   game.videoPath = link;
   game.physicalVideoPath = target;
   game.missingVideo = false;

   return commitMedia( { write } );
}

bool Gamelist::fileShared( int aIndex, const QString& aPhysicalPath, bool aIsImage ) const
{
   if ( aPhysicalPath.isEmpty() )
      return false;

   for ( int index = 0; index < FEntries.size(); ++index ) {
      if ( index == aIndex )
         continue;
      const Game& other = FEntries.at( index ).game;
      if ( ( aIsImage ? other.physicalImagePath : other.physicalVideoPath ) == aPhysicalPath )
         return true;
   }

   return false;
}

Result<> Gamelist::removeImage( int aIndex )
{
   if ( aIndex < 0 || aIndex >= FEntries.size() )
      return Error{ QStringLiteral( "No game selected." ) };

   Game& game = FEntries[aIndex].game;
   const QString file = game.physicalImagePath;
   const Game before = game;

   setChildText( FEntries[aIndex].node, Cst_ImageLink, QString() );
   game.imagePath.clear();
   game.physicalImagePath.clear();
   game.missingImage = true;

   if ( const Result<> saved = save(); !saved ) {
      restoreLink( aIndex, true, before );
      return saved;
   }

   // Only once the gamelist has stopped pointing at it, and only if no other
   // game points at it either - several games can share one picture.
   if ( !file.isEmpty() && !fileShared( aIndex, file, true ) )
      QFile::remove( file );

   return {};
}

Result<> Gamelist::removeVideo( int aIndex )
{
   if ( aIndex < 0 || aIndex >= FEntries.size() )
      return Error{ QStringLiteral( "No game selected." ) };

   Game& game = FEntries[aIndex].game;
   const QString file = game.physicalVideoPath;
   const Game before = game;

   setChildText( FEntries[aIndex].node, Cst_VideoLink, QString() );
   game.videoPath.clear();
   game.physicalVideoPath.clear();
   game.missingVideo = true;

   if ( const Result<> saved = save(); !saved ) {
      restoreLink( aIndex, false, before );
      return saved;
   }

   if ( !file.isEmpty() && !fileShared( aIndex, file, false ) )
      QFile::remove( file );

   return {};
}

Result<QStringList> Gamelist::removeGame( int aIndex )
{
   if ( aIndex < 0 || aIndex >= FEntries.size() )
      return Error{ QStringLiteral( "No game selected." ) };

   QDomElement node = FEntries.at( aIndex ).node;
   QDomNode parent = node.parentNode();
   const QDomNode next = node.nextSibling();
   parent.removeChild( node );

   if ( const Result<> saved = save(); !saved ) {
      parent.insertBefore( node, next );
      return Error{ saved.error() };
   }

   const Game game = FEntries.at( aIndex ).game;
   FEntries.remove( aIndex );
   countDuplicates();

   // Only once the gamelist has stopped listing it, and not what another entry
   // (a duplicate, a shared picture) still points at.
   const auto shared = [this]( const QString& aPath, QString Game::*aMember ) {
      for ( const Entry& other : std::as_const( FEntries ) ) {
         if ( other.game.*aMember == aPath )
            return true;
      }
      return false;
   };

   QStringList stuck;
   const auto drop = [&]( const QString& aPath, QString Game::*aMember ) {
      if ( aPath.isEmpty() || !QFileInfo::exists( aPath ) || shared( aPath, aMember ) )
         return;
      if ( !QFile::remove( aPath ) )
         stuck << aPath;
   };

   drop( game.physicalRomPath, &Game::physicalRomPath );
   drop( game.physicalImagePath, &Game::physicalImagePath );
   drop( game.physicalVideoPath, &Game::physicalVideoPath );

   return stuck;
}

Result<> Gamelist::removeGames( const QVector<int>& aIndexes )
{
   QSet<int> doomed;
   for ( int index : aIndexes ) {
      if ( index >= 0 && index < FEntries.size() )
         doomed.insert( index );
   }
   if ( doomed.isEmpty() )
      return {};

   struct Removed
   {
      QDomElement node;
      QDomNode parent;
      QDomNode next;
   };
   QVector<Removed> removed;
   for ( int index = 0; index < FEntries.size(); ++index ) {
      if ( !doomed.contains( index ) )
         continue;

      const QDomElement node = FEntries.at( index ).node;
      removed.append( { node, node.parentNode(), node.nextSibling() } );
      removed.last().parent.removeChild( node );
   }

   if ( const Result<> saved = save(); !saved ) {
      // Backwards, so every remembered neighbour is back in place first.
      for ( auto it = removed.crbegin(); it != removed.crend(); ++it ) {
         QDomNode parent = it->parent;
         parent.insertBefore( it->node, it->next );
      }
      return saved;
   }

   QVector<Entry> kept;
   for ( int index = 0; index < FEntries.size(); ++index ) {
      if ( !doomed.contains( index ) )
         kept.append( FEntries.at( index ) );
   }
   FEntries = kept;
   countDuplicates();
   return {};
}

QVector<int> Gamelist::orphanIndexes() const
{
   QVector<int> found;
   for ( int index = 0; index < FEntries.size(); ++index ) {
      if ( FEntries.at( index ).game.isOrphan )
         found.append( index );
   }
   return found;
}

QVector<int> Gamelist::duplicateIndexes() const
{
   // physicalRomPath already has its case settled against the disk, so two links
   // to one file match and two files differing only in case do not.
   QSet<QString> seen;
   QVector<int> found;
   for ( int index = 0; index < FEntries.size(); ++index ) {
      const QString& path = FEntries.at( index ).game.physicalRomPath;
      if ( path.isEmpty() )
         continue;
      if ( seen.contains( path ) )
         found.append( index );
      else
         seen.insert( path );
   }
   return found;
}

void Gamelist::ensureHashes( int aIndex )
{
   if ( aIndex < 0 || aIndex >= FEntries.size() )
      return;

   Game& game = FEntries[aIndex].game;

   // An orphan (unreadable ROM) hashes to the empty string.
   if ( game.md5.isEmpty() )
      game.md5 = fileHash( game.physicalRomPath, QCryptographicHash::Md5 );
   if ( game.sha1.isEmpty() )
      game.sha1 = fileHash( game.physicalRomPath, QCryptographicHash::Sha1 );
   if ( game.crc32.isEmpty() )
      game.crc32 = fileCrc32( game.physicalRomPath );
}

bool Gamelist::accepts( int aIndex, const FilterSpec& aFilter ) const
{
   if ( aIndex < 0 || aIndex >= FEntries.size() )
      return false;

   if ( !matchesCategory( FEntries.at( aIndex ).game, aFilter ) )
      return false;

   // Case-insensitive, against whatever the list displays.
   return aFilter.search.isEmpty() ||
          displayName( aIndex, aFilter ).contains( aFilter.search, Qt::CaseInsensitive );
}

QString Gamelist::displayName( int aIndex, const FilterSpec& aFilter ) const
{
   const Game& game = FEntries.at( aIndex ).game;

   if ( !aFilter.listByRom )
      return game.name;
   if ( !aFilter.fullRomName )
      return game.romName();

   QString path = game.romPath;
   if ( path.startsWith( QLatin1String( "./" ) ) )
      path.remove( 0, 2 );
   return path;
}

bool Gamelist::matchesCategory( const Game& aGame, const FilterSpec& aFilter ) const
{
   // The "same as" categories compare against the selected game; with none selected
   // nothing matches.
   const bool needsReference = filterDependsOnSelection( aFilter.category );
   if ( needsReference && ( aFilter.reference < 0 || aFilter.reference >= FEntries.size() ) )
      return false;

   // Only read by the categories above, for which it is valid. No default in the
   // switch below, so the compiler flags a category added to the enum and not handled.
   const Game& reference = needsReference ? FEntries.at( aFilter.reference ).game : aGame;

   switch ( aFilter.category ) {
      case FilterCategory::All: return true;
      case FilterCategory::MissingImage: return aGame.missingImage;
      case FilterCategory::MissingVideo: return aGame.missingVideo;
      case FilterCategory::MissingDate: return aGame.releaseDate.isEmpty();
      case FilterCategory::MissingPlayers: return aGame.players.isEmpty();
      case FilterCategory::MissingRating: return aGame.rating.isEmpty();
      case FilterCategory::MissingDeveloper: return aGame.developer.isEmpty();
      case FilterCategory::MissingPublisher: return aGame.publisher.isEmpty();
      case FilterCategory::MissingDescription: return aGame.description.isEmpty();
      case FilterCategory::MissingGenre: return aGame.genre.isEmpty();
      case FilterCategory::MissingRegion: return aGame.region.isEmpty();
      case FilterCategory::KidGame: return aGame.kidGame == 1;
      case FilterCategory::Hidden: return aGame.hidden == 1;
      case FilterCategory::Favorite: return aGame.favorite == 1;
      case FilterCategory::Orphan: return aGame.isOrphan;
      case FilterCategory::DuplicateNames: return FNameCounts.value( aGame.name ) > 1;
      case FilterCategory::DuplicateRoms: return FRomCounts.value( aGame.romName() ) > 1;

      case FilterCategory::SameRegion: return aGame.region == reference.region;
      case FilterCategory::SameDate: return aGame.releaseDate == reference.releaseDate;
      case FilterCategory::SameOrEarlierDate:
      case FilterCategory::SameOrLaterDate: {
         if ( aGame.releaseDate.isEmpty() )
            return false;
         // An unset reference date counts as today.
         QDate referenceDate = releaseDateValue( reference.releaseDate );
         if ( !referenceDate.isValid() )
            referenceDate = QDate::currentDate();
         const QDate date = releaseDateValue( aGame.releaseDate );
         return aFilter.category == FilterCategory::SameOrEarlierDate ? date <= referenceDate
                                                                      : date >= referenceDate;
      }
      case FilterCategory::SamePlayers: return aGame.players == reference.players;
      case FilterCategory::SameRating: return aGame.rating == reference.rating;
      case FilterCategory::SameOrLowerRating:
         return !aGame.rating.isEmpty() &&
                ratingValue( aGame.rating ) <= ratingValue( reference.rating );
      case FilterCategory::SameOrHigherRating:
         return !aGame.rating.isEmpty() &&
                ratingValue( aGame.rating ) >= ratingValue( reference.rating );
      case FilterCategory::SamePublisher: return aGame.publisher == reference.publisher;
      case FilterCategory::SameDeveloper: return aGame.developer == reference.developer;
      case FilterCategory::SameGenre: return aGame.genre == reference.genre;
      // Compared as written, not case-insensitively: on Linux two distinct directories
      // can differ only in case. The case is already settled against the disk by
      // resolvePath.
      case FilterCategory::SameFolder:
         return QFileInfo( aGame.physicalRomPath ).path() ==
                QFileInfo( reference.physicalRomPath ).path();
      case FilterCategory::SameName: return aGame.name == reference.name;
      case FilterCategory::SameRom: return aGame.romName() == reference.romName();

      case FilterCategory::Count: break;
   }

   return false;
}
