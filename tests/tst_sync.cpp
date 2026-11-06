#include "GamelistFixture.h"
#include "QtPrinters.h"
#include "Gamelist.h"
#include "Sync.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QSettings>

namespace {

// <tmp>/master and <tmp>/target are legacy roots (a gamelist.xml per system folder);
// makeEsde turns one into an ES-DE app-data folder, with ROMs in <tmp>/ROMs.
struct Roots
{
   QTemporaryDir dir;

   QString master( const char* aRelative = "" ) const { return dir.filePath( QStringLiteral( "master/" ) + QLatin1String( aRelative ) ); }
   QString target( const char* aRelative = "" ) const { return dir.filePath( QStringLiteral( "target/" ) + QLatin1String( aRelative ) ); }
   QString at( const char* aRelative ) const { return dir.filePath( QLatin1String( aRelative ) ); }

   void makeEsde( const QString& aRoot ) const
   {
      writeFile( aRoot + QStringLiteral( "/settings/es_settings.xml" ),
                 "<?xml version=\"1.0\"?>\n<string name=\"MediaDirectory\" value=\"\" />\n" );
      EXPECT_TRUE( QDir().mkpath( aRoot + QStringLiteral( "/gamelists" ) ) );
   }

   SyncPlan plan() const { return planSync( scanSystems( master() ), target() ); }
};

QByteArray list( const QByteArray& aGames )
{
   return "<?xml version=\"1.0\"?>\n<gameList>" + aGames + "</gameList>\n";
}

QByteArray game( const char* aPath, const char* aName, const QByteArray& aExtra = {} )
{
   return QByteArray( "<game><path>./" ) + aPath + "</path><name>" + aName + "</name>" + aExtra + "</game>";
}

// A system with a gamelist and the ROM files it names; both roots are legacy.
void putSystem( const QString& aRoot, const char* aFolder, const QByteArray& aGames, const QStringList& aRoms )
{
   writeFile( aRoot + QLatin1Char( '/' ) + QLatin1String( aFolder ) + QStringLiteral( "/gamelist.xml" ), list( aGames ) );
   for ( const QString& rom : aRoms )
      writeFile( aRoot + QLatin1Char( '/' ) + QLatin1String( aFolder ) + QLatin1Char( '/' ) + rom, "rom" );
}

struct CleanMap : testing::Test
{
   void TearDown() override { QSettings().remove( QStringLiteral( "SystemMap" ) ); }
};

TEST( SyncPlan, ANewGameIsPlannedWithItsRomAndEverySyncedField )
{
   Roots roots;
   putSystem( roots.master(), "snes",
              game( "Sonic.zip", "Sonic", "<genre>Platform</genre><favorite>true</favorite>" ) + game( "Mario.zip", "Mario" ),
              { "Sonic.zip", "Mario.zip" } );
   putSystem( roots.target(), "snes", game( "Sonic.zip", "Sonic", "<genre>Platform</genre>" ), { "Sonic.zip" } );

   const SyncPlan plan = roots.plan();

   ASSERT_EQ( plan.systems.size(), 1 );
   const SystemPlan& system = plan.systems.first();
   EXPECT_FALSE( plan.isEmpty() );
   EXPECT_TRUE( system.copyRoms );
   EXPECT_FALSE( system.createTarget );
   ASSERT_EQ( system.games.size(), 1 );
   const SyncGame& added = system.games.first();
   EXPECT_EQ( added.targetIndex, -1 );
   EXPECT_EQ( added.romLink, QStringLiteral( "./Mario.zip" ) );
   EXPECT_EQ( added.fields, GameFieldSet( gfName ) ) << "only what the master has filled in";
   ASSERT_EQ( added.rom.files.size(), 1 );
   EXPECT_EQ( added.rom.files.first().target, roots.target( "snes/Mario.zip" ) );
   EXPECT_EQ( system.unchanged, 1 );
   EXPECT_EQ( system.romBytes, 3 );
}

TEST( SyncPlan, OnlyTheSyncedFieldsCountAndPerDeviceFlagsAreIgnored )
{
   Roots roots;
   putSystem( roots.master(), "snes",
              game( "Sonic.zip", "Sonic",
                    "<genre>Platform</genre><favorite>true</favorite><hidden>true</hidden><playcount>9</playcount>"
                    "<lastplayed>20240101T000000</lastplayed><sortname>A</sortname>" ),
              { "Sonic.zip" } );
   putSystem( roots.target(), "snes",
              game( "Sonic.zip", "Sonic",
                    "<genre>Action</genre><favorite>false</favorite><playcount>1</playcount><sortname>B</sortname>" ),
              { "Sonic.zip" } );

   const SyncPlan plan = roots.plan();

   ASSERT_EQ( plan.systems.size(), 1 );
   ASSERT_EQ( plan.systems.first().games.size(), 1 );
   const SyncGame& changed = plan.systems.first().games.first();
   EXPECT_EQ( changed.targetIndex, 0 );
   EXPECT_EQ( changed.fields, GameFieldSet( gfGenre ) );
}

TEST( SyncPlan, AFlagOnlyDifferenceIsNothingToDo )
{
   Roots roots;
   putSystem( roots.master(), "snes", game( "Sonic.zip", "Sonic", "<favorite>true</favorite>" ), { "Sonic.zip" } );
   putSystem( roots.target(), "snes", game( "Sonic.zip", "Sonic" ), { "Sonic.zip" } );

   EXPECT_TRUE( roots.plan().isEmpty() );
}

TEST( SyncPlan, ACoverIsCopiedWhenMissingOrDifferentAndSkippedWhenEqual )
{
   Roots roots;
   const QByteArray image = "<image>./media/images/Sonic.png</image>";
   putSystem( roots.master(), "snes", game( "Sonic.zip", "Sonic", image ), { "Sonic.zip" } );
   putSystem( roots.target(), "snes", game( "Sonic.zip", "Sonic", image ), { "Sonic.zip" } );
   writeFile( roots.master( "snes/media/images/Sonic.png" ), "AAAA" );

   const auto copies = [&] {
      const SyncPlan plan = roots.plan();
      return plan.isEmpty() ? QString() : plan.systems.first().games.first().imageSource;
   };

   EXPECT_EQ( copies(), roots.master( "snes/media/images/Sonic.png" ) ) << "missing on the target";

   writeFile( roots.target( "snes/media/images/Sonic.png" ), "BBBB" );
   EXPECT_FALSE( copies().isEmpty() ) << "same size, other MD5";

   writeFile( roots.target( "snes/media/images/Sonic.png" ), "AA" );
   EXPECT_FALSE( copies().isEmpty() ) << "other size";

   writeFile( roots.target( "snes/media/images/Sonic.png" ), "AAAA" );
   EXPECT_TRUE( roots.plan().isEmpty() ) << "equal bytes: skipped";
}

TEST( SyncPlan, ATargetOnlyGameIsListedAndNeverAnAction )
{
   Roots roots;
   putSystem( roots.master(), "snes", game( "Sonic.zip", "Sonic" ), { "Sonic.zip" } );
   putSystem( roots.target(), "snes", game( "Sonic.zip", "Sonic" ) + game( "Zelda.zip", "Zelda" ),
              { "Sonic.zip", "Zelda.zip" } );

   const SyncPlan plan = roots.plan();

   ASSERT_EQ( plan.systems.size(), 1 );
   EXPECT_EQ( plan.systems.first().targetOnly, QStringList( { "Zelda" } ) );
   EXPECT_TRUE( plan.isEmpty() );
}

TEST( SyncPlan, ASystemWithoutAPartnerIsPlannedForCreation )
{
   Roots roots;
   putSystem( roots.master(), "gb", game( "Tetris.gb", "Tetris" ), { "Tetris.gb" } );
   ASSERT_TRUE( QDir().mkpath( roots.target() ) );

   const SyncPlan plan = roots.plan();

   ASSERT_EQ( plan.systems.size(), 1 );
   const SystemPlan& system = plan.systems.first();
   EXPECT_TRUE( system.createTarget );
   EXPECT_FALSE( system.pair.targetExists );
   EXPECT_EQ( system.pair.target.gamelistPath, roots.target( "gb/gamelist.xml" ) );
   ASSERT_EQ( system.games.size(), 1 );
   EXPECT_EQ( system.games.first().targetIndex, -1 );
}

TEST_F( CleanMap, SystemsPairByFolderNameThenByKindAndNeverTwice )
{
   Roots roots;
   setSystemMapping( QStringLiteral( "amiga500" ), skAM );
   putSystem( roots.master(), "amiga", game( "A.adf", "A" ), { "A.adf" } );
   putSystem( roots.master(), "snes", game( "S.zip", "S" ), { "S.zip" } );
   putSystem( roots.master(), "megadrive", game( "M.zip", "M" ), { "M.zip" } );
   putSystem( roots.target(), "amiga500", game( "A.adf", "A" ), { "A.adf" } );
   putSystem( roots.target(), "snes", game( "S.zip", "S" ), { "S.zip" } );
   putSystem( roots.target(), "genesis", game( "M.zip", "M" ), { "M.zip" } );

   const QVector<SystemPair> pairs = pairSystems( scanSystems( roots.master() ), roots.target() );

   ASSERT_EQ( pairs.size(), 3 );
   EXPECT_EQ( pairs[0].source.folderName, QStringLiteral( "amiga" ) );
   EXPECT_EQ( pairs[0].target.folderName, QStringLiteral( "amiga500" ) ) << "same kind, other folder name";
   EXPECT_TRUE( pairs[0].targetExists );
   EXPECT_EQ( pairs[1].source.folderName, QStringLiteral( "megadrive" ) );
   EXPECT_FALSE( pairs[1].targetExists ) << "megadrive and genesis are not paired";
   EXPECT_EQ( pairs[1].target.folderName, QStringLiteral( "megadrive" ) );
   EXPECT_EQ( pairs[2].target.folderName, QStringLiteral( "snes" ) );
}

TEST( SyncPlan, AbsolutePathsAndDuplicatesOnTheMasterAreProblemsNotGuesses )
{
   Roots roots;
   putSystem( roots.master(), "snes",
              game( "Sonic.zip", "Sonic" ) + game( "Sonic.zip", "Sonic again" ) +
                 "<game><path>/home/pi/RetroPie/roms/snes/Abs.zip</path><name>Abs</name></game>",
              { "Sonic.zip" } );
   putSystem( roots.target(), "snes", game( "Other.zip", "Other" ), { "Other.zip" } );

   const SyncPlan plan = roots.plan();

   ASSERT_EQ( plan.systems.size(), 1 );
   const SystemPlan& system = plan.systems.first();
   EXPECT_EQ( system.games.size(), 1 ) << "the first Sonic only";
   EXPECT_EQ( system.problems.size(), 2 );
}

TEST( SyncPlan, ANewGameWhoseRomIsMissingOnTheMasterIsAProblem )
{
   Roots roots;
   putSystem( roots.master(), "snes", game( "Gone.zip", "Gone" ), {} );
   putSystem( roots.target(), "snes", game( "Other.zip", "Other" ), { "Other.zip" } );

   const SyncPlan plan = roots.plan();

   ASSERT_EQ( plan.systems.size(), 1 );
   EXPECT_TRUE( plan.isEmpty() );
   EXPECT_EQ( plan.systems.first().problems.size(), 1 );
}

TEST( SyncPlan, TheSameRomFolderCopiesNoRoms )
{
   Roots roots;
   putSystem( roots.master(), "snes", game( "Sonic.zip", "Sonic" ), { "Sonic.zip" } );
   putSystem( roots.target(), "snes", game( "Other.zip", "Other" ), {} );
   // The target's ROM folder is the master's: a ES-DE target pointing at the master's snes folder.
   roots.makeEsde( roots.target( "esde" ) );
   writeFile( roots.target( "esde/gamelists/snes/gamelist.xml" ), list( game( "Other.zip", "Other" ) ) );
   writeFile( roots.target( "esde/settings/es_settings.xml" ),
              "<?xml version=\"1.0\"?>\n<string name=\"ROMDirectory\" value=\"" + roots.master().toUtf8() + "\" />\n" );

   const SyncPlan plan = planSync( scanSystems( roots.master() ), roots.target( "esde" ) );

   ASSERT_EQ( plan.systems.size(), 1 );
   const SystemPlan& system = plan.systems.first();
   EXPECT_FALSE( system.copyRoms );
   ASSERT_EQ( system.games.size(), 1 );
   EXPECT_TRUE( system.games.first().rom.files.isEmpty() );
}

TEST( SyncPlan, LegacyToEsdeKeepsAMatchingCoverAndPlansOnlyWhatEsdeCanRead )
{
   Roots roots;
   roots.makeEsde( roots.target() );
   putSystem( roots.master(), "snes",
              game( "Sonic.zip", "Sonic", "<image>./a.jpeg</image><video>./v.flv</video>" ) +
                 game( "Mario.zip", "Mario", "<image>./b.jpg</image><video>./w.mkv</video>" ),
              { "Sonic.zip", "Mario.zip" } );
   writeFile( roots.master( "snes/a.jpeg" ), "picture a" );
   writeFile( roots.master( "snes/v.flv" ), "video v" );
   writeFile( roots.master( "snes/b.jpg" ), "picture b" );
   writeFile( roots.master( "snes/w.mkv" ), "video w" );
   writeFile( roots.target( "gamelists/snes/gamelist.xml" ), list( game( "Sonic.zip", "Sonic" ) + game( "Mario.zip", "Mario" ) ) );
   writeFile( roots.at( "ROMs/snes/Sonic.zip" ), "rom" );
   writeFile( roots.at( "ROMs/snes/Mario.zip" ), "rom" );
   writeFile( roots.target( "downloaded_media/snes/covers/Sonic.png" ), "converted earlier" );

   const SyncPlan plan = roots.plan();

   ASSERT_EQ( plan.systems.size(), 1 );
   const SystemPlan& system = plan.systems.first();
   ASSERT_EQ( system.games.size(), 1 ) << "Sonic: converted cover present, flv unplayable: nothing to do";
   const SyncGame& mario = system.games.first();
   EXPECT_EQ( mario.name, QStringLiteral( "Mario" ) );
   EXPECT_TRUE( mario.copyImage );
   EXPECT_TRUE( mario.copyVideo );
   EXPECT_EQ( system.problems.size(), 1 );
   EXPECT_TRUE( system.problems.first().contains( QStringLiteral( "flv" ) ) );
}

TEST( SyncPlan, EsdeToLegacyFindsTheMastersMediaByRomName )
{
   Roots roots;
   roots.makeEsde( roots.master() );
   writeFile( roots.master( "gamelists/snes/gamelist.xml" ), list( game( "Sonic.zip", "Sonic", "<genre>Platform</genre>" ) ) );
   writeFile( roots.at( "ROMs/snes/Sonic.zip" ), "rom" );
   writeFile( roots.master( "downloaded_media/snes/covers/Sonic.jpg" ), "jpg bytes" );
   ASSERT_TRUE( QDir().mkpath( roots.target() ) );

   const SyncPlan plan = roots.plan();

   ASSERT_EQ( plan.systems.size(), 1 );
   const SystemPlan& system = plan.systems.first();
   EXPECT_TRUE( system.createTarget );
   ASSERT_EQ( system.games.size(), 1 );
   EXPECT_EQ( system.games.first().imageSource, roots.master( "downloaded_media/snes/covers/Sonic.jpg" ) );
   EXPECT_FALSE( system.games.first().copyVideo );
   EXPECT_EQ( system.games.first().fields, GameFieldSet( gfName | gfGenre ) );
}

TEST( SyncPlan, ProgressNamesTheGameBeingComparedAndCancelingLeavesNoPlan )
{
   Roots roots;
   putSystem( roots.master(), "snes", game( "Sonic.zip", "Sonic" ) + game( "Mario.zip", "Mario" ), { "Sonic.zip", "Mario.zip" } );
   putSystem( roots.target(), "snes", game( "Other.zip", "Other" ), { "Other.zip" } );

   QStringList messages;
   int total = 0;
   const SyncPlan full = planSync( scanSystems( roots.master() ), roots.target(), [&]( const QString& aMessage, int, int aTotal ) {
      messages << aMessage;
      total = aTotal;
      return true;
   } );
   EXPECT_FALSE( full.canceled );
   EXPECT_EQ( total, 1 );
   EXPECT_TRUE( messages.contains( QStringLiteral( "snes: comparing game 2 of 2, Mario" ) ) ) << messages.join( '|' ).toStdString();

   const SyncPlan stopped = planSync( scanSystems( roots.master() ), roots.target(), []( const QString& aMessage, int, int ) {
      return !aMessage.contains( QStringLiteral( "comparing" ) );
   } );
   EXPECT_TRUE( stopped.canceled );
   EXPECT_TRUE( stopped.systems.isEmpty() );
}

TEST( SyncPlan, TheMastersOwnFolderAsTargetIsAProblemNotAPlan )
{
   Roots roots;
   putSystem( roots.master(), "snes", game( "Sonic.zip", "Sonic" ), { "Sonic.zip" } );

   const SyncPlan plan = planSync( scanSystems( roots.master() ), roots.master() );

   EXPECT_TRUE( plan.isEmpty() );
   ASSERT_EQ( plan.systems.size(), 1 );
   EXPECT_EQ( plan.systems.first().problems.size(), 1 );
}

TEST( SyncPlan, AGameMatchesWhateverTheCaseWhenTheTargetVolumeFoldsIt )
{
   if ( !filesystemIsCaseInsensitive() )
      GTEST_SKIP() << "filesystem keeps case";

   Roots roots;
   putSystem( roots.master(), "snes", game( "sonic.zip", "Sonic" ), { "sonic.zip" } );
   putSystem( roots.target(), "snes", game( "SONIC.ZIP", "Sonic" ), { "SONIC.ZIP" } );

   EXPECT_TRUE( roots.plan().isEmpty() ) << "no duplicate game for a case difference";
}

QByteArray fileText( const QString& aPath )
{
   QFile file( aPath );
   return file.open( QIODevice::ReadOnly ) ? file.readAll() : QByteArray();
}

Gamelist loadTarget( const QString& aRoot, const char* aFolder )
{
   Gamelist list;
   const Result<> loaded = list.load( systemAt( aRoot, QLatin1String( aFolder ) ), true );
   EXPECT_TRUE( loaded ) << loaded.error().toStdString();
   return list;
}

TEST( SyncApply, LegacyToLegacyAddsGamesFieldsRomsAndMediaThenHasNothingLeftToDo )
{
   Roots roots;
   putSystem( roots.master(), "snes",
              game( "Sonic.zip", "Sonic", "<genre>Platform</genre><image>./Sonic.png</image>" ) +
                 game( "Mario.zip", "Mario", "<desc>Jump</desc><video>./Mario.mp4</video>" ),
              { "Sonic.zip", "Mario.zip" } );
   writeFile( roots.master( "snes/Sonic.png" ), "AAAA" );
   writeFile( roots.master( "snes/Mario.mp4" ), "video" );
   putSystem( roots.target(), "snes", game( "Sonic.zip", "Sonic", "<favorite>true</favorite>" ), { "Sonic.zip" } );

   const SyncReport report = applySync( roots.plan() );

   ASSERT_EQ( report.systems.size(), 1 );
   const SystemReport& system = report.systems.first();
   EXPECT_TRUE( system.ok() ) << system.error.toStdString();
   EXPECT_EQ( system.added, 1 );
   EXPECT_EQ( system.updated, 1 );
   EXPECT_EQ( system.mediaCopied, 2 );

   const Gamelist target = loadTarget( roots.target(), "snes" );
   ASSERT_EQ( target.count(), 2 );
   EXPECT_EQ( target.at( 0 ).genre, QStringLiteral( "Platform" ) );
   EXPECT_TRUE( target.at( 0 ).favorite ) << "a per-device flag is left alone";
   EXPECT_FALSE( target.at( 0 ).missingImage );
   EXPECT_EQ( target.at( 1 ).name, QStringLiteral( "Mario" ) );
   EXPECT_EQ( target.at( 1 ).description, QStringLiteral( "Jump" ) );
   EXPECT_FALSE( target.at( 1 ).missingVideo );
   EXPECT_EQ( fileText( roots.target( "snes/Mario.zip" ) ), QByteArray( "rom" ) );

   EXPECT_TRUE( roots.plan().isEmpty() ) << "a second run has nothing to do";
}

TEST( SyncApply, LegacyToEsdeConvertsWhatEsdeCannotReadAndIsThenQuiet )
{
   Roots roots;
   roots.makeEsde( roots.target() );
   putSystem( roots.master(), "snes", game( "Mario.zip", "Mario", "<image>./m.jpeg</image><video>./m.mkv</video>" ),
              { "Mario.zip" } );
   QImage picture( 4, 4, QImage::Format_RGB32 );
   picture.fill( Qt::red );
   ASSERT_TRUE( picture.save( roots.master( "snes/m.jpeg" ), "JPEG" ) );
   writeFile( roots.master( "snes/m.mkv" ), "video" );

   const SyncReport report = applySync( roots.plan() );

   ASSERT_EQ( report.systems.size(), 1 );
   EXPECT_TRUE( report.systems.first().ok() ) << report.systems.first().error.toStdString();
   EXPECT_TRUE( QFileInfo::exists( roots.at( "ROMs/snes/Mario.zip" ) ) );
   EXPECT_TRUE( QFileInfo::exists( roots.target( "downloaded_media/snes/covers/Mario.png" ) ) ) << "jpeg becomes PNG";
   EXPECT_TRUE( QFileInfo::exists( roots.target( "downloaded_media/snes/videos/Mario.mkv" ) ) );

   EXPECT_TRUE( roots.plan().isEmpty() ) << "a converted cover is not planned again";
}

TEST( SyncApply, EsdeToLegacyCreatesTheTargetSystem )
{
   Roots roots;
   roots.makeEsde( roots.master() );
   writeFile( roots.master( "gamelists/snes/gamelist.xml" ), list( game( "Sonic.zip", "Sonic", "<genre>Platform</genre>" ) ) );
   writeFile( roots.at( "ROMs/snes/Sonic.zip" ), "rom" );
   writeFile( roots.master( "downloaded_media/snes/covers/Sonic.jpg" ), "jpg bytes" );
   ASSERT_TRUE( QDir().mkpath( roots.target() ) );

   const SyncReport report = applySync( roots.plan() );

   ASSERT_EQ( report.systems.size(), 1 );
   EXPECT_TRUE( report.systems.first().ok() ) << report.systems.first().error.toStdString();
   const Gamelist target = loadTarget( roots.target(), "snes" );
   ASSERT_EQ( target.count(), 1 );
   EXPECT_EQ( target.at( 0 ).genre, QStringLiteral( "Platform" ) );
   EXPECT_FALSE( target.at( 0 ).missingImage );
   EXPECT_EQ( fileText( roots.target( "snes/Sonic.zip" ) ), QByteArray( "rom" ) );

   EXPECT_TRUE( roots.plan().isEmpty() );
}

TEST( SyncApply, CancelingBeforeAFileStopsTheRunAndLeavesTheTargetAlone )
{
   Roots roots;
   putSystem( roots.master(), "snes", game( "Mario.zip", "Mario" ), { "Mario.zip" } );
   putSystem( roots.target(), "snes", game( "Sonic.zip", "Sonic" ), { "Sonic.zip" } );

   const SyncReport report = applySync( roots.plan(), []( const QString& ) { return false; } );

   EXPECT_TRUE( report.canceled );
   ASSERT_EQ( report.systems.size(), 1 );
   EXPECT_FALSE( report.systems.first().ok() );
   EXPECT_FALSE( QFileInfo::exists( roots.target( "snes/Mario.zip" ) ) );
   EXPECT_EQ( loadTarget( roots.target(), "snes" ).count(), 1 );
}

TEST( SyncApply, ATargetChangedSinceThePlanIsReportedAndNotGuessedAt )
{
   Roots roots;
   putSystem( roots.master(), "snes", game( "Sonic.zip", "Sonic", "<genre>Platform</genre>" ), { "Sonic.zip" } );
   putSystem( roots.target(), "snes", game( "Sonic.zip", "Sonic" ), { "Sonic.zip" } );
   const SyncPlan plan = roots.plan();
   putSystem( roots.target(), "snes", game( "Other.zip", "Other" ) + game( "Sonic.zip", "Sonic" ), { "Other.zip" } );

   const SyncReport report = applySync( plan );

   ASSERT_EQ( report.systems.size(), 1 );
   EXPECT_FALSE( report.systems.first().ok() );
   EXPECT_TRUE( loadTarget( roots.target(), "snes" ).at( 1 ).genre.isEmpty() );
}

}  // namespace
