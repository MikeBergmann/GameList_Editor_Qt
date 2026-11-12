#include "GameEditController.h"

#include "GamesController.h"
#include "LibraryController.h"

#include <QThread>
#include <QUrl>

namespace {

struct Field
{
   const char* key;
   QString GameFields::* text;
   int GameFields::* flag;
};

const Field Cst_Fields[] = {
   { "name", &GameFields::name, nullptr },
   { "genre", &GameFields::genre, nullptr },
   { "rating", &GameFields::rating, nullptr },
   { "players", &GameFields::players, nullptr },
   { "developer", &GameFields::developer, nullptr },
   { "publisher", &GameFields::publisher, nullptr },
   { "releaseDate", &GameFields::releaseDate, nullptr },
   { "description", &GameFields::description, nullptr },
   { "region", &GameFields::region, nullptr },
   { "kidGame", nullptr, &GameFields::kidGame },
   { "hidden", nullptr, &GameFields::hidden },
   { "favorite", nullptr, &GameFields::favorite },
};

}  // namespace

GameEditController::GameEditController( LibraryController* aLibrary, GamesController* aGames,
                                        QObject* aParent )
   : QObject( aParent ), FLibrary( aLibrary ), FGames( aGames )
{
   connect( FGames, &GamesController::currentChanged, this, [this] { load( FGames->current() ); } );
}

const Game* GameEditController::game() const
{
   const Gamelist* list = FLibrary->gamelist();
   return list && FIndex >= 0 && FIndex < list->count() ? &list->at( FIndex ) : nullptr;
}

QVariantMap GameEditController::values() const
{
   const Game* current = game();
   if ( !current )
      return {};

   QVariantMap result{ { QStringLiteral( "romPath" ), current->romPath } };
   for ( const Field& field : Cst_Fields )
      result.insert( QLatin1String( field.key ), field.text ? QVariant( FFields.*field.text )
                                                            : QVariant( FFields.*field.flag ) );
   return result;
}

bool GameEditController::isDirty() const
{
   const Game* current = game();
   return current && changedFields( *current, FFields );
}

QString GameEditController::imageSource() const
{
   const Game* current = game();
   if ( !current || current->missingImage )
      return {};

   QUrl url = QUrl::fromLocalFile( current->physicalImagePath );
   url.setQuery( QString::number( FMediaVersion ) );
   return url.toString();
}

bool GameEditController::hasVideo() const
{
   const Game* current = game();
   return current && !current->missingVideo;
}

GameEditController::~GameEditController()
{
   if ( FHashThread )
      FHashThread->wait();
}

QVariantMap GameEditController::info() const
{
   const Game* current = game();
   if ( !current )
      return {};

   const auto file = []( const QString& aPath, bool aMissing ) {
      return aMissing && !aPath.isEmpty() ? aPath + QStringLiteral( " (missing)" ) : aPath;
   };
   return {
      { QStringLiteral( "gamelist" ), FLibrary->gamelist()->system().gamelistPath },
      { QStringLiteral( "rom" ), file( current->physicalRomPath, current->isOrphan ) },
      { QStringLiteral( "image" ), file( current->physicalImagePath, current->missingImage ) },
      { QStringLiteral( "video" ), file( current->physicalVideoPath, current->missingVideo ) },
      { QStringLiteral( "playcount" ), current->playcount },
      { QStringLiteral( "lastPlayed" ), gamelistDateTimeToDisplay( current->lastplayed ) },
      { QStringLiteral( "crc32" ), current->crc32 },
      { QStringLiteral( "md5" ), current->md5 },
      { QStringLiteral( "sha1" ), current->sha1 },
   };
}

void GameEditController::computeHashes()
{
   Gamelist* list = FLibrary->gamelist();
   const Game* current = game();
   if ( FHashThread || !list || !current || current->isOrphan )
      return;

   // A large disc image would run into Android's ~5 s ANR limit on the UI thread.
   const int index = FIndex;
   FHashThread = QThread::create( [list, index] { list->ensureHashes( index ); } );
   FHashThread->setParent( this );
   connect( FHashThread, &QThread::finished, this, [this] {
      FHashThread->deleteLater();
      FHashThread = nullptr;
      emit infoChanged();
   } );
   emit infoChanged();  // before start(): the thread writes the hashes into the Game
   FHashThread->start();
}

void GameEditController::setField( const QString& aKey, const QVariant& aValue )
{
   if ( !game() )
      return;

   for ( const Field& field : Cst_Fields ) {
      if ( aKey != QLatin1String( field.key ) )
         continue;

      if ( field.text )
         FFields.*field.text = aValue.toString();
      else
         FFields.*field.flag = aValue.toInt();

      emit valuesChanged();
      emit dirtyChanged();
      return;
   }
}

bool GameEditController::save()
{
   Gamelist* list = FLibrary->gamelist();
   const Game* current = game();
   if ( !list || !current )
      return false;

   const GameFieldSet which = changedFields( *current, FFields );
   if ( which ) {
      if ( const Result<> saved = list->setFields( { FIndex }, FFields, which ); !saved ) {
         setError( saved.error() );
         return false;
      }
      FGames->refresh();
   }

   // A date that would not parse comes back blank, so show what was stored.
   revert();
   return true;
}

void GameEditController::revert()
{
   load( FIndex );
}

bool GameEditController::removeImage()
{
   Gamelist* list = FLibrary->gamelist();
   return list && game() && finishMedia( list->removeImage( FIndex ) );
}

bool GameEditController::removeVideo()
{
   Gamelist* list = FLibrary->gamelist();
   return list && game() && finishMedia( list->removeVideo( FIndex ) );
}

bool GameEditController::changeImage( const QString& aPath )
{
   Gamelist* list = FLibrary->gamelist();
   return list && game() && finishMedia( list->setImage( FIndex, aPath ) );
}

bool GameEditController::changeVideo( const QString& aPath )
{
   Gamelist* list = FLibrary->gamelist();
   return list && game() && finishMedia( list->setVideo( FIndex, aPath ) );
}

bool GameEditController::finishMedia( const Result<>& aResult )
{
   if ( !aResult ) {
      setError( aResult.error() );
      return false;
   }

   setError( {} );
   ++FMediaVersion;
   FGames->refresh();
   emit mediaChanged();
   emit infoChanged();
   return true;
}

void GameEditController::load( int aIndex )
{
   FIndex = aIndex;
   FFields = game() ? gameFields( *game() ) : GameFields();
   FError.clear();
   ++FMediaVersion;

   emit valuesChanged();
   emit dirtyChanged();
   emit errorChanged();
   emit mediaChanged();
   emit infoChanged();
}

void GameEditController::setError( const QString& aError )
{
   FError = aError;
   emit errorChanged();
}
