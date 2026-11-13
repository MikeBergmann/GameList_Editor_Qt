// The mobile controllers run without QML: driven the way a page would, over the shared fixture.

#include "ActionsController.h"
#include "FileBrowserController.h"
#include "GameEditController.h"
#include "GamelistFixture.h"
#include "GamesController.h"
#include "LibraryController.h"
#include "QtPrinters.h"
#include "ScrapeController.h"
#include "SettingsController.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QImage>
#include <QSettings>
#include <QTimer>

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

TEST( LibraryController, MapsAnUnknownFolderToASystemTypeAndForgetsItAgain )
{
   Fixture fixture;
   writeFile( fixture.path( "amiga500/gamelist.xml" ), Cst_Gamelist );
   LibraryController library;
   ASSERT_TRUE( library.openRoot( fixture.dir.path() ) );
   const int amiga = systemIndex( library, QStringLiteral( "amiga500" ) );
   const int snes = systemIndex( library, QStringLiteral( "snes" ) );
   EXPECT_TRUE( library.canSetKind( amiga ) );
   EXPECT_FALSE( library.canSetKind( snes ) ) << "a known folder would ignore the mapping";
   EXPECT_EQ( library.kindChoice( amiga ), 0 );

   const SystemInfo& info = systemInfo( skAM );
   const int choice = static_cast<int>( library.kindLabels().indexOf(
      QStringLiteral( "%1 (%2)" ).arg( QLatin1String( info.name ), QLatin1String( info.folder ) ) ) );
   ASSERT_GT( choice, 0 );

   library.setKind( snes, choice );
   EXPECT_FALSE( library.systems().at( snes ).toMap().value( QStringLiteral( "name" ) ).toString()
                    .contains( QLatin1String( info.name ) ) ) << "the known folder is untouched";

   library.setKind( amiga, choice );
   EXPECT_EQ( library.kindChoice( amiga ), choice );
   EXPECT_TRUE( library.systems().at( amiga ).toMap().value( QStringLiteral( "name" ) ).toString()
                   .contains( QLatin1String( info.name ) ) );

   LibraryController again;
   ASSERT_TRUE( again.openRoot( fixture.dir.path() ) );
   EXPECT_EQ( again.kindChoice( systemIndex( again, QStringLiteral( "amiga500" ) ) ), choice ) << "stored";

   library.setKind( amiga, 0 );
   EXPECT_EQ( library.kindChoice( amiga ), 0 );
   QSettings().remove( Cst_IniSystemMap );
}

TEST( ActionsController, AddsTheChosenMissingRomsAndResetsTheGamesList )
{
   MobileFixture fixture;
   writeFile( fixture.path( "snes/Streets.zip" ), "rom" );
   writeFile( fixture.path( "snes/sub/Zelda.sfc" ), "rom" );
   auto* model = qobject_cast<QAbstractItemModel*>( fixture.games.model() );
   ASSERT_EQ( model->rowCount(), 3 );

   ActionsController actions( &fixture.library );
   ASSERT_TRUE( actions.findMissingRoms() );
   EXPECT_EQ( actions.roms(), QStringList( { QStringLiteral( "Streets.zip" ), QStringLiteral( "sub/Zelda.sfc" ) } ) );

   ASSERT_TRUE( actions.addRoms( { 1, 99 } ) ) << "an unknown row is ignored";
   EXPECT_EQ( actions.added(), 1 );
   EXPECT_EQ( model->rowCount(), 4 );
   EXPECT_TRUE( fixture.library.isLoaded() );

   ASSERT_TRUE( actions.findMissingRoms() );
   EXPECT_EQ( actions.roms(), QStringList( QStringLiteral( "Streets.zip" ) ) );
}

TEST( ActionsController, FailsWithoutALoadedSystem )
{
   LibraryController library;
   ActionsController actions( &library );

   EXPECT_FALSE( actions.findMissingRoms() );
   EXPECT_FALSE( actions.error().isEmpty() );
   EXPECT_FALSE( actions.addRoms( { 0 } ) );
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

TEST( GamesController, StepsToTheNeighbourInTheFilteredListAndStopsAtTheEnds )
{
   MobileFixture fixture;
   auto& games = fixture.games;
   EXPECT_FALSE( games.step( 1 ) ) << "nothing selected";

   games.select( 0 );
   EXPECT_EQ( games.currentRow(), 0 );
   EXPECT_FALSE( games.step( -1 ) );
   EXPECT_TRUE( games.step( 1 ) );
   EXPECT_EQ( games.currentRow(), 1 );
   EXPECT_EQ( games.current(), games.sourceRow( 1 ) );
   EXPECT_TRUE( games.step( 1 ) );
   EXPECT_FALSE( games.step( 1 ) );
   EXPECT_EQ( games.currentRow(), 2 );

   games.setSearch( QStringLiteral( "vanish" ) );
   EXPECT_EQ( games.currentRow(), 0 ) << "the row is in the filtered list";
   EXPECT_FALSE( games.step( 1 ) );
   games.setSearch( QStringLiteral( "sonic" ) );
   EXPECT_EQ( games.currentRow(), -1 ) << "filtered out";
   EXPECT_FALSE( games.step( 1 ) );
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

TEST( FileBrowserController, ListsAVolumeTheSystemReportsOnceEvenWhenStorageCannotBeListed )
{
   QTemporaryDir storage;
   QTemporaryDir card;
   ASSERT_TRUE( QDir().mkpath( storage.filePath( QStringLiteral( "emulated/0" ) ) ) );
   const QString listed = storage.filePath( QStringLiteral( "1107-2E5F" ) );
   ASSERT_TRUE( QDir().mkpath( listed ) );

   FileBrowserController browser( storage.path() );
   browser.setVolumeSource( [&] {
      return QStringList( { card.path(), listed, storage.filePath( QStringLiteral( "gone" ) ) } );
   } );
   EXPECT_EQ( names( browser ),
              QStringList( { QStringLiteral( "Internal storage" ), QStringLiteral( "1107-2E5F" ),
                             QFileInfo( card.path() ).fileName() } ) );
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

TEST( FileBrowserController, ListsFilesWithTheChosenSuffixesAfterTheFolders )
{
   QTemporaryDir storage;
   const QString internal = storage.filePath( QStringLiteral( "emulated/0" ) );
   ASSERT_TRUE( QDir().mkpath( internal + QStringLiteral( "/Pics" ) ) );
   writeFile( internal + QStringLiteral( "/b.PNG" ), "x" );
   writeFile( internal + QStringLiteral( "/a.jpg" ), "x" );
   writeFile( internal + QStringLiteral( "/notes.txt" ), "x" );

   FileBrowserController browser( storage.path() );
   browser.open( internal );
   EXPECT_EQ( names( browser ), QStringList( QStringLiteral( "Pics" ) ) );

   browser.setSuffixes( { QStringLiteral( "png" ), QStringLiteral( "jpg" ) } );
   EXPECT_EQ( browser.path(), internal );
   EXPECT_EQ( names( browser ), QStringList( { QStringLiteral( "Pics" ), QStringLiteral( "a.jpg" ), QStringLiteral( "b.PNG" ) } ) );
   EXPECT_FALSE( browser.entries().at( 1 ).toMap().value( QStringLiteral( "isDir" ) ).toBool() );
}

TEST( GameEditController, ChangesMediaWithoutLosingTypedValues )
{
   MobileFixture fixture;
   auto& edit = fixture.edit;
   fixture.games.select( 2 );  // Vanished: no media yet
   ASSERT_TRUE( edit.imageSource().isEmpty() );
   ASSERT_FALSE( edit.hasVideo() );
   EXPECT_TRUE( edit.videoSource().isEmpty() );

   const QString picture = fixture.dir.filePath( QStringLiteral( "new.png" ) );
   QImage image( 4, 4, QImage::Format_RGB32 );
   image.fill( Qt::blue );
   ASSERT_TRUE( image.save( picture ) );
   const QString video = fixture.dir.filePath( QStringLiteral( "new.mp4" ) );
   writeFile( video, "mp4" );

   edit.setField( QStringLiteral( "genre" ), QStringLiteral( "Racing" ) );
   ASSERT_TRUE( edit.changeImage( picture ) ) << edit.error().toStdString();
   ASSERT_TRUE( edit.changeVideo( video ) ) << edit.error().toStdString();
   EXPECT_FALSE( edit.imageSource().isEmpty() );
   EXPECT_TRUE( edit.hasVideo() );
   EXPECT_EQ( edit.videoSource().toLocalFile().section( '/', -1 ), QStringLiteral( "Vanished.mp4" ) );
   EXPECT_TRUE( edit.isDirty() );

   const Gamelist again = reload( fixture );
   EXPECT_FALSE( again.at( 2 ).missingImage );
   EXPECT_FALSE( again.at( 2 ).missingVideo );

   const QString before = edit.imageSource();
   ASSERT_TRUE( edit.changeImage( picture ) );
   EXPECT_NE( edit.imageSource(), before ) << "same path must still reload in the Image";

   EXPECT_FALSE( edit.changeImage( fixture.dir.filePath( QStringLiteral( "missing.png" ) ) ) );
   EXPECT_FALSE( edit.error().isEmpty() );
}

TEST( GameEditController, ShowsTheDetailsAndHashesTheRomOffTheUiThread )
{
   MobileFixture fixture;
   auto& edit = fixture.edit;

   fixture.games.select( 2 );  // Vanished.zip is not on disk
   EXPECT_TRUE( edit.info().value( QStringLiteral( "rom" ) ).toString().endsWith( QStringLiteral( " (missing)" ) ) );
   edit.computeHashes();
   EXPECT_FALSE( edit.isHashing() ) << "nothing to hash";

   fixture.games.select( 0 );
   EXPECT_TRUE( edit.info().value( QStringLiteral( "gamelist" ) ).toString().endsWith( QStringLiteral( "gamelist.xml" ) ) );
   ASSERT_TRUE( edit.info().value( QStringLiteral( "md5" ) ).toString().isEmpty() );

   QEventLoop loop;
   QTimer::singleShot( 10000, &loop, &QEventLoop::quit );
   QObject::connect( &edit, &GameEditController::infoChanged, &loop, [&] { if ( !edit.isHashing() ) loop.quit(); } );
   edit.computeHashes();
   EXPECT_TRUE( edit.isHashing() );
   loop.exec();

   ASSERT_FALSE( edit.isHashing() );
   const QString md5 = QString::fromLatin1( QCryptographicHash::hash( "rom", QCryptographicHash::Md5 ).toHex().toUpper() );
   EXPECT_EQ( edit.info().value( QStringLiteral( "md5" ) ).toString(), md5 );
   EXPECT_FALSE( edit.info().value( QStringLiteral( "crc32" ) ).toString().isEmpty() );
   EXPECT_FALSE( edit.info().value( QStringLiteral( "sha1" ) ).toString().isEmpty() );
}

TEST( GameEditController, OpensTheVideoThroughTheInjectedOpener )
{
   MobileFixture fixture;
   auto& edit = fixture.edit;
   QString opened;
   bool answer = true;
   edit.setOpener( [&]( const QString& aPath ) {
      opened = aPath;
      return answer;
   } );

   fixture.games.select( 2 );  // Vanished: no video
   EXPECT_FALSE( edit.openVideoExternally() );
   EXPECT_TRUE( opened.isEmpty() );

   fixture.games.select( 0 );
   EXPECT_TRUE( edit.openVideoExternally() );
   EXPECT_EQ( opened, edit.videoSource().toLocalFile() );
   answer = false;
   EXPECT_FALSE( edit.openVideoExternally() );
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

TEST( SettingsController, WritesEveryChangeToQSettingsAndNormalisesThem )
{
   SettingsController settings;
   int changes = 0;
   QObject::connect( &settings, &SettingsController::valuesChanged, [&] { ++changes; } );

   settings.setValue( QStringLiteral( "ssUser" ), QStringLiteral( "mike" ) );
   settings.setValue( QStringLiteral( "proxyUse" ), true );
   settings.setValue( QStringLiteral( "proxyPort" ), QStringLiteral( "99999" ) );
   settings.setValue( QStringLiteral( "language" ), 99 );
   settings.setValue( QStringLiteral( "nonsense" ), 1 );
   EXPECT_EQ( changes, 4 );

   const QVariantMap values = settings.values();
   EXPECT_EQ( values.value( QStringLiteral( "ssUser" ) ).toString(), QStringLiteral( "mike" ) );
   EXPECT_TRUE( values.value( QStringLiteral( "proxyUse" ) ).toBool() );
   EXPECT_EQ( values.value( QStringLiteral( "proxyPort" ) ).toString(), QStringLiteral( "65535" ) );
   EXPECT_EQ( values.value( QStringLiteral( "language" ) ).toInt(), static_cast<int>( Cst_IniLanguageDefault ) )
      << "an out-of-range language falls back to the default";

   QSettings stored;
   stored.beginGroup( Cst_IniOptions );
   EXPECT_EQ( stored.value( Cst_IniSSUser ).toString(), QStringLiteral( "mike" ) );

   settings.setValue( QStringLiteral( "ssUser" ), QString() );
   settings.setValue( QStringLiteral( "proxyUse" ), false );
   settings.setValue( QStringLiteral( "proxyPort" ), QString() );
   EXPECT_EQ( settings.values().value( QStringLiteral( "proxyPort" ) ).toString(), QString() );
}

ScrapedInfo sampleInfo()
{
   ScrapedInfo info;
   info.name = QStringLiteral( "Sonic the Hedgehog 2" );
   info.genre = QStringLiteral( "Platform" );
   info.publisher = QStringLiteral( "Sega" );
   return info;
}

QImage redSquare()
{
   QImage image( 4, 4, QImage::Format_RGB32 );
   image.fill( Qt::red );
   return image;
}

// The write is deferred one event-loop turn; the signal count says whether it succeeded.
int saveAndCount( ScrapeController& aScrape )
{
   int saved = 0;
   QObject::connect( &aScrape, &ScrapeController::saved, [&] { ++saved; } );
   aScrape.save();
   QCoreApplication::processEvents();
   return saved;
}

TEST( ScrapeController, OnlyAGameWithItsRomCanBeScraped )
{
   MobileFixture fixture;
   ScrapeController scrape( &fixture.library, &fixture.games );
   EXPECT_FALSE( scrape.canScrape() );

   fixture.games.select( 0 );
   EXPECT_TRUE( scrape.canScrape() );

   fixture.games.select( 2 );  // Vanished.zip: nothing to hash
   EXPECT_FALSE( scrape.canScrape() );
   scrape.scrape();
   EXPECT_FALSE( scrape.isBusy() );
}

TEST( ScrapeController, ASuccessfulReplyFillsTheFieldsAndArmsSaveWhenTheMediaIsDone )
{
   MobileFixture fixture;
   ScrapeController scrape( &fixture.library, &fixture.games );
   fixture.games.select( 0 );

   // No pictures: the empty batch ends at once, inside the gameInfoReady handler.
   emit scrape.scraper().gameInfoReady( sampleInfo() );
   EXPECT_EQ( scrape.values().value( QStringLiteral( "name" ) ).toString(), QStringLiteral( "Sonic the Hedgehog 2" ) );
   EXPECT_TRUE( scrape.hasResult() );
   EXPECT_TRUE( scrape.canSave() );

   scrape.setSaveInfos( false );
   EXPECT_FALSE( scrape.canSave() );

   scrape.setSavePicture( true );
   EXPECT_FALSE( scrape.savePicture() ) << "no picture is selected yet";
   scrape.setSaveVideo( true );
   EXPECT_FALSE( scrape.saveVideo() ) << "the reply offered no video";

   emit scrape.scraper().mediaReady( 0, redSquare() );
   EXPECT_EQ( scrape.pictureCount(), 1 );
   EXPECT_EQ( scrape.selectedPicture(), -1 );
   scrape.selectPicture( 5 );
   EXPECT_EQ( scrape.selectedPicture(), -1 );
   scrape.selectPicture( 0 );
   EXPECT_TRUE( scrape.savePicture() );
   EXPECT_TRUE( scrape.canSave() );
}

TEST( ScrapeController, SaveWritesTheEditedFieldsAndTheSelectedPicture )
{
   MobileFixture fixture;
   ScrapeController scrape( &fixture.library, &fixture.games );
   fixture.games.select( 0 );
   emit scrape.scraper().gameInfoReady( sampleInfo() );
   emit scrape.scraper().mediaReady( 0, redSquare() );
   scrape.selectPicture( 0 );
   scrape.setField( QStringLiteral( "genre" ), QStringLiteral( "Racing" ) );

   EXPECT_EQ( saveAndCount( scrape ), 1 );
   EXPECT_FALSE( scrape.isBusy() );
   EXPECT_TRUE( scrape.error().isEmpty() );

   const Gamelist again = reload( fixture );
   EXPECT_EQ( again.at( 0 ).name, QStringLiteral( "Sonic the Hedgehog 2" ) );
   EXPECT_EQ( again.at( 0 ).genre, QStringLiteral( "Racing" ) );
   EXPECT_EQ( QImage( again.at( 0 ).physicalImagePath ).size(), QSize( 4, 4 ) );
}

TEST( ScrapeController, UntickedPartsAreLeftAlone )
{
   MobileFixture fixture;
   ScrapeController scrape( &fixture.library, &fixture.games );
   fixture.games.select( 0 );
   emit scrape.scraper().gameInfoReady( sampleInfo() );
   emit scrape.scraper().mediaReady( 0, redSquare() );
   scrape.setSaveInfos( false );
   scrape.selectPicture( 0 );
   scrape.setSavePicture( false );
   EXPECT_FALSE( scrape.canSave() );

   EXPECT_EQ( saveAndCount( scrape ), 0 );
   EXPECT_EQ( reload( fixture ).at( 0 ).name, QStringLiteral( "Sonic The Hedgehog" ) );
}

TEST( ScrapeController, AFailedSaveKeepsTheResultAndReportsTheError )
{
   MobileFixture fixture;
   ScrapeController scrape( &fixture.library, &fixture.games );
   fixture.games.select( 0 );
   emit scrape.scraper().gameInfoReady( sampleInfo() );
   breakSave( fixture, *fixture.library.gamelist() );

   EXPECT_EQ( saveAndCount( scrape ), 0 );
   EXPECT_FALSE( scrape.error().isEmpty() );
   EXPECT_TRUE( scrape.hasResult() );
   EXPECT_FALSE( scrape.isBusy() );
   EXPECT_EQ( fixture.library.gamelist()->at( 0 ).name, QStringLiteral( "Sonic The Hedgehog" ) );
}

TEST( ScrapeController, AFailureReportsItsMessageAndAnotherGameStartsClean )
{
   MobileFixture fixture;
   ScrapeController scrape( &fixture.library, &fixture.games );
   fixture.games.select( 0 );
   emit scrape.scraper().gameInfoReady( sampleInfo() );
   emit scrape.scraper().gameInfoFailed( QStringLiteral( "Server busy" ) );
   EXPECT_EQ( scrape.error(), QStringLiteral( "Server busy" ) );
   EXPECT_FALSE( scrape.isBusy() );

   const int before = scrape.generation();
   fixture.games.select( 1 );
   EXPECT_FALSE( scrape.hasResult() );
   EXPECT_TRUE( scrape.error().isEmpty() );
   EXPECT_EQ( scrape.pictureCount(), 0 );
   EXPECT_NE( scrape.generation(), before );
}

TEST( ScrapeController, GameNotFoundGoesToANameSearch )
{
   if ( devCredentialsConfigured() )
      GTEST_SKIP() << "would query the real server";

   MobileFixture fixture;
   ScrapeController scrape( &fixture.library, &fixture.games );
   fixture.games.select( 0 );

   // Without credentials the search fails on the spot, which shows the search was attempted.
   emit scrape.scraper().gameInfoFailed( QString( Cst_GameNotFound ) );
   EXPECT_NE( scrape.error(), QString( Cst_GameNotFound ) );
   EXPECT_EQ( scrape.searchText(), QStringLiteral( "Sonic The Hedgehog" ) );
   EXPECT_FALSE( scrape.isPicking() );
}

TEST( ScrapeController, ASearchReplyNobodyAskedForIsIgnored )
{
   MobileFixture fixture;
   ScrapeController scrape( &fixture.library, &fixture.games );
   fixture.games.select( 0 );

   emit scrape.scraper().searchReady( { ScrapeCandidate{ QStringLiteral( "Sonic" ), {}, {} } } );
   EXPECT_FALSE( scrape.isPicking() );
   EXPECT_TRUE( scrape.candidates().isEmpty() );
}

TEST( ScrapeController, AManualCrcSkipsTheHashingAndAsksAtOnce )
{
   if ( devCredentialsConfigured() )
      GTEST_SKIP() << "would query the real server";

   MobileFixture fixture;
   ScrapeController scrape( &fixture.library, &fixture.games );
   fixture.games.select( 0 );
   scrape.setManualCrc( QStringLiteral( " DEADBEEF " ) );
   EXPECT_EQ( scrape.manualCrc(), QStringLiteral( "DEADBEEF" ) );

   scrape.scrape();
   EXPECT_FALSE( scrape.isBusy() ) << "the missing credentials fail the request immediately";
   EXPECT_FALSE( scrape.error().isEmpty() );
}

TEST( ScrapeController, MediaChoicesFeedTheRequestByKey )
{
   MobileFixture fixture;
   ScrapeController scrape( &fixture.library, &fixture.games );
   EXPECT_TRUE( scrape.media().value( QStringLiteral( "video" ) ).toBool() );

   scrape.setMedia( QStringLiteral( "wheel" ), false );
   scrape.setMedia( QStringLiteral( "nonsense" ), true );
   EXPECT_FALSE( scrape.media().value( QStringLiteral( "wheel" ) ).toBool() );
   EXPECT_FALSE( scrape.media().contains( QStringLiteral( "nonsense" ) ) );
}

}  // namespace
