#include "Game.h"

#include "Resources.h"

int gamelistFlag( const QString& aValue )
{
   return aValue == QLatin1String( "true" ) ? 1 : 0;
}

// Hand-rolled: a partly-known date carries "00" for the unknown month or day, and the
// display form leaves those parts out.
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
   if ( month > 0 ) {
      if ( day > 0 )
         display += aValue.mid( 6, 2 ) + QLatin1Char( '/' );
      display += aValue.mid( 4, 2 ) + QLatin1Char( '/' );
   }
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
         return aDate + Cst_DateLongFill + Cst_DateSuffix;
      case 7:  // MM/yyyy
         return aDate.mid( 3, 4 ) + aDate.left( 2 ) + Cst_DateShortFill +
                Cst_DateSuffix;
      case 10:  // dd/MM/yyyy
         return aDate.mid( 6, 4 ) + aDate.mid( 3, 2 ) + aDate.left( 2 ) +
                Cst_DateSuffix;
      default: return {};
   }
}

QDate displayDateToDate( const QString& aDisplayDate )
{
   switch ( aDisplayDate.size() ) {
      case 4: return QDate::fromString( aDisplayDate, QStringLiteral( "yyyy" ) );
      case 7: return QDate::fromString( aDisplayDate, QStringLiteral( "MM/yyyy" ) );
      case 10: return QDate::fromString( aDisplayDate, QStringLiteral( "dd/MM/yyyy" ) );
      default: return {};
   }
}

QString isoDateToDisplay( const QString& aIso )
{
   switch ( aIso.size() ) {
      case 4: return aIso;
      case 7: return aIso.mid( 5, 2 ) + QLatin1Char( '/' ) + aIso.left( 4 );
      case 10:
         return aIso.mid( 8, 2 ) + QLatin1Char( '/' ) + aIso.mid( 5, 2 ) + QLatin1Char( '/' ) +
                aIso.left( 4 );
      default: return {};
   }
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

   const auto differs = [&which]( GameField aField, const QString& aStored,
                                  const QString& aValue ) {
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

QString gamelistDateTimeToDisplay( const QString& aValue )
{
   const QDateTime parsed = gamelistDateTime( aValue );
   return parsed.isValid() ? parsed.toString( QStringLiteral( "dd/MM/yyyy HH:mm:ss" ) ) : QString();
}
