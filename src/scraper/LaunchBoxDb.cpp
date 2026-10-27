#include "LaunchBoxDb.h"

#include "Resources.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <algorithm>
#include <initializer_list>

namespace {

const QLatin1String Cst_ImageBase( "https://images.launchbox-app.com/" );

QString stripTags( const QString& aName )
{
   static const QRegularExpression tags( QStringLiteral( R"(\([^)]*\)|\[[^\]]*\]|\{[^}]*\})" ) );
   QString name = aName;
   name.remove( tags );
   return name.trimmed();
}

// "Legend of Zelda, The - A Link" to "The Legend of Zelda - A Link"; empty when there is no
// trailing article.
QString articleFront( const QString& aName )
{
   static const QRegularExpression article( QStringLiteral( "^(.+?), (The|A|An)(?= - |: |$)" ) );
   const QString name = stripTags( aName );
   const QRegularExpressionMatch match = article.match( name );
   if ( !match.hasMatch() )
      return {};
   return match.captured( 2 ) + QLatin1Char( ' ' ) + match.captured( 1 ) +
          name.mid( static_cast<int>( match.capturedEnd( 0 ) ) );
}

// Game::region and the ROM's (USA, Europe) tags, as LaunchBox region names, best first.
// ponytail: Game::region is written in the UI language, so only English names match here.
QStringList regionPreference( const QString& aGameRegion, const QString& aRomName )
{
   QStringList tokens = aGameRegion.split( QStringLiteral( " - " ), Qt::SkipEmptyParts );
   static const QRegularExpression tag( QStringLiteral( R"(\(([^)]*)\))" ) );
   for ( auto it = tag.globalMatch( aRomName ); it.hasNext(); )
      tokens += it.next().captured( 1 ).split( QLatin1Char( ',' ) );

   QStringList regions;
   for ( const QString& raw : std::as_const( tokens ) ) {
      const QString token = raw.trimmed();
      if ( token.isEmpty() )
         continue;
      if ( token.compare( QLatin1String( "USA" ), Qt::CaseInsensitive ) == 0 ||
           token.compare( QLatin1String( "US" ), Qt::CaseInsensitive ) == 0 )
         regions << QStringLiteral( "North America" ) << QStringLiteral( "United States" );
      else if ( token.compare( QLatin1String( "UK" ), Qt::CaseInsensitive ) == 0 )
         regions << QStringLiteral( "United Kingdom" );
      else
         regions << token;
   }
   return regions;
}

bool sameText( const QString& aLeft, const QString& aRight )
{
   return aLeft.compare( aRight, Qt::CaseInsensitive ) == 0;
}

using Fields = QHash<QString, QString>;

// Leaves aXml at the end of the current record. Only the named children are read.
Fields readFields( QXmlStreamReader& aXml, std::initializer_list<const char*> aWanted )
{
   Fields fields;
   while ( aXml.readNextStartElement() ) {
      bool wanted = false;
      for ( const char* name : aWanted ) {
         if ( aXml.name() == QLatin1String( name ) ) {
            wanted = true;
            break;
         }
      }
      if ( wanted )
         fields.insert( aXml.name().toString(),
                        aXml.readElementText( QXmlStreamReader::SkipChildElements ) );
      else
         aXml.skipCurrentElement();
   }
   return fields;
}

// aHandler( xml ) is called at each record under the root and returns whether it read the
// record with readFields; an unread record is skipped.
template <typename Handler>
Result<> forEachRecord( QIODevice& aDevice, Handler aHandler )
{
   QXmlStreamReader xml( &aDevice );
   if ( !xml.readNextStartElement() )
      return Error{ QStringLiteral( "Not a LaunchBox XML file: %1" ).arg( xml.errorString() ) };
   while ( xml.readNextStartElement() ) {
      if ( !aHandler( xml ) )
         xml.skipCurrentElement();
   }
   if ( xml.hasError() )
      return Error{ QStringLiteral( "XML error in line %1: %2" )
                       .arg( xml.lineNumber() )
                       .arg( xml.errorString() ) };
   return {};
}

Result<> checkSeekable( const QIODevice& aDevice )
{
   if ( !aDevice.isReadable() || aDevice.isSequential() )
      return Error{ QStringLiteral( "The LaunchBox file must be an open, seekable device." ) };
   return {};
}

struct MediaSlot
{
   const char* media;
   QStringList types;  // LaunchBox Type values, best first
};

const QVector<MediaSlot>& mediaSlots()
{
   static const QVector<MediaSlot> table = {
      { Cst_MediaBox2d,
        { QStringLiteral( "Box - Front" ), QStringLiteral( "Box - Front - Reconstructed" ),
          QStringLiteral( "Fanart - Box - Front" ) } },
      { Cst_MediaScreenShot, { QStringLiteral( "Screenshot - Gameplay" ) } },
      { Cst_MediaSsTitle, { QStringLiteral( "Screenshot - Game Title" ) } },
      { Cst_MediaBox3d, { QStringLiteral( "Box - 3D" ) } },
      { Cst_MediaWheel, { QStringLiteral( "Clear Logo" ) } },
   };
   return table;
}

}  // namespace

QString launchBoxNameKey( const QString& aName )
{
   const QString name = stripTags( aName ).normalized( QString::NormalizationForm_D );
   QString key;
   const auto separator = [&key] {
      if ( !key.isEmpty() && !key.endsWith( QLatin1Char( '_' ) ) )
         key += QLatin1Char( '_' );
   };
   for ( const QChar ch : name ) {
      if ( ch.category() == QChar::Mark_NonSpacing )
         continue;
      const QChar up = ch.toUpper();
      if ( up == QLatin1Char( '&' ) ) {
         separator();
         key += QLatin1String( "AND" );
         separator();
      } else if ( up.isSpace() || up == QLatin1Char( ':' ) || up == QLatin1Char( '-' ) ) {
         separator();
      } else if ( QStringView( u".!?'\u2019" ).contains( up ) ) {
         continue;
      } else {
         key += up;
      }
   }
   if ( key.endsWith( QLatin1Char( '_' ) ) )
      key.chop( 1 );
   return key;
}

Result<> LaunchBoxDb::loadMetadata( QIODevice& aDevice, const QString& aPlatform )
{
   FGames.clear();
   FByKey.clear();
   FByAltKey.clear();
   if ( const Result<> ok = checkSeekable( aDevice ); !ok )
      return ok;

   const auto fail = [this]( const Result<>& aError ) {
      FGames.clear();
      FByKey.clear();
      FByAltKey.clear();
      return aError;
   };

   QHash<QString, int> byId;
   const Result<> first = forEachRecord( aDevice, [&]( QXmlStreamReader& aXml ) {
      if ( aXml.name() != QLatin1String( "Game" ) )
         return false;
      const Fields f = readFields( aXml, { "DatabaseID", "Name", "Platform", "ReleaseDate",
                                           "ReleaseYear", "Overview", "MaxPlayers", "Genres",
                                           "Developer", "Publisher", "CommunityRating" } );
      if ( f.value( QStringLiteral( "Platform" ) ) != aPlatform )
         return true;
      Entry game;
      game.name = f.value( QStringLiteral( "Name" ) );
      game.releaseDate = f.value( QStringLiteral( "ReleaseDate" ) );
      game.releaseYear = f.value( QStringLiteral( "ReleaseYear" ) );
      game.overview = f.value( QStringLiteral( "Overview" ) );
      game.maxPlayers = f.value( QStringLiteral( "MaxPlayers" ) );
      game.genres = f.value( QStringLiteral( "Genres" ) );
      game.developer = f.value( QStringLiteral( "Developer" ) );
      game.publisher = f.value( QStringLiteral( "Publisher" ) );
      game.rating = f.value( QStringLiteral( "CommunityRating" ) );

      const int index = static_cast<int>( FGames.size() );
      byId.insert( f.value( QStringLiteral( "DatabaseID" ) ), index );
      const QString key = launchBoxNameKey( game.name );
      if ( !key.isEmpty() )
         FByKey[key].append( index );
      FGames.append( game );
      return true;
   } );
   if ( !first )
      return fail( first );
   if ( byId.isEmpty() )
      return {};

   if ( !aDevice.seek( 0 ) )
      return fail( Error{ aDevice.errorString() } );
   // Alternate names and images carry no platform, only the game's id: keep the ones whose
   // id the first pass found. Two passes because the document order is not a contract.
   const Result<> second = forEachRecord( aDevice, [&]( QXmlStreamReader& aXml ) {
      const bool isAlt = aXml.name() == QLatin1String( "GameAlternateName" );
      if ( !isAlt && aXml.name() != QLatin1String( "GameImage" ) )
         return false;
      const Fields f = readFields(
         aXml, { "DatabaseID", "AlternateName", "FileName", "Type", "Region" } );
      const auto it = byId.constFind( f.value( QStringLiteral( "DatabaseID" ) ) );
      if ( it == byId.constEnd() )
         return true;
      Entry& game = FGames[it.value()];
      if ( isAlt ) {
         const QString name = f.value( QStringLiteral( "AlternateName" ) );
         game.alts.append( { name, f.value( QStringLiteral( "Region" ) ) } );
         const QString key = launchBoxNameKey( name );
         if ( !key.isEmpty() )
            FByAltKey[key].append( it.value() );
      } else {
         game.images.append( { f.value( QStringLiteral( "FileName" ) ),
                               f.value( QStringLiteral( "Type" ) ),
                               f.value( QStringLiteral( "Region" ) ) } );
      }
      return true;
   } );
   if ( !second )
      return fail( second );
   return {};
}

Result<> LaunchBoxDb::loadFiles( QIODevice& aDevice, const QString& aPlatform )
{
   FFiles.clear();
   if ( const Result<> ok = checkSeekable( aDevice ); !ok )
      return ok;
   const Result<> done = forEachRecord( aDevice, [&]( QXmlStreamReader& aXml ) {
      if ( aXml.name() != QLatin1String( "File" ) )
         return false;
      const Fields f = readFields( aXml, { "FileName", "Platform", "GameName" } );
      if ( f.value( QStringLiteral( "Platform" ) ) == aPlatform )
         FFiles.insert( f.value( QStringLiteral( "FileName" ) ).toLower(),
                        f.value( QStringLiteral( "GameName" ) ) );
      return true;
   } );
   if ( !done )
      FFiles.clear();
   return done;
}

Result<> LaunchBoxDb::loadMame( QIODevice& aDevice )
{
   FMame.clear();
   if ( const Result<> ok = checkSeekable( aDevice ); !ok )
      return ok;
   const Result<> done = forEachRecord( aDevice, [&]( QXmlStreamReader& aXml ) {
      if ( aXml.name() != QLatin1String( "MameFile" ) )
         return false;
      const Fields f = readFields( aXml, { "FileName", "Name" } );
      FMame.insert( f.value( QStringLiteral( "FileName" ) ).toLower(),
                    f.value( QStringLiteral( "Name" ) ) );
      return true;
   } );
   if ( !done )
      FMame.clear();
   return done;
}

Result<LaunchBoxMatch> LaunchBoxDb::find( const LaunchBoxQuery& aQuery ) const
{
   const QString rom = aQuery.romName.toLower();
   QStringList names;
   if ( FFiles.contains( rom ) )
      names << FFiles.value( rom );
   if ( FMame.contains( rom ) )
      names << FMame.value( rom );
   names << aQuery.romName << aQuery.gameName;

   QStringList keys;
   const auto addKey = [&keys]( const QString& aName ) {
      const QString key = launchBoxNameKey( aName );
      if ( !key.isEmpty() && !keys.contains( key ) )
         keys << key;
   };
   for ( const QString& name : std::as_const( names ) )
      addKey( name );
   for ( const QString& name : std::as_const( names ) )
      addKey( articleFront( name ) );

   for ( const auto* table : { &FByKey, &FByAltKey } ) {
      for ( const QString& key : std::as_const( keys ) ) {
         const auto it = table->constFind( key );
         if ( it == table->constEnd() )
            continue;
         LaunchBoxMatch match;
         match.info = toInfo( FGames.at( it.value().first() ), aQuery );
         match.ambiguous = it.value().size() > 1;
         return match;
      }
   }
   return Error{ QStringLiteral( "Game not found" ) };
}

ScrapedInfo LaunchBoxDb::toInfo( const Entry& aGame, const LaunchBoxQuery& aQuery ) const
{
   ScrapedInfo info;
   info.name = aGame.name;
   info.description = aGame.overview;
   info.developer = aGame.developer;
   info.publisher = aGame.publisher;
   info.players = aGame.maxPlayers;
   info.maxThreads = 2;

   const QStringList regions = regionPreference( aQuery.region, aQuery.romName );

   for ( const QString& region : regions ) {
      const auto alt = std::find_if( aGame.alts.cbegin(), aGame.alts.cend(), [&]( const Alt& aAlt ) {
         return !aAlt.name.isEmpty() && sameText( aAlt.region, region );
      } );
      if ( alt != aGame.alts.cend() ) {
         info.name = alt->name;
         info.region = alt->region;
         break;
      }
   }

   info.releaseDate = formatScrapedDate( aGame.releaseDate.left( 10 ) );
   if ( info.releaseDate.isEmpty() )
      info.releaseDate = formatScrapedDate( aGame.releaseYear );

   QStringList genres;
   for ( const QString& genre : aGame.genres.split( QLatin1Char( ';' ), Qt::SkipEmptyParts ) ) {
      if ( !genre.trimmed().isEmpty() )
         genres << genre.trimmed();
   }
   info.genre = genres.join( QStringLiteral( " - " ) );

   bool ratingOk = false;
   const double rating = aGame.rating.toDouble( &ratingOk );
   if ( ratingOk && rating > 0 )
      info.rating = QString::number( rating / 5.0, 'f', 2 );

   // Within a type the game's region wins, else the first image. Box 3D follows Box 2D's region.
   const auto pick = [&aGame]( const QStringList& aTypes, const QStringList& aRegions ) {
      for ( const QString& type : aTypes ) {
         const Image* firstOfType = nullptr;
         for ( const QString& region : aRegions ) {
            for ( const Image& image : aGame.images ) {
               if ( image.type == type && sameText( image.region, region ) )
                  return &image;
            }
         }
         for ( const Image& image : aGame.images ) {
            if ( image.type == type ) {
               firstOfType = &image;
               break;
            }
         }
         if ( firstOfType )
            return firstOfType;
      }
      return static_cast<const Image*>( nullptr );
   };

   const Image* box2d = pick( mediaSlots().first().types, regions );
   QStringList boxRegions = regions;
   if ( box2d && !box2d->region.isEmpty() )
      boxRegions.prepend( box2d->region );

   for ( const MediaSlot& slot : mediaSlots() ) {
      if ( !aQuery.mediaTypes.contains( QLatin1String( slot.media ) ) )
         continue;
      const bool is3d = QLatin1String( slot.media ) == QLatin1String( Cst_MediaBox3d );
      const Image* image = pick( slot.types, is3d ? boxRegions : regions );
      if ( !image || image->fileName.isEmpty() )
         continue;
      info.pictures.append(
         { QFileInfo( image->fileName ).suffix().toLower(), Cst_ImageBase + image->fileName } );
   }
   return info;
}
