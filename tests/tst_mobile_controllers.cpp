// The mobile controllers run without QML: driven the way a page would, over the shared fixture.

#include "FileBrowserController.h"
#include "GameEditController.h"
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
   GameEditController edit{ &library, &games };

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

QStringList names( const FileBrowserController& aBrowser )
{
   QStringList result;
   for ( const QVariant& entry : aBrowser.entries() )
      result.append( entry.toMap().value( QStringLiteral( "name" ) ).toString() );
   return result;
}

TEST( FileBrowserController, ListsTheVolumesThenFoldersAndGoesBackUp )
{
   QTemporaryDir storage;
   const QString internal = storage.filePath( QStringLiteral( "emulated/0" ) );
   ASSERT_TRUE( QDir().mkpath( internal + QStringLiteral( "/ROMs/snes" ) ) );
   ASSERT_TRUE( QDir().mkpath( internal + QStringLiteral( "/.hidden" ) ) );
   ASSERT_TRUE( QDir().mkpath( storage.filePath( QStringLiteral( "1107-2E5F/Games" ) ) ) );
   ASSERT_TRUE( QDir().mkpath( storage.filePath( QStringLiteral( "self" ) ) ) );

   FileBrowserController browser( storage.path() );
   EXPECT_TRUE( browser.atRoots() );
   EXPECT_EQ( names( browser ), QStringList( { QStringLiteral( "Internal storage" ), QStringLiteral( "1107-2E5F" ) } ) );

   browser.open( internal );
   EXPECT_FALSE( browser.atRoots() );
   EXPECT_EQ( names( browser ), QStringList( { QStringLiteral( ".hidden" ), QStringLiteral( "ROMs" ) } ) );

   browser.open( internal + QStringLiteral( "/ROMs" ) );
   EXPECT_EQ( names( browser ), QStringList( QStringLiteral( "snes" ) ) );
   browser.up();
   EXPECT_EQ( browser.path(), internal );
   browser.up();
   EXPECT_TRUE( browser.atRoots() );

   browser.open( storage.filePath( QStringLiteral( "nowhere" ) ) );
   EXPECT_TRUE( browser.atRoots() );
}

TEST( GameEditController, TracksTypedValuesAndSavesThem )
{
   MobileFixture fixture;
   auto& edit = fixture.edit;
   fixture.games.select( 2 );  // "Vanished", the last row

   EXPECT_EQ( edit.values().value( QStringLiteral( "name" ) ).toString(), QStringLiteral( "Vanished" ) );
   EXPECT_EQ( edit.values().value( QStringLiteral( "romPath" ) ).toString(), QStringLiteral( "./Vanished.zip" ) );
   EXPECT_FALSE( edit.isDirty() );

   edit.setField( QStringLiteral( "name" ), QStringLiteral( "Renamed" ) );
   edit.setField( QStringLiteral( "favorite" ), 1 );
   edit.setField( QStringLiteral( "nonsense" ), 1 );
   EXPECT_TRUE( edit.isDirty() );
   edit.revert();
   EXPECT_FALSE( edit.isDirty() );
   EXPECT_EQ( edit.values().value( QStringLiteral( "name" ) ).toString(), QStringLiteral( "Vanished" ) );

   edit.setField( QStringLiteral( "name" ), QStringLiteral( "Renamed" ) );
   edit.setField( QStringLiteral( "favorite" ), 1 );
   ASSERT_TRUE( edit.save() );
   EXPECT_FALSE( edit.isDirty() );

   const Gamelist again = reload( fixture );
   EXPECT_EQ( again.at( 2 ).name, QStringLiteral( "Renamed" ) );
   EXPECT_EQ( again.at( 2 ).favorite, 1 );
   fixture.games.setSearch( QStringLiteral( "renamed" ) );
   EXPECT_EQ( qobject_cast<QAbstractItemModel*>( fixture.games.model() )->rowCount(), 1 );
}

TEST( GameEditController, KeepsTheTypedValuesWhenTheSaveFails )
{
   MobileFixture fixture;
   fixture.games.select( 2 );
   breakSave( fixture, *fixture.library.gamelist() );

   fixture.edit.setField( QStringLiteral( "name" ), QStringLiteral( "Renamed" ) );
   EXPECT_FALSE( fixture.edit.save() );
   EXPECT_FALSE( fixture.edit.error().isEmpty() );
   EXPECT_TRUE( fixture.edit.isDirty() );
   EXPECT_EQ( fixture.edit.values().value( QStringLiteral( "name" ) ).toString(), QStringLiteral( "Renamed" ) );
}

TEST( GameEditController, RemovesMediaWithoutLosingTypedValues )
{
   MobileFixture fixture;
   auto& edit = fixture.edit;
   fixture.games.select( 0 );  // the first Sonic, which has a picture and a video
   ASSERT_FALSE( edit.imageSource().isEmpty() );
   ASSERT_TRUE( edit.hasVideo() );

   edit.setField( QStringLiteral( "genre" ), QStringLiteral( "Racing" ) );
   EXPECT_TRUE( edit.removeImage() );
   EXPECT_TRUE( edit.removeVideo() );
   EXPECT_TRUE( edit.imageSource().isEmpty() );
   EXPECT_FALSE( edit.hasVideo() );
   EXPECT_TRUE( edit.isDirty() );

   fixture.library.selectSystem( fixture.library.current() );
   EXPECT_TRUE( edit.values().isEmpty() );
}

}  // namespace
