#include "Gamelist.h"

#include "PathUtil.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>

namespace {

// The layout EmulationStation writes, for systems with no media yet.
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

// The directory follows existing case ("Media/" for "media/"). The file name stays
// literal: resolving it could overwrite another game's differently-cased file.
QString writeTarget( const QString& aSystemDir, const QString& aLink )
{
   const QFileInfo info( QDir::cleanPath( QDir( aSystemDir ).filePath( aLink ) ) );
   return QDir( resolvePath( info.path() ) ).filePath( info.fileName() );
}


// An empty link counts as missing.
bool fileMissing( const QString& aPath )
{
   return aPath.isEmpty() || !QFileInfo::exists( aPath );
}

// New media is written under the staged name, then swapped in; the old file waits
// under the backup name until the gamelist is saved.
constexpr const char* Cst_StagedSuffix = ".gle-new";
constexpr const char* Cst_BackupSuffix = ".gle-bak";

// Returns the backup name of a replaced aTarget (empty if none). On an Error,
// aTarget is as it was and aStaged is gone.
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

}  // namespace

QString Gamelist::domText( int aIndex, const char* aName ) const
{
   return childText( FEntries.value( aIndex ).node, aName );
}

Result<> Gamelist::load( const SystemEntry& aSystem )
{
   FSystem = aSystem;
   FBackedUp = false;
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
   FFoldsCase = volumeFoldsCase( aSystem.gamelistPath );
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

Result<int> Gamelist::addGames( const QStringList& aRomPaths )
{
   const QDir systemDir( FSystemDir );
   QSet<QString> seen;
   for ( const Entry& entry : FEntries ) {
      if ( !entry.game.physicalRomPath.isEmpty() )
         seen.insert( fileKey( entry.game.physicalRomPath, FFoldsCase ) );
   }

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
      const QString key = fileKey( info.absoluteFilePath(), FFoldsCase );
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
      return saved;
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

Result<> Gamelist::checkIndex( int aIndex ) const
{
   if ( aIndex < 0 || aIndex >= FEntries.size() )
      return Error{ QStringLiteral( "No game selected." ) };
   return {};
}

Result<> Gamelist::save()
{
   // ponytail: one copy per load, never pruned; cap or age them out if they pile up.
   if ( !FBackedUp ) {
      const QString backup =
         FSystem.gamelistPath + QDateTime::currentDateTime().toString( ".yyyyMMdd-HHmmss" ) + ".bak";
      if ( !QFile::exists( backup ) && !QFile::copy( FSystem.gamelistPath, backup ) )
         return Error{ QStringLiteral( "Cannot back up %1 to %2" )
                          .arg( FSystem.gamelistPath, backup ) };
      FBackedUp = true;
   }

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

// Sonic.zip and Sonic.7z share a stem: keep the ROM's extension, then count up.
QString Gamelist::freeMediaLink( int aIndex, bool aIsImage ) const
{
   const Game& game = FEntries.at( aIndex ).game;
   const QString folder = aIsImage ? FImageFolder : FVideoFolder;
   const QLatin1String suffix( aIsImage ? Cst_ImageSuffixPng : Cst_VideoSuffixMp4 );

   for ( int attempt = 0;; ++attempt ) {
      const QString stem = attempt == 0   ? game.romNameWoExt()
                           : attempt == 1 ? game.romName()
                                          : game.romNameWoExt() + QLatin1Char( '_' ) +
                                               QString::number( attempt );
      const QString link = folder + stem + suffix;
      const QString target = writeTarget( FSystemDir, link );

      // ponytail: a scan of every game per attempt, so a ChangeAll over N games is O(N^2)
      // (~25M compares at 5000). Upgrade: a set of the taken keys, built once per batch.
      bool taken = false;
      for ( int index = 0; index < FEntries.size() && !taken; ++index ) {
         const Game& other = FEntries.at( index ).game;
         taken = index != aIndex &&
                 sameFile( aIsImage ? other.physicalImagePath : other.physicalVideoPath, target,
                           FFoldsCase );
      }

      if ( !taken )
         return link;
   }
}

void Gamelist::setMediaLink( int aIndex, bool aIsImage, const QString& aLink,
                             const QString& aPhysical )
{
   Entry& entry = FEntries[aIndex];
   Game& game = entry.game;
   setChildText( entry.node, aIsImage ? Cst_ImageLink : Cst_VideoLink, aLink );
   ( aIsImage ? game.imagePath : game.videoPath ) = aLink;
   ( aIsImage ? game.physicalImagePath : game.physicalVideoPath ) = aPhysical;
   ( aIsImage ? game.missingImage : game.missingVideo ) = aPhysical.isEmpty();
}

Result<Gamelist::MediaWrite> Gamelist::applyImage( int aIndex, const QImage& aPicture )
{
   Game& game = FEntries[aIndex].game;
   const QString link = freeMediaLink( aIndex, true );
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
      return backup;
   write.backup = backup.value();

   setMediaLink( aIndex, true, link, target );

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
      if ( !aWrite.movedFrom.isEmpty() ) {
         QFile::rename( aWrite.target, aWrite.movedFrom );
      } else {
         QFile::remove( aWrite.target );
         if ( !aWrite.backup.isEmpty() )
            QFile::rename( aWrite.backup, aWrite.target );
      }
   }
   restoreLink( aWrite.index, aWrite.isImage, aWrite.before );
}

void Gamelist::rollbackMedia( const QVector<MediaWrite>& aWrites )
{
   for ( auto it = aWrites.crbegin(); it != aWrites.crend(); ++it )
      undoMedia( *it );
}

Result<> Gamelist::commitMedia( const QVector<MediaWrite>& aWrites )
{
   if ( const Result<> saved = save(); !saved ) {
      rollbackMedia( aWrites );
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
   if ( const Result<> valid = checkIndex( aIndex ); !valid )
      return valid;

   QImage picture;
   if ( !picture.load( aSourcePath ) ) {
      return Error{
         QStringLiteral( "%1 is not a picture this build can read." ).arg( aSourcePath ) };
   }

   return setImage( aIndex, picture );
}

Result<> Gamelist::setImage( int aIndex, const QImage& aPicture )
{
   if ( const Result<> valid = checkIndex( aIndex ); !valid )
      return valid;

   const Result<MediaWrite> write = applyImage( aIndex, aPicture );
   if ( !write )
      return write;

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
         rollbackMedia( writes );
         return write;
      }
      writes.append( write.value() );
   }

   if ( writes.isEmpty() )
      return 0;

   if ( const Result<> committed = commitMedia( writes ); !committed )
      return committed;

   return static_cast<int>( writes.size() );
}

Result<int> Gamelist::linkMedia( const QVector<MediaLink>& aLinks )
{
   const QDir systemDir( FSystemDir );

   // No file is touched, so the journal holds only the games as they were.
   QVector<MediaWrite> writes;
   for ( const MediaLink& link : aLinks ) {
      if ( link.index < 0 || link.index >= FEntries.size() )
         continue;

      Game& game = FEntries[link.index].game;

      MediaWrite write;
      write.index = link.index;
      write.isImage = link.isImage;
      write.before = game;
      writes.append( write );

      const QString text = QStringLiteral( "./" ) + systemDir.relativeFilePath( link.path );
      setMediaLink( link.index, link.isImage, text, link.path );
   }

   if ( writes.isEmpty() )
      return 0;

   if ( const Result<> committed = commitMedia( writes ); !committed )
      return committed;

   return static_cast<int>( writes.size() );
}

Result<int> Gamelist::renameMedia( const QVector<MediaRename>& aRenames )
{
   QVector<MediaWrite> writes;

   for ( const MediaRename& rename : aRenames ) {
      if ( rename.index < 0 || rename.index >= FEntries.size() || !rename.problem.isEmpty() )
         continue;

      if ( !QFile::rename( rename.path, rename.newPath ) ) {
         rollbackMedia( writes );
         return Error{
            QStringLiteral( "Cannot rename %1 to %2." ).arg( rename.path, rename.newPath ) };
      }

      Game& game = FEntries[rename.index].game;

      MediaWrite write;
      write.index = rename.index;
      write.isImage = rename.isImage;
      write.target = rename.newPath;
      write.movedFrom = rename.path;
      write.before = game;
      writes.append( write );

      // Only the file name changes: the folder part of the link stays as written.
      const QString oldLink = rename.isImage ? game.imagePath : game.videoPath;
      const QString text = folderOf( oldLink ) + QFileInfo( rename.newPath ).fileName();
      setMediaLink( rename.index, rename.isImage, text, rename.newPath );
   }

   if ( writes.isEmpty() )
      return 0;

   if ( const Result<> committed = commitMedia( writes ); !committed )
      return committed;

   return static_cast<int>( writes.size() );
}

Result<> Gamelist::setVideo( int aIndex, const QString& aSourcePath )
{
   if ( const Result<> valid = checkIndex( aIndex ); !valid )
      return valid;

   Game& game = FEntries[aIndex].game;
   const QString link = freeMediaLink( aIndex, false );
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
         return backup;
      write.backup = backup.value();
      write.target = target;
   }

   setMediaLink( aIndex, false, link, target );

   return commitMedia( { write } );
}

bool Gamelist::sharedFile( int aSkip, const QString& aPath, QString Game::*aMember ) const
{
   if ( aPath.isEmpty() )
      return false;

   for ( int index = 0; index < FEntries.size(); ++index ) {
      if ( index != aSkip &&
           sameFileOnDisk( FEntries.at( index ).game.*aMember, aPath, FFoldsCase ) )
         return true;
   }

   return false;
}

bool Gamelist::fileShared( int aIndex, const QString& aPhysicalPath, bool aIsImage ) const
{
   return sharedFile( aIndex, aPhysicalPath,
                      aIsImage ? &Game::physicalImagePath : &Game::physicalVideoPath );
}

Result<> Gamelist::removeMedia( int aIndex, bool aIsImage )
{
   if ( const Result<> valid = checkIndex( aIndex ); !valid )
      return valid;

   const Game before = FEntries.at( aIndex ).game;
   const QString file = aIsImage ? before.physicalImagePath : before.physicalVideoPath;

   setMediaLink( aIndex, aIsImage, QString(), QString() );

   if ( const Result<> saved = save(); !saved ) {
      restoreLink( aIndex, aIsImage, before );
      return saved;
   }

   // Only once the gamelist has stopped pointing at it, and only if no other
   // game points at it either - several games can share one file.
   if ( !file.isEmpty() && !fileShared( aIndex, file, aIsImage ) )
      discardFile( file );

   return {};
}

Result<> Gamelist::removeImage( int aIndex )
{
   return removeMedia( aIndex, true );
}

Result<> Gamelist::removeVideo( int aIndex )
{
   return removeMedia( aIndex, false );
}

Result<QStringList> Gamelist::removeGame( int aIndex, bool aDeleteFiles )
{
   if ( const Result<> valid = checkIndex( aIndex ); !valid )
      return valid;

   const Game game = FEntries.at( aIndex ).game;
   if ( const Result<> removed = removeGames( { aIndex } ); !removed )
      return removed;

   QStringList stuck;
   if ( !aDeleteFiles )
      return stuck;

   const auto drop = [&]( const QString& aPath, QString Game::*aMember ) {
      // Only once the gamelist has stopped listing it, and not what another entry
      // (a duplicate, a shared picture) still points at.
      if ( aPath.isEmpty() || !QFileInfo::exists( aPath ) || sharedFile( -1, aPath, aMember ) )
         return;
      if ( !discardFile( aPath ) )
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
