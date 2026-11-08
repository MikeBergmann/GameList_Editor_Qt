// The mobile controllers run without QML: driven the way a page would, over the shared fixture.

#include "GamelistFixture.h"
#include "GamesController.h"
#include "LibraryController.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

namespace {

int systemIndex( const LibraryController& aLibrary, const QString& aFolder )
{
   const QVariantList systems = aLibrary.systems();
   for ( int i = 0; i < systems.size(); ++i )
      if ( systems.at( i ).toMap().value( QStringLiteral( "folder" ) ) == aFolder )
         return i;
   return -1;
}

struct MobileFixture : Fixture
{
   LibraryController library;
   GamesController games{ &library };

   MobileFixture()
   {
      EXPECT_TRUE( library.openRoot( dir.path() ) );
      EXPECT_TRUE( library.selectSystem( systemIndex( library, QStringLiteral( "snes" ) ) ) );
   }
};

TEST( LibraryController, ListsSystemsWithAGamelistAndRemembersTheRoot )
{
   Fixture fixture;
   LibraryController library;

   ASSERT_TRUE( library.openRoot( fixture.dir.path() ) );
   EXPECT_EQ( library.systems().size(), 2 );
   EXPECT_FALSE( library.isLoaded() );
   EXPECT_EQ( library.lastFolder(), fixture.dir.path() );

   const QVariantMap snes = library.systems().at( systemIndex( library, QStringLiteral( "snes" ) ) ).toMap();
   EXPECT_TRUE( snes.value( QStringLiteral( "logo" ) ).toString().startsWith( QStringLiteral( "qrc:/SystemsLogos/" ) ) );
}

TEST( LibraryController, FailsOnAFolderWithoutGamelists )
{
   QTemporaryDir empty;
   LibraryController library;

   EXPECT_FALSE( library.openRoot( empty.path() ) );
   EXPECT_TRUE( library.systems().isEmpty() );
   EXPECT_FALSE( library.error().isEmpty() );
}

TEST( LibraryController, ReportsAGamelistThatDoesNotLoadAndDetachesTheOldOne )
{
   Fixture fixture;
   LibraryController library;
   ASSERT_TRUE( library.openRoot( fixture.dir.path() ) );
   const int snes = systemIndex( library, QStringLiteral( "snes" ) );
   ASSERT_TRUE( library.selectSystem( snes ) );
   ASSERT_NE( library.gamelist(), nullptr );

   writeFile( fixture.snes.gamelistPath, "<gameList><game>" );
   EXPECT_FALSE( library.selectSystem( snes ) );
   EXPECT_EQ( library.gamelist(), nullptr );
   EXPECT_EQ( library.current(), -1 );
   EXPECT_FALSE( library.error().isEmpty() );
   EXPECT_FALSE( library.selectSystem( 99 ) );
}

TEST( GamesController, FiltersAndCountsLikeTheDesktopList )
{
   MobileFixture fixture;
   auto* model = qobject_cast<QAbstractItemModel*>( fixture.games.model() );

   EXPECT_EQ( model->rowCount(), 3 );
   EXPECT_EQ( fixture.games.countText(), QStringLiteral( "3 game(s) found." ) );

   fixture.games.setSearch( QStringLiteral( "vanish" ) );
   EXPECT_EQ( model->rowCount(), 1 );

   fixture.games.setSearch( {} );
   fixture.games.setCategory( static_cast<int>( FilterCategory::Favorite ) );
   EXPECT_EQ( model->rowCount(), 1 );
   EXPECT_EQ( fixture.games.countText(), QStringLiteral( "1 / 3 game(s) found." ) );

   fixture.games.setCategory( 999 );
   EXPECT_EQ( fixture.games.category(), static_cast<int>( FilterCategory::Favorite ) );
}

TEST( GamesController, SameAsFollowsTheSelectedGame )
{
   MobileFixture fixture;
   auto* model = qobject_cast<QAbstractItemModel*>( fixture.games.model() );

   fixture.games.select( 0 );
   EXPECT_GE( fixture.games.current(), 0 );
   fixture.games.setCategory( static_cast<int>( FilterCategory::SameName ) );
   EXPECT_EQ( model->rowCount(), 2 );

   fixture.games.select( 99 );
   EXPECT_EQ( fixture.games.current(), -1 );
}

TEST( GamesController, ANewSystemClearsSearchAndSelectionButKeepsTheCategory )
{
   MobileFixture fixture;
   auto* model = qobject_cast<QAbstractItemModel*>( fixture.games.model() );
   fixture.games.setSearch( QStringLiteral( "vanish" ) );
   fixture.games.setCategory( static_cast<int>( FilterCategory::MissingImage ) );
   fixture.games.select( 0 );

   ASSERT_TRUE( fixture.library.selectSystem( systemIndex( fixture.library, QStringLiteral( "megadrive" ) ) ) );
   EXPECT_EQ( fixture.games.search(), QString() );
   EXPECT_EQ( fixture.games.current(), -1 );
   EXPECT_EQ( fixture.games.category(), static_cast<int>( FilterCategory::MissingImage ) );

   writeFile( fixture.snes.gamelistPath, "<gameList><game>" );
   EXPECT_FALSE( fixture.library.selectSystem( systemIndex( fixture.library, QStringLiteral( "snes" ) ) ) );
   EXPECT_EQ( model->rowCount(), 0 );
}

}  // namespace
