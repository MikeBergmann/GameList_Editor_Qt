#include "GamelistFixture.h"
#include "QtPrinters.h"
#include "Systems.h"

#include <gtest/gtest.h>

#include <QSet>
#include <QSettings>

namespace {

// QSettings is shared by the whole test run, so each test leaves the map empty.
struct CleanMap : testing::Test
{
   void TearDown() override { QSettings().remove( QStringLiteral( "SystemMap" ) ); }
};

using SystemMap = CleanMap;

// <tmp>/ES-DE is the app-data folder; gamelists exist for snes only, gb has none.
struct EsdeFolder
{
   QTemporaryDir dir;
   QString appData = dir.filePath( QStringLiteral( "ES-DE" ) );

   explicit EsdeFolder( const QByteArray& aSettings )
   {
      writeFile( appData + QStringLiteral( "/settings/es_settings.xml" ),
                 "<?xml version=\"1.0\"?>\n" + aSettings );
      writeFile( appData + QStringLiteral( "/gamelists/snes/gamelist.xml" ), Cst_Gamelist );
      EXPECT_TRUE( QDir().mkpath( appData + QStringLiteral( "/gamelists/gb" ) ) );
   }
};

TEST( RomDirOf, IsTheGamelistFolderUnlessTheSystemNamesAnotherRomFolder )
{
   QTemporaryDir dir;
   const QString root = QDir( dir.path() ).absolutePath();

   EXPECT_EQ( romDirOf( SystemEntry( QStringLiteral( "snes" ), skOther, root + QStringLiteral( "/snes/gamelist.xml" ) ) ),
              root + QStringLiteral( "/snes" ) );
   EXPECT_EQ( romDirOf( SystemEntry( QStringLiteral( "snes" ), skOther, root + QStringLiteral( "/gl/snes/gamelist.xml" ),
                                     root + QStringLiteral( "/roms/../roms/snes" ) ) ),
              root + QStringLiteral( "/roms/snes" ) );
}

TEST( ScanEsde, SettingsWithSeveralRootsGiveTheRomFolderAndSkipSystemsWithoutGamelist )
{
   EsdeFolder esde( "<bool name=\"DebugMode\" value=\"false\" />\n"
                    "<string name=\"ROMDirectory\" value=\"%ESPATH%/games\" />\n" );
   ASSERT_TRUE( QDir().mkpath( esde.dir.filePath( QStringLiteral( "games" ) ) ) );

   const QVector<SystemEntry> systems = scanSystems( esde.appData );

   ASSERT_EQ( systems.size(), 1 );
   EXPECT_EQ( systems.at( 0 ).folderName, QStringLiteral( "snes" ) );
   EXPECT_EQ( systems.at( 0 ).gamelistPath, esde.appData + QStringLiteral( "/gamelists/snes/gamelist.xml" ) );
   EXPECT_EQ( systems.at( 0 ).romDir, esde.dir.filePath( QStringLiteral( "games/snes" ) ) );
}

TEST( ScanEsde, WithoutARomDirectoryKeyTheRomsSitBesideTheAppDataFolder )
{
   EsdeFolder esde( "<string name=\"MediaDirectory\" value=\"\" />\n" );

   const QVector<SystemEntry> systems = scanSystems( esde.appData );

   ASSERT_EQ( systems.size(), 1 );
   EXPECT_EQ( systems.at( 0 ).romDir, esde.dir.filePath( QStringLiteral( "ROMs/snes" ) ) );
}

TEST( ScanEsde, ARomDirectoryThatIsGoneFallsBackToTheFolderBesideTheAppData )
{
   EsdeFolder esde( "<string name=\"ROMDirectory\" value=\"/nonexistent/old-card/ROMs\" />\n" );
   ASSERT_TRUE( QDir().mkpath( esde.dir.filePath( QStringLiteral( "ROMs" ) ) ) );

   const QVector<SystemEntry> systems = scanSystems( esde.appData );

   ASSERT_EQ( systems.size(), 1 );
   EXPECT_EQ( systems.at( 0 ).romDir, esde.dir.filePath( QStringLiteral( "ROMs/snes" ) ) );
}

TEST( ScanEsde, MediaIsInDownloadedMediaUnlessTheSettingsNameAnotherFolder )
{
   EsdeFolder plain( "<string name=\"MediaDirectory\" value=\"\" />\n" );
   EXPECT_EQ( scanSystems( plain.appData ).at( 0 ).mediaDir,
              plain.appData + QStringLiteral( "/downloaded_media/snes" ) );

   EsdeFolder moved( "<string name=\"MediaDirectory\" value=\"%ESPATH%/art\" />\n" );
   EXPECT_EQ( scanSystems( moved.appData ).at( 0 ).mediaDir, moved.dir.filePath( QStringLiteral( "art/snes" ) ) );
}

TEST( ScanEsde, ALegacyRootHasNoSeparateRomOrMediaDir )
{
   Fixture fixture;

   for ( const SystemEntry& system : scanSystems( fixture.dir.path() ) )
      EXPECT_TRUE( system.romDir.isEmpty() && system.mediaDir.isEmpty() );
}

QVector<SystemEntry> unknownFolder( const QString& aFolder )
{
   return { { aFolder, skOther, QString() } };
}

TEST( SystemAt, GivesTheEntryAnEsdeRootWouldHaveForAFolderWithNoGamelistYet )
{
   EsdeFolder esde( "<string name=\"MediaDirectory\" value=\"\" />\n" );

   const SystemEntry entry = systemAt( esde.appData, QStringLiteral( "gb" ) );

   EXPECT_EQ( entry.folderName, QStringLiteral( "gb" ) );
   EXPECT_EQ( entry.kind, systemKindFromFolder( QStringLiteral( "gb" ) ) );
   EXPECT_EQ( entry.gamelistPath, esde.appData + QStringLiteral( "/gamelists/gb/gamelist.xml" ) );
   EXPECT_EQ( entry.romDir, esde.dir.filePath( QStringLiteral( "ROMs/gb" ) ) );
   EXPECT_EQ( entry.mediaDir, esde.appData + QStringLiteral( "/downloaded_media/gb" ) );
}

TEST( SystemAt, GivesTheEntryALegacyRootWouldHaveAndMatchesTheScan )
{
   Fixture fixture;

   const SystemEntry fresh = systemAt( fixture.dir.path(), QStringLiteral( "gb" ) );
   EXPECT_EQ( fresh.gamelistPath, fixture.dir.filePath( QStringLiteral( "gb/gamelist.xml" ) ) );
   EXPECT_TRUE( fresh.romDir.isEmpty() && fresh.mediaDir.isEmpty() );

   const SystemEntry known = systemAt( fixture.dir.path(), QStringLiteral( "snes" ) );
   EXPECT_EQ( known.gamelistPath, fixture.snes.gamelistPath );
}

TEST( CreateGamelist, WritesAnEmptyListThatLoadsOnlyWhenAskedAndTakesGames )
{
   Fixture fixture;
   const SystemEntry entry = systemAt( fixture.dir.path(), QStringLiteral( "gb" ) );

   ASSERT_TRUE( createGamelist( entry.gamelistPath ) );
   EXPECT_FALSE( createGamelist( entry.gamelistPath ) ) << "an existing file must not be overwritten";

   Gamelist list;
   EXPECT_FALSE( list.load( entry ) ) << "the editor keeps refusing an empty list";
   ASSERT_TRUE( list.load( entry, true ) );
   EXPECT_EQ( list.count(), 0 );

   writeFile( fixture.dir.filePath( QStringLiteral( "gb/Tetris.gb" ) ), "rom" );
   const Result<int> added = list.addGames( { fixture.dir.filePath( QStringLiteral( "gb/Tetris.gb" ) ) } );
   ASSERT_TRUE( added );
   EXPECT_EQ( added.value(), 1 );

   Gamelist again;
   ASSERT_TRUE( again.load( entry ) );
   EXPECT_EQ( again.count(), 1 );
}

TEST_F( SystemMap, AMappedFolderBecomesTheKnownSystemAndAnUnmappedOneStaysOther )
{
   setSystemMapping( QStringLiteral( "amiga500" ), skAM );

   QVector<SystemEntry> systems = unknownFolder( QStringLiteral( "amiga500" ) );
   systems.append( { QStringLiteral( "other" ), skOther, QString() } );
   applySystemMap( systems );

   EXPECT_EQ( systems[0].kind, skAM );
   EXPECT_EQ( systems[1].kind, skOther );
}

TEST_F( SystemMap, ASystemTheTablesKnowIsNeverRemapped )
{
   setSystemMapping( QStringLiteral( "snes" ), skAM );

   QVector<SystemEntry> systems{ { QStringLiteral( "snes" ), systemKindFromFolder( QStringLiteral( "snes" ) ),
                                   QString() } };
   const SystemKind before = systems[0].kind;
   applySystemMap( systems );

   EXPECT_EQ( systems[0].kind, before );
}

TEST_F( SystemMap, MappingToOtherForgetsTheMapping )
{
   setSystemMapping( QStringLiteral( "amiga500" ), skAM );
   setSystemMapping( QStringLiteral( "amiga500" ), skOther );

   QVector<SystemEntry> systems = unknownFolder( QStringLiteral( "amiga500" ) );
   applySystemMap( systems );

   EXPECT_EQ( systems[0].kind, skOther );
}

TEST( SystemKindChoices, OffersEverySystemOnceWithAUniqueLabelInOrder )
{
   const QVector<SystemKindChoice> choices = systemKindChoices();

   // Everything but the catch-all, which is "no mapping".
   ASSERT_EQ( choices.size(), static_cast<int>( skOther ) );

   QSet<QString> labels;
   QSet<int> kinds;
   for ( int i = 0; i < choices.size(); ++i ) {
      labels.insert( choices[i].label );
      kinds.insert( choices[i].kind );
      if ( i > 0 ) {
         EXPECT_LE( choices[i - 1].label.compare( choices[i].label, Qt::CaseInsensitive ), 0 );
      }
   }
   EXPECT_EQ( labels.size(), choices.size() );  // the table names repeat; the labels do not
   EXPECT_EQ( kinds.size(), choices.size() );
   EXPECT_FALSE( kinds.contains( skOther ) );
}

TEST( SystemDisplayName, ShowsTheTableNameAndTheFolderOnlyWhenItDiffers )
{
   EXPECT_EQ( systemDisplayName( { QStringLiteral( "mystery" ), skOther, QString() }, false ),
              QStringLiteral( "mystery" ) );

   const QString plain = systemDisplayName( { QLatin1String( systemInfo( skAM ).folder ), skAM, QString() }, false );
   EXPECT_EQ( plain, QLatin1String( systemInfo( skAM ).name ) );
   EXPECT_EQ( systemDisplayName( { QStringLiteral( "amiga500" ), skAM, QString() }, false ),
              plain + QStringLiteral( " (amiga500)" ) );
}

TEST( SystemDisplayName, GenesisRenamesOnlyTheMegaDrive )
{
   const SystemEntry megaDrive{ QLatin1String( systemInfo( skMegaDrive ).folder ), skMegaDrive, QString() };
   EXPECT_EQ( systemDisplayName( megaDrive, false ), QLatin1String( systemInfo( skMegaDrive ).name ) );
   EXPECT_EQ( systemDisplayName( megaDrive, true ), QLatin1String( systemInfo( skGenesis ).name ) );
}

}  // namespace
