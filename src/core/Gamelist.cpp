#include "Gamelist.h"

#include "PathUtil.h"

#include <algorithm>
#include <utility>

#include <QDateTime>
#include <QDir>
#include <QDirIterator>

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>

namespace {

// ES-DE writes <alternativeEmulator> beside <gameList>, and XML allows one root: the file
// is parsed under this wrapper, which load() then takes out again.
constexpr const char* Cst_Wrapper = "gle-root";
constexpr const char* Cst_GameList = "gameList";

// The layout EmulationStation writes, for systems with no media yet.
constexpr const char* Cst_DefaultImageFolder = "./media/images/";
constexpr const char* Cst_DefaultVideoFolder = "./media/videos/";

// ES-DE keeps one folder per media type; covers and videos are the types this editor writes.
constexpr const char* Cst_EsdeImageType = "covers";
constexpr const char* Cst_EsdeVideoType = "videos";
const QStringList Cst_EsdeMediaTypes = {
   QStringLiteral( "3dboxes" ),   QStringLiteral( "backcovers" ),    QStringLiteral( "covers" ),
   QStringLiteral( "custom" ),    QStringLiteral( "fanart" ),        QStringLiteral( "manuals" ),
   QStringLiteral( "marquees" ),  QStringLiteral( "miximages" ),     QStringLiteral( "physicalmedia" ),
   QStringLiteral( "screenshots" ), QStringLiteral( "titlescreens" ), QStringLiteral( "videos" ) };

// Best first. ES-DE tries lowercase names only, so .jpeg and .PNG count as missing there.
const QStringList Cst_EsdeImageExtensions = { QStringLiteral( "jpg" ), QStringLiteral( "png" ),
                                              QStringLiteral( "webp" ) };
const QStringList Cst_EsdeVideoExtensions = {
   QStringLiteral( "mp4" ), QStringLiteral( "mkv" ), QStringLiteral( "avi" ), QStringLiteral( "wmv" ),
   QStringLiteral( "mov" ), QStringLiteral( "webm" ), QStringLiteral( "m4v" ) };

// The ROM's path without its extension, and a folder ROM (ScummVM) keeps its whole name.
QString mediaStem( const Game& aGame )
{
   const QFileInfo info( QDir::cleanPath( aGame.romPath ) );
   const QString base = QFileInfo( aGame.physicalRomPath ).isDir() ? info.fileName() : info.completeBaseName();
   return info.path() == QLatin1String( "." ) ? base : info.path() + QLatin1Char( '/' ) + base;
}

// Every type folder, so a deleted game leaves no screenshot or manual behind. Returns what stayed.
QStringList discardEsdeMedia( const QString& aMediaDir, const Game& aGame, bool aFoldsCase )
{
   QStringList stuck;
   const QFileInfo stem( mediaStem( aGame ) );
   const Qt::CaseSensitivity sensitivity = aFoldsCase ? Qt::CaseInsensitive : Qt::CaseSensitive;

   for ( const QString& type : Cst_EsdeMediaTypes ) {
      const QDir folder( resolvePath( QDir::cleanPath( aMediaDir + QLatin1Char( '/' ) + type + QLatin1Char( '/' ) + stem.path() ) ) );
      for ( const QFileInfo& file : folder.entryInfoList( QDir::Files ) ) {
         if ( file.completeBaseName().compare( stem.fileName(), sensitivity ) == 0 && !discardFile( file.filePath() ) )
            stuck << file.filePath();
      }
   }
   return stuck;
}

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

Result<> Gamelist::load( const SystemEntry& aSystem, bool aAllowEmpty )
{
   FSystem = aSystem;
   FBackedUp = false;
   FRomDir.clear();
   FMediaDir.clear();
   FDocument = QDomDocument();
   FGameList = QDomElement();
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
   if ( !FDocument.setContent( wrapInRoot( file.readAll(), Cst_Wrapper ), &parseError, &line, &column ) ) {
      return Error{ QStringLiteral( "%1 is not valid XML: %2 (line %3, column %4)" )
                       .arg( aSystem.gamelistPath, parseError )
                       .arg( line )
                       .arg( column ) };
   }

   FGameList = FDocument.documentElement().firstChildElement( QLatin1String( Cst_GameList ) );
   if ( FGameList.isNull() )
      return Error{ QStringLiteral( "%1 has no <gameList>." ).arg( aSystem.gamelistPath ) };

   // Back to the file's own shape: the wrapper's children become the document's.
   QDomElement wrapper = FDocument.documentElement();
   while ( !wrapper.firstChild().isNull() )
      FDocument.appendChild( wrapper.firstChild() );
   FDocument.removeChild( wrapper );

   // save() writes UTF-8, whatever encoding the file declared.
   if ( QDomNode declaration = FDocument.firstChild(); declaration.isProcessingInstruction() )
      declaration.toProcessingInstruction().setData( QStringLiteral( "version=\"1.0\"" ) );

   FRomDir = aSystem.romDir.isEmpty() ? QFileInfo( aSystem.gamelistPath ).absolutePath()
                                      : QDir( aSystem.romDir ).absolutePath();
   FFoldsCase = volumeFoldsCase( aSystem.gamelistPath );
   if ( !aSystem.mediaDir.isEmpty() )
      FMediaDir = QDir( aSystem.mediaDir ).absolutePath();
   indexMedia();
   bool imageFolderFound = false;
   bool videoFolderFound = false;

   for ( QDomElement node = FGameList.firstChildElement( QLatin1String( Cst_Game ) );
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

   if ( FEntries.isEmpty() && !aAllowEmpty )
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

   game.physicalRomPath = physicalPath( FRomDir, game.romPath );
   game.isOrphan = fileMissing( game.physicalRomPath );
   if ( conventionalMedia() ) {
      game.imagePath.clear();
      game.videoPath.clear();
      findConventionalMedia( game );
   } else {
      game.physicalImagePath = physicalPath( FRomDir, game.imagePath );
      game.physicalVideoPath = physicalPath( FRomDir, game.videoPath );
      game.missingImage = fileMissing( game.physicalImagePath );
      game.missingVideo = fileMissing( game.physicalVideoPath );
   }
   return game;
}

QString Gamelist::conventionalPath( const Game& aGame, bool aIsImage ) const
{
   return FMediaDir + QLatin1Char( '/' ) + QLatin1String( aIsImage ? Cst_EsdeImageType : Cst_EsdeVideoType ) +
          QLatin1Char( '/' ) + mediaStem( aGame ) + QLatin1String( aIsImage ? Cst_ImageSuffixPng : Cst_VideoSuffixMp4 );
}

void Gamelist::findConventionalMedia( Game& aGame ) const
{
   const QString key = fileKey( mediaStem( aGame ), FFoldsCase );
   const QString image = FImageIndex.value( key );
   const QString video = FVideoIndex.value( key );

   aGame.missingImage = image.isEmpty();
   aGame.missingVideo = video.isEmpty();
   aGame.physicalImagePath = aGame.missingImage ? conventionalPath( aGame, true ) : image;
   aGame.physicalVideoPath = aGame.missingVideo ? conventionalPath( aGame, false ) : video;
}

// One walk per type folder; a directory lookup per game would list folders for every miss.
void Gamelist::indexMedia()
{
   FImageIndex.clear();
   FVideoIndex.clear();
   if ( !conventionalMedia() )
      return;

   const auto index = [this]( const char* aType, const QStringList& aExtensions, QHash<QString, QString>& aIndex ) {
      const QDir folder( resolvePath( FMediaDir + QLatin1Char( '/' ) + QLatin1String( aType ) ) );
      const auto rank = [&]( const QString& aPath ) {
         const QString extension = QFileInfo( aPath ).suffix();
         return aExtensions.indexOf( FFoldsCase ? extension.toLower() : extension );
      };

      QDirIterator it( folder.path(), QDir::Files, QDirIterator::Subdirectories );
      while ( it.hasNext() ) {
         const QString path = it.next();
         if ( rank( path ) < 0 )
            continue;

         const QFileInfo info( path );
         const QString stem = folder.relativeFilePath( info.path() + QLatin1Char( '/' ) + info.completeBaseName() );
         const QString key = fileKey( stem, FFoldsCase );
         const auto found = aIndex.constFind( key );
         if ( found == aIndex.constEnd() || rank( path ) < rank( found.value() ) )
            aIndex.insert( key, path );
      }
   };

   index( Cst_EsdeImageType, Cst_EsdeImageExtensions, FImageIndex );
   index( Cst_EsdeVideoType, Cst_EsdeVideoExtensions, FVideoIndex );
}

Result<int> Gamelist::addGames( const QStringList& aRomPaths )
{
   const QDir systemDir( FRomDir );
   indexMedia();
   QSet<QString> seen;
   for ( const Entry& entry : FEntries ) {
      if ( !entry.game.physicalRomPath.isEmpty() )
         seen.insert( fileKey( entry.game.physicalRomPath, FFoldsCase ) );
   }

   const int firstNew = static_cast<int>( FEntries.size() );
   QVector<QDomElement> added;

   const auto rollback = [&] {
      for ( QDomElement node : std::as_const( added ) )
         FGameList.removeChild( node );
      FEntries.resize( firstNew );
   };

   for ( const QString& path : aRomPaths ) {
      const QFileInfo info( path );
      const QString relative = systemDir.relativeFilePath( info.absoluteFilePath() );

      if ( !info.isFile() || relative.startsWith( QLatin1String( ".." ) ) ) {
         rollback();
         return Error{ QStringLiteral( "%1 is not a file inside %2." ).arg( path, FRomDir ) };
      }

      // Already listed, or picked twice: nothing to add, and not an error.
      const QString key = fileKey( info.absoluteFilePath(), FFoldsCase );
      if ( seen.contains( key ) )
         continue;
      seen.insert( key );

      QDomElement node = FDocument.createElement( QLatin1String( Cst_Game ) );
      FGameList.appendChild( node );
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

Result<> Gamelist::setFields( const QVector<int>& aTargets, const GameFields& aValues,
                              GameFieldSet aWhich )
{
   if ( !aWhich )
      return {};

   return applyAndSave( aTargets, [&] { applyFields( aTargets, aValues, aWhich ); } );
}

Result<> Gamelist::editNames( const QVector<int>& aTargets, const NameEdit& aEdit )
{
   QVector<int> changed;
   for ( int index : aTargets )
      if ( index >= 0 && index < FEntries.size() &&
           applyNameEdit( FEntries.at( index ).game.name, aEdit ) != FEntries.at( index ).game.name )
         changed.append( index );

   return applyAndSave( changed, [&] {
      for ( int index : std::as_const( changed ) ) {
         const QString name = applyNameEdit( FEntries.at( index ).game.name, aEdit );
         setChildText( FEntries[index].node, Cst_Name, name );
         FEntries[index].game.name = name;
      }
      countDuplicates();
   } );
}

Result<> Gamelist::applyAndSave( const QVector<int>& aTargets, const std::function<void()>& aApply )
{
   // Whole <game> copies, so the undo also drops the elements aApply created.
   QVector<std::pair<int, Entry>> before;
   for ( int index : aTargets ) {
      if ( index >= 0 && index < FEntries.size() )
         before.append( { index, { FEntries.at( index ).game,
                                   FEntries.at( index ).node.cloneNode().toElement() } } );
   }
   if ( before.isEmpty() )
      return {};

   aApply();

   if ( const Result<> saved = save(); !saved ) {
      for ( const auto& [index, entry] : std::as_const( before ) ) {
         FEntries[index].node.parentNode().replaceChild( entry.node, FEntries[index].node );
         FEntries[index] = entry;
      }
      countDuplicates();
      return saved;
   }

   return {};
}

void Gamelist::applyFields( const QVector<int>& aTargets, const GameFields& aValues,
                            GameFieldSet aWhich )
{

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

   // ES-DE has one file per stem, so a taken name is not a reason to pick another.
   if ( conventionalMedia() )
      return conventionalPath( game, aIsImage );

   for ( int attempt = 0;; ++attempt ) {
      const QString stem = attempt == 0   ? game.romNameWoExt()
                           : attempt == 1 ? game.romName()
                                          : game.romNameWoExt() + QLatin1Char( '_' ) +
                                               QString::number( attempt );
      const QString link = folder + stem + suffix;
      const QString target = writeTarget( FRomDir, link );

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
   if ( conventionalMedia() ) {
      // ponytail: a game sharing this media stem keeps its old paths until the next load.
      ( aIsImage ? game.physicalImagePath : game.physicalVideoPath ) =
         aPhysical.isEmpty() ? conventionalPath( game, aIsImage ) : aPhysical;
      ( aIsImage ? game.missingImage : game.missingVideo ) = aPhysical.isEmpty();
      return;
   }

   setChildText( entry.node, aIsImage ? Cst_ImageLink : Cst_VideoLink, aLink );
   ( aIsImage ? game.imagePath : game.videoPath ) = aLink;
   ( aIsImage ? game.physicalImagePath : game.physicalVideoPath ) = aPhysical;
   ( aIsImage ? game.missingImage : game.missingVideo ) = aPhysical.isEmpty();
}

Result<Gamelist::MediaWrite> Gamelist::applyImage( int aIndex, const QImage& aPicture )
{
   Game& game = FEntries[aIndex].game;
   const QString link = freeMediaLink( aIndex, true );
   const QString target = writeTarget( FRomDir, link );

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
   if ( conventionalMedia() && !game.missingImage && !sameFile( game.physicalImagePath, target, FFoldsCase ) )
      write.stale = game.physicalImagePath;
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
   if ( !conventionalMedia() )
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

Result<> Gamelist::saveLinks()
{
   return conventionalMedia() ? Result<>{} : save();
}

Result<> Gamelist::commitMedia( const QVector<MediaWrite>& aWrites )
{
   if ( const Result<> saved = saveLinks(); !saved ) {
      rollbackMedia( aWrites );
      return saved;
   }

   for ( const MediaWrite& write : aWrites ) {
      if ( !write.backup.isEmpty() )
         QFile::remove( write.backup );
      if ( !write.stale.isEmpty() )
         QFile::remove( write.stale );
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
   if ( conventionalMedia() )
      return Error{ QStringLiteral( "ES-DE finds media by the ROM's name; there is nothing to link." ) };

   const QDir systemDir( FRomDir );

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
   if ( conventionalMedia() )
      return Error{ QStringLiteral( "ES-DE finds media by the ROM's name; renaming it would lose it." ) };

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
   const QString target = writeTarget( FRomDir, link );

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
   if ( conventionalMedia() && !game.missingVideo && !sameFile( game.physicalVideoPath, target, FFoldsCase ) )
      write.stale = game.physicalVideoPath;

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

   if ( const Result<> saved = saveLinks(); !saved ) {
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

   if ( conventionalMedia() ) {
      // Games with one media stem (Sonic.zip, Sonic.7z) share it, so it stays while one is left.
      const QString key = fileKey( mediaStem( game ), FFoldsCase );
      const bool shared = std::any_of( FEntries.cbegin(), FEntries.cend(), [&]( const Entry& aEntry ) {
         return fileKey( mediaStem( aEntry.game ), FFoldsCase ) == key;
      } );
      if ( !shared )
         stuck << discardEsdeMedia( FMediaDir, game, FFoldsCase );
      if ( !game.physicalRomPath.isEmpty() && QFileInfo::exists( game.physicalRomPath ) &&
           !sharedFile( -1, game.physicalRomPath, &Game::physicalRomPath ) && !discardFile( game.physicalRomPath ) )
         stuck << game.physicalRomPath;
      return stuck;
   }

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
