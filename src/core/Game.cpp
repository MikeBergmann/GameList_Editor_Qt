#include "Game.h"

#include <QFile>

#include <zlib.h>

namespace {
constexpr int Cst_ChunkSize = 64 * 1024;
}

int gamelistFlag( const QString& aValue )
{
   return aValue == QLatin1String( "true" ) ? 1 : 0;
}

GameFields gameFields( const Game& aGame )
{
   GameFields fields;
   fields.name = aGame.name;
   fields.genre = aGame.genre;
   fields.rating = aGame.rating;
   fields.players = aGame.players;
   fields.developer = aGame.developer;
   fields.publisher = aGame.publisher;
   fields.releaseDate = aGame.releaseDate;
   fields.description = aGame.description;
   fields.region = aGame.region;
   fields.kidGame = aGame.kidGame;
   fields.hidden = aGame.hidden;
   fields.favorite = aGame.favorite;
   return fields;
}

GameFieldSet changedFields( const Game& aGame, const GameFields& aValues )
{
   GameFieldSet which;

   const auto differs = [&which]( GameField aField, const QString& aStored, const QString& aValue ) {
      if ( aStored != aValue )
         which |= aField;
   };

   differs( gfName, aGame.name, aValues.name );
   differs( gfGenre, aGame.genre, aValues.genre );
   differs( gfRating, aGame.rating, aValues.rating );
   differs( gfPlayers, aGame.players, aValues.players );
   differs( gfDeveloper, aGame.developer, aValues.developer );
   differs( gfPublisher, aGame.publisher, aValues.publisher );
   differs( gfReleaseDate, aGame.releaseDate, aValues.releaseDate );
   differs( gfDescription, aGame.description, aValues.description );
   differs( gfRegion, aGame.region, aValues.region );

   // A combo at -1 is no answer at all, not "No" - it is what the editor shows
   // while several games are selected, and it must not write a 0 over a 1.
   if ( aValues.kidGame >= 0 && aValues.kidGame != aGame.kidGame )
      which |= gfKidGame;
   if ( aValues.hidden >= 0 && aValues.hidden != aGame.hidden )
      which |= gfHidden;
   if ( aValues.favorite >= 0 && aValues.favorite != aGame.favorite )
      which |= gfFavorite;

   return which;
}

GameFieldSet filledFields( const GameFields& aValues )
{
   GameFieldSet which;

   const auto filled = [&which]( GameField aField, const QString& aValue ) {
      if ( !aValue.isEmpty() )
         which |= aField;
   };

   filled( gfName, aValues.name );
   filled( gfGenre, aValues.genre );
   filled( gfRating, aValues.rating );
   filled( gfPlayers, aValues.players );
   filled( gfDeveloper, aValues.developer );
   filled( gfPublisher, aValues.publisher );
   filled( gfReleaseDate, aValues.releaseDate );
   filled( gfDescription, aValues.description );
   filled( gfRegion, aValues.region );

   if ( aValues.kidGame >= 0 )
      which |= gfKidGame;
   if ( aValues.hidden >= 0 )
      which |= gfHidden;
   if ( aValues.favorite >= 0 )
      which |= gfFavorite;

   return which;
}

QDateTime gamelistDateTime( const QString& aValue )
{
   // 'T' is quoted because a bare t is Qt's time-zone specifier.
   return QDateTime::fromString( aValue, QStringLiteral( "yyyyMMdd'T'HHmmss" ) );
}

QString fileHash( const QString& aPath, QCryptographicHash::Algorithm aAlgorithm )
{
   QFile file( aPath );
   if ( !file.open( QIODevice::ReadOnly ) )
      return {};

   QCryptographicHash hash( aAlgorithm );
   if ( !hash.addData( &file ) )
      return {};

   return QString::fromLatin1( hash.result().toHex() ).toUpper();
}

// Qt has no CRC32, so this is zlib's - already a dependency of Qt6Core.
QString fileCrc32( const QString& aPath )
{
   QFile file( aPath );
   if ( !file.open( QIODevice::ReadOnly ) )
      return {};

   uLong crc = crc32( 0, nullptr, 0 );
   QByteArray chunk( Cst_ChunkSize, '\0' );
   for ( qint64 read = file.read( chunk.data(), chunk.size() ); read > 0;
         read = file.read( chunk.data(), chunk.size() ) ) {
      crc = crc32( crc, reinterpret_cast<const Bytef*>( chunk.constData() ),
                    static_cast<uInt>( read ) );
   }

   if ( file.error() != QFileDevice::NoError )
      return {};

   return QString::number( crc, 16 ).toUpper().rightJustified( 8, QLatin1Char( '0' ) );
}
