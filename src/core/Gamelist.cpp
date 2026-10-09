#include "Gamelist.h"

#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>

namespace {

// Where new media lands when the system has none yet to copy the layout from.
// The Delphi left both empty in that case, which dropped scraped art straight
// into the ROM folder; this is the layout EmulationStation itself writes.
constexpr const char* Cst_DefaultImageFolder = "./media/images/";
constexpr const char* Cst_DefaultVideoFolder = "./media/videos/";

QString folderOf( const QString& aLink )
{
   return aLink.left( aLink.lastIndexOf( QLatin1Char( '/' ) ) + 1 );
}

// The Delphi's GetNodeValue: a missing node reads as empty rather
// than raising, which is what made the unguarded FindNode(...).Text calls
// stand out as bugs.
QString childText( const QDomElement& aNode, const char* aName )
{
   return aNode.firstChildElement( QLatin1String( aName ) ).text();
}

// GetPhysicalRomPath / GetPhysicalMediaPath, minus the six
// hardcoded backslashes: gamelist paths are relative to the system folder and
// start with "./", which cleanPath folds away.
QString physicalPath( const QString& aSystemDir, const QString& aRelative )
{
   if ( aRelative.isEmpty() )
      return {};

   return resolvePath( QDir::cleanPath( QDir( aSystemDir ).filePath( aRelative ) ) );
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

// CheckIfFileMissing. Snapshot at load: the proxy must not stat a
// file per row on every keystroke.
bool fileMissing( const QString& aPath )
{
   return aPath.isEmpty() || !QFileInfo::exists( aPath );
}

bool fail( QString* aError, const QString& aMessage )
{
   if ( aError )
      *aError = aMessage;
   return false;
}

// AdjustDecimalSeparator + StrToFloatDef (F_Main 1104, 1283). Ratings are
// written with either separator; an unparseable one counts as 0, as it did.
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

QVector<SystemEntry> scanSystems( const QString& aRootPath )
{
   QVector<SystemEntry> systems;

   // Dirs by attribute, not by a '*.*' mask
   // Hidden folders stay out, which is what Info.Name[1] <> '.' was for.
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

   // re-lists the containing directory on every component that misses,
   // so a ROM folder that is absent altogether costs one listing per game. That
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

// I do not use Qt's date parsing because this would be a behavior change
// (previously-accepted garbage dates would now render as empty
QString gamelistDateToDisplay( const QString& aValue )
{
   if ( aValue.size() < 8 )
      return {};

   bool isNumber = false;
   const int year = aValue.left( 4 ).toInt( &isNumber );
   if ( !isNumber )
      return {};
   const int month = aValue.mid( 4, 2 ).toInt( &isNumber );
   if ( !isNumber )
      return {};
   const int day = aValue.mid( 6, 2 ).toInt( &isNumber );
   if ( !isNumber )
      return {};

   QString display;
   if ( day > 0 )
      display += aValue.mid( 6, 2 ) + QLatin1Char( '/' );
   if ( month > 0 )
      display += aValue.mid( 4, 2 ) + QLatin1Char( '/' );
   if ( year > 0 )
      display += aValue.left( 4 );

   return display;
}

QString displayDateToGamelist( const QString& aDate )
{
   if ( aDate.isEmpty() || !aDate.at( 0 ).isDigit() )
      return {};

   switch ( aDate.size() ) {
      case 4:  // yyyy
         return aDate + QLatin1String( Cst_DateLongFill ) + QLatin1String( Cst_DateSuffix );
      case 7:  // MM/yyyy
         return aDate.mid( 3, 4 ) + aDate.left( 2 ) + QLatin1String( Cst_DateShortFill ) +
                QLatin1String( Cst_DateSuffix );
      case 10:  // dd/MM/yyyy
         return aDate.mid( 6, 4 ) + aDate.mid( 3, 2 ) + aDate.left( 2 ) +
                QLatin1String( Cst_DateSuffix );
      default: return {};
   }
}

bool Gamelist::load( const SystemEntry& aSystem, QString* aError )
{
   FSystem = aSystem;
   FSystemDir.clear();
   FDocument = QDomDocument();
   FGames.clear();
   FNodes.clear();
   FImageFolder = QLatin1String( Cst_DefaultImageFolder );
   FVideoFolder = QLatin1String( Cst_DefaultVideoFolder );
   FNameCounts.clear();
   FRomCounts.clear();

   QFile file( aSystem.gamelistPath );
   if ( !file.open( QIODevice::ReadOnly ) )
      return fail( aError, QStringLiteral( "Cannot read %1: %2" )
                              .arg( aSystem.gamelistPath, file.errorString() ) );

   QString parseError;
   int line = 0;
   int column = 0;
   if ( !FDocument.setContent( &file, &parseError, &line, &column ) ) {
      return fail( aError, QStringLiteral( "%1 is not valid XML: %2 (line %3, column %4)" )
                              .arg( aSystem.gamelistPath, parseError )
                              .arg( line )
                              .arg( column ) );
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

      FGames.append( game );
      FNodes.append( node );
   }

   if ( FGames.isEmpty() )
      return fail( aError, QStringLiteral( "%1 holds no game." ).arg( aSystem.gamelistPath ) );

   countDuplicates();

   return true;
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
   for ( const Game& game : FGames )
      listed.insert( key( game.romPath ) );

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

int Gamelist::addGames( const QStringList& aRomPaths, QString* aError )
{
   const QDir systemDir( FSystemDir );
   QSet<QString> seen;
   for ( const Game& game : FGames )
      seen.insert( QDir::cleanPath( systemDir.filePath( game.romPath ) ).toLower() );

   const int firstNew = static_cast<int>( FGames.size() );
   QVector<QDomElement> added;

   const auto rollback = [&] {
      for ( QDomElement node : std::as_const( added ) )
         FDocument.documentElement().removeChild( node );
      FGames.resize( firstNew );
      FNodes.resize( firstNew );
   };

   for ( const QString& path : aRomPaths ) {
      const QFileInfo info( path );
      const QString relative = systemDir.relativeFilePath( info.absoluteFilePath() );

      if ( !info.isFile() || relative.startsWith( QLatin1String( ".." ) ) ) {
         rollback();
         fail( aError, QStringLiteral( "%1 is not a file inside %2." ).arg( path, FSystemDir ) );
         return -1;
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
      FGames.append( gameFromNode( node ) );
      FNodes.append( node );
   }

   if ( added.isEmpty() )
      return 0;

   if ( !save( aError ) ) {
      rollback();
      return -1;
   }

   countDuplicates();
   return static_cast<int>( added.size() );
}

void Gamelist::countDuplicates()
{
   FNameCounts.clear();
   FRomCounts.clear();

   for ( const Game& game : FGames ) {
      ++FNameCounts[game.name];
      ++FRomCounts[game.romName()];
   }
}

bool Gamelist::save( QString* aError )
{
   QSaveFile file( FSystem.gamelistPath );
   if ( !file.open( QIODevice::WriteOnly ) )
      return fail( aError, QStringLiteral( "Cannot write %1: %2" )
                              .arg( FSystem.gamelistPath, file.errorString() ) );

   // The whole document is re-indented, where the Delphi reformatted only when
   // it had added a node. Three spaces is what EmulationStation writes.
   const QByteArray content = FDocument.toByteArray( 3 );
   if ( file.write( content ) != content.size() ) {
      file.cancelWriting();
      return fail( aError, QStringLiteral( "Cannot write %1: %2" )
                              .arg( FSystem.gamelistPath, file.errorString() ) );
   }

   if ( !file.commit() )
      return fail( aError, QStringLiteral( "Cannot write %1: %2" )
                              .arg( FSystem.gamelistPath, file.errorString() ) );

   return true;
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
      if ( index < 0 || index >= FGames.size() )
         continue;

      Game& game = FGames[index];
      QDomElement node = FNodes[index];

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
         // back blank, which is what the hint under Edt_ReleaseDate promises.
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

bool Gamelist::applyImage( int aIndex, const QImage& aPicture, QString* aError )
{
   Game& game = FGames[aIndex];
   const QString link = FImageFolder + game.romNameWoExt() + QLatin1String( Cst_ImageSuffixPng );
   const QString target = writeTarget( FSystemDir, link );

   if ( !QDir().mkpath( QFileInfo( target ).path() ) )
      return fail( aError,
                   QStringLiteral( "Cannot create %1." ).arg( QFileInfo( target ).path() ) );

   // The suffix picks the format, so this re-encodes to PNG the way
   // Picture.SaveToFile did
   if ( !aPicture.save( target ) )
      return fail( aError, QStringLiteral( "Cannot write %1." ).arg( target ) );

   setChildText( FNodes[aIndex], Cst_ImageLink, link );
   game.imagePath = link;
   game.physicalImagePath = target;
   game.missingImage = false;

   return true;
}

bool Gamelist::setImage( int aIndex, const QString& aSourcePath, QString* aError )
{
   if ( aIndex < 0 || aIndex >= FGames.size() )
      return fail( aError, QStringLiteral( "No game selected." ) );

   QImage picture;
   if ( !picture.load( aSourcePath ) ) {
      return fail(
         aError, QStringLiteral( "%1 is not a picture this build can read." ).arg( aSourcePath ) );
   }

   return applyImage( aIndex, picture, aError ) && save( aError );
}

bool Gamelist::setImage( int aIndex, const QImage& aPicture, QString* aError )
{
   if ( aIndex < 0 || aIndex >= FGames.size() )
      return fail( aError, QStringLiteral( "No game selected." ) );

   return applyImage( aIndex, aPicture, aError ) && save( aError );
}

int Gamelist::setDefaultImageForMissing( const QString& aSourcePath, QString* aError )
{
   QImage picture;
   if ( !picture.load( aSourcePath ) ) {
      fail( aError,
            QStringLiteral( "%1 is not a picture this build can read." ).arg( aSourcePath ) );
      return 0;
   }

   int changed = 0;
   for ( int index = 0; index < FGames.size(); ++index ) {
      // The missing-image test the caption always claimed and the code never had.
      if ( !FGames.at( index ).missingImage )
         continue;
      if ( !applyImage( index, picture, aError ) )
         break;
      ++changed;
   }

   if ( changed > 0 && !save( aError ) )
      return 0;

   return changed;
}

bool Gamelist::setVideo( int aIndex, const QString& aSourcePath, QString* aError )
{
   if ( aIndex < 0 || aIndex >= FGames.size() )
      return fail( aError, QStringLiteral( "No game selected." ) );

   Game& game = FGames[aIndex];
   const QString link = FVideoFolder + game.romNameWoExt() + QLatin1String( Cst_VideoSuffixMp4 );
   const QString target = writeTarget( FSystemDir, link );

   if ( !QDir().mkpath( QFileInfo( target ).path() ) )
      return fail( aError,
                   QStringLiteral( "Cannot create %1." ).arg( QFileInfo( target ).path() ) );

   // Picking the game's own video would otherwise be remove-then-copy, i.e. a
   // deletion. TFile.Copy raised instead; either way, do not lose the file.
   const QString source = QFileInfo( aSourcePath ).canonicalFilePath();
   if ( source.isEmpty() )
      return fail( aError, QStringLiteral( "%1 cannot be read." ).arg( aSourcePath ) );

   if ( source != QFileInfo( target ).canonicalFilePath() ) {
      QFile::remove( target );
      if ( !QFile::copy( source, target ) )
         return fail( aError, QStringLiteral( "Cannot copy %1 to %2." ).arg( source, target ) );
   }

   setChildText( FNodes[aIndex], Cst_VideoLink, link );
   game.videoPath = link;
   game.physicalVideoPath = target;
   game.missingVideo = false;

   return save( aError );
}

bool Gamelist::fileShared( int aIndex, const QString& aPhysicalPath, bool aIsImage ) const
{
   if ( aPhysicalPath.isEmpty() )
      return false;

   for ( int index = 0; index < FGames.size(); ++index ) {
      if ( index == aIndex )
         continue;
      const Game& other = FGames.at( index );
      if ( ( aIsImage ? other.physicalImagePath : other.physicalVideoPath ) == aPhysicalPath )
         return true;
   }

   return false;
}

bool Gamelist::removeImage( int aIndex, QString* aError )
{
   if ( aIndex < 0 || aIndex >= FGames.size() )
      return fail( aError, QStringLiteral( "No game selected." ) );

   Game& game = FGames[aIndex];
   const QString file = game.physicalImagePath;

   setChildText( FNodes[aIndex], Cst_ImageLink, QString() );
   game.imagePath.clear();
   game.physicalImagePath.clear();
   game.missingImage = true;

   if ( !save( aError ) )
      return false;

   // Only once the gamelist has stopped pointing at it, and only if no other
   // game points at it either - several games can share one picture.
   if ( !file.isEmpty() && !fileShared( aIndex, file, true ) )
      QFile::remove( file );

   return true;
}

bool Gamelist::removeVideo( int aIndex, QString* aError )
{
   if ( aIndex < 0 || aIndex >= FGames.size() )
      return fail( aError, QStringLiteral( "No game selected." ) );

   Game& game = FGames[aIndex];
   const QString file = game.physicalVideoPath;

   setChildText( FNodes[aIndex], Cst_VideoLink, QString() );
   game.videoPath.clear();
   game.physicalVideoPath.clear();
   game.missingVideo = true;

   if ( !save( aError ) )
      return false;

   if ( !file.isEmpty() && !fileShared( aIndex, file, false ) )
      QFile::remove( file );

   return true;
}

bool Gamelist::removeGame( int aIndex, QString* aError )
{
   if ( aIndex < 0 || aIndex >= FGames.size() )
      return fail( aError, QStringLiteral( "No game selected." ) );

   QDomElement node = FNodes.at( aIndex );
   QDomNode parent = node.parentNode();
   const QDomNode next = node.nextSibling();
   parent.removeChild( node );

   if ( !save( aError ) ) {
      parent.insertBefore( node, next );
      return false;
   }

   const Game game = FGames.at( aIndex );
   FGames.remove( aIndex );
   FNodes.remove( aIndex );
   countDuplicates();

   // Only once the gamelist has stopped listing it, and not what another entry
   // (a duplicate, a shared picture) still points at.
   const auto shared = [this]( const QString& aPath, QString Game::*aMember ) {
      for ( const Game& other : std::as_const( FGames ) ) {
         if ( other.*aMember == aPath )
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

   if ( !stuck.isEmpty() && aError )
      *aError = QStringLiteral( "Removed from the gamelist, but could not delete:\n%1" )
                   .arg( stuck.join( QLatin1Char( '\n' ) ) );

   return true;
}

void Gamelist::ensureHashes( int aIndex )
{
   if ( aIndex < 0 || aIndex >= FGames.size() )
      return;

   Game& game = FGames[aIndex];

   // An orphan hashes to nothing rather than raising
   if ( game.md5.isEmpty() )
      game.md5 = fileHash( game.physicalRomPath, QCryptographicHash::Md5 );
   if ( game.sha1.isEmpty() )
      game.sha1 = fileHash( game.physicalRomPath, QCryptographicHash::Sha1 );
   if ( game.crc32.isEmpty() )
      game.crc32 = fileCrc32( game.physicalRomPath );
}

bool Gamelist::accepts( int aIndex, const FilterSpec& aFilter ) const
{
   if ( aIndex < 0 || aIndex >= FGames.size() )
      return false;

   if ( !matchesCategory( FGames.at( aIndex ), aFilter ) )
      return false;

   // ContainsText was case-insensitive, and matched whatever the list displays.
   return aFilter.search.isEmpty() ||
          displayName( aIndex, aFilter ).contains( aFilter.search, Qt::CaseInsensitive );
}

QString Gamelist::displayName( int aIndex, const FilterSpec& aFilter ) const
{
   const Game& game = FGames.at( aIndex );

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
   switch ( aFilter.index ) {
      case 0: return true;
      case 1: return aGame.missingImage;
      case 2: return aGame.missingVideo;
      case 3: return aGame.releaseDate.isEmpty();
      case 4: return aGame.players.isEmpty();
      case 5: return aGame.rating.isEmpty();
      case 6: return aGame.developer.isEmpty();
      case 7: return aGame.publisher.isEmpty();
      case 8: return aGame.description.isEmpty();
      case 9: return aGame.genre.isEmpty();
      case 10: return aGame.region.isEmpty();
      case 11: return aGame.kidGame == 1;
      case 12: return aGame.hidden == 1;
      case 13: return aGame.favorite == 1;
      case 14: return aGame.isOrphan;
      case 29: return FNameCounts.value( aGame.name ) > 1;
      case 30: return FRomCounts.value( aGame.romName() ) > 1;
      default: break;
   }

   if ( aFilter.reference < 0 || aFilter.reference >= FGames.size() )
      return false;

   const Game& reference = FGames.at( aFilter.reference );

   switch ( aFilter.index ) {
      case 15: return aGame.region == reference.region;
      case 16: return aGame.releaseDate == reference.releaseDate;
      case 17:
      case 18: {
         if ( aGame.releaseDate.isEmpty() )
            return false;
         // An unset reference date meant "today" in the Delphi, via StrToDateDef.
         QDate referenceDate = releaseDateValue( reference.releaseDate );
         if ( !referenceDate.isValid() )
            referenceDate = QDate::currentDate();
         const QDate date = releaseDateValue( aGame.releaseDate );
         return aFilter.index == 17 ? date <= referenceDate : date >= referenceDate;
      }
      case 19: return aGame.players == reference.players;
      case 20: return aGame.rating == reference.rating;
      case 21:
         return !aGame.rating.isEmpty() &&
                ratingValue( aGame.rating ) <= ratingValue( reference.rating );
      case 22:
         return !aGame.rating.isEmpty() &&
                ratingValue( aGame.rating ) >= ratingValue( reference.rating );
      case 23: return aGame.publisher == reference.publisher;
      case 24: return aGame.developer == reference.developer;
      case 25: return aGame.genre == reference.genre;
      // Trap: AnsiSameText here was a case-insensitive compare of two paths,
      // which on Linux would call two distinct directories the same one. The
      // case is already settled against the disk by resolvePath, so compare it.
      case 26:
         return QFileInfo( aGame.physicalRomPath ).path() ==
                QFileInfo( reference.physicalRomPath ).path();
      case 27: return aGame.name == reference.name;
      case 28: return aGame.romName() == reference.romName();
      default: return false;
   }
}
