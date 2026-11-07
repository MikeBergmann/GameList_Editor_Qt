#include "GamelistFixture.h"
#include "QtPrinters.h"
#include "RomCopy.h"

#include <gtest/gtest.h>

#include <QDateTime>
#include <QSet>

namespace {

struct Roms
{
   QTemporaryDir dir;

   QString src( const char* aRelative = "" ) const { return dir.filePath( QStringLiteral( "src/" ) + QLatin1String( aRelative ) ); }
   QString dst( const char* aRelative = "" ) const { return dir.filePath( QStringLiteral( "dst/" ) + QLatin1String( aRelative ) ); }

   // Sorted source file names relative to the source folder.
   static QStringList names( const RomCopyPlan& aPlan, const QString& aSourceDir )
   {
      QStringList names;
      for ( const RomFile& file : aPlan.files )
         names << QDir( aSourceDir ).relativeFilePath( file.source );
      names.sort();
      return names;
   }
};

TEST( RomCopy, ACueSheetBringsItsTracks )
{
   Roms roms;
   writeFile( roms.src( "Game.cue" ),
              "FILE \"Game (Track 1).bin\" BINARY\r\n  TRACK 01 MODE2/2352\r\nFILE Track2.bin BINARY\r\n" );
   writeFile( roms.src( "Game (Track 1).bin" ), "one" );
   writeFile( roms.src( "Track2.bin" ), "two" );
   writeFile( roms.src( "Unlisted.bin" ), "not part of it" );

   const RomCopyPlan plan = planRomCopy( roms.src(), roms.dst(), QStringLiteral( "./Game.cue" ) );

   EXPECT_EQ( Roms::names( plan, roms.src() ), QStringList( { "Game (Track 1).bin", "Game.cue", "Track2.bin" } ) );
   EXPECT_TRUE( plan.problems.isEmpty() );
   EXPECT_EQ( plan.bytes, 3 + 3 + QFileInfo( roms.src( "Game.cue" ) ).size() );

   ASSERT_TRUE( copyRomFiles( plan.files ) );
   EXPECT_EQ( QFileInfo( roms.dst( "Track2.bin" ) ).size(), 3 );
   EXPECT_TRUE( QFileInfo::exists( roms.dst( "Game (Track 1).bin" ) ) );
   EXPECT_FALSE( QFileInfo::exists( roms.dst( "Unlisted.bin" ) ) );
   EXPECT_EQ( QFileInfo( roms.dst( "Track2.bin" ) ).lastModified(), QFileInfo( roms.src( "Track2.bin" ) ).lastModified() )
      << "the source's time is kept";
   EXPECT_TRUE( planRomCopy( roms.src(), roms.dst(), QStringLiteral( "./Game.cue" ) ).files.isEmpty() )
      << "a second plan finds nothing to do";
}

TEST( RomCopy, AnM3uBringsItsDiscsAndTheirTracks )
{
   Roms roms;
   writeFile( roms.src( "Game.m3u" ), "#EXTM3U\nDisc1.cue\r\nsub\\Disc2.cue\n\n" );
   writeFile( roms.src( "Disc1.cue" ), "FILE \"Disc1.bin\" BINARY\n" );
   writeFile( roms.src( "Disc1.bin" ), "d1" );
   writeFile( roms.src( "sub/Disc2.cue" ), "FILE \"Disc2.bin\" BINARY\n" );
   writeFile( roms.src( "sub/Disc2.bin" ), "d2" );

   const RomCopyPlan plan = planRomCopy( roms.src(), roms.dst(), QStringLiteral( "./Game.m3u" ) );

   EXPECT_EQ( Roms::names( plan, roms.src() ),
              QStringList( { "Disc1.bin", "Disc1.cue", "Game.m3u", "sub/Disc2.bin", "sub/Disc2.cue" } ) );
   ASSERT_TRUE( copyRomFiles( plan.files ) );
   EXPECT_TRUE( QFileInfo::exists( roms.dst( "sub/Disc2.bin" ) ) );
}

TEST( RomCopy, AGdiBringsItsTracksAndAFolderRomIsAProblemNotACopy )
{
   Roms roms;
   writeFile( roms.src( "g.gdi" ), "2\n1 0 4 2352 track01.bin 0\n2 450 0 2352 \"track 02.raw\" 0\n" );
   writeFile( roms.src( "track01.bin" ), "a" );
   writeFile( roms.src( "track 02.raw" ), "b" );
   writeFile( roms.src( "dig.scummvm/data/dig.001" ), "c" );
   writeFile( roms.src( "dig.scummvm/dig.000" ), "d" );

   EXPECT_EQ( Roms::names( planRomCopy( roms.src(), roms.dst(), QStringLiteral( "./g.gdi" ) ), roms.src() ),
              QStringList( { "g.gdi", "track 02.raw", "track01.bin" } ) );
   const RomCopyPlan folder = planRomCopy( roms.src(), roms.dst(), QStringLiteral( "./dig.scummvm" ) );
   EXPECT_TRUE( folder.files.isEmpty() ) << "folder ROMs are not synced";
   EXPECT_EQ( folder.problems.size(), 1 );
}

TEST( RomCopy, OnlyAMissingOrDifferentSizedFileIsPlannedAndAMissingTrackIsAProblem )
{
   Roms roms;
   writeFile( roms.src( "Game.cue" ), "FILE \"a.bin\" BINARY\nFILE \"gone.bin\" BINARY\n" );
   writeFile( roms.src( "a.bin" ), "four" );
   writeFile( roms.dst( "Game.cue" ), QByteArray( QFileInfo( roms.src( "Game.cue" ) ).size(), 'x' ) );
   writeFile( roms.dst( "a.bin" ), "other size" );

   const RomCopyPlan plan = planRomCopy( roms.src(), roms.dst(), QStringLiteral( "./Game.cue" ) );

   EXPECT_EQ( Roms::names( plan, roms.src() ), QStringList( { "a.bin" } ) ) << "same size: left alone";
   ASSERT_EQ( plan.problems.size(), 1 );
   EXPECT_TRUE( plan.problems.first().contains( QStringLiteral( "gone.bin" ) ) );

   ASSERT_TRUE( copyRomFiles( plan.files ) );
   EXPECT_EQ( QFileInfo( roms.dst( "a.bin" ) ).size(), 4 ) << "the different file was replaced";
   EXPECT_FALSE( QFileInfo::exists( roms.dst( "a.bin.gle-bak" ) ) ) << "the replaced file's backup is dropped";
}

TEST( RomCopy, ATrackOutsideTheRomFolderIsAProblemNotACopy )
{
   Roms roms;
   writeFile( roms.src( "sys/Game.cue" ), "FILE \"../elsewhere.bin\" BINARY\n" );
   writeFile( roms.src( "elsewhere.bin" ), "x" );

   const RomCopyPlan plan = planRomCopy( roms.src( "sys" ), roms.dst( "sys" ), QStringLiteral( "./Game.cue" ) );

   EXPECT_EQ( Roms::names( plan, roms.src( "sys" ) ), QStringList( { "Game.cue" } ) );
   EXPECT_EQ( plan.problems.size(), 1 );
}

TEST( RomCopy, AFailedCopyLeavesNoHalfFileAndCancelStopsBeforeTheNextOne )
{
   Roms roms;
   writeFile( roms.src( "a.bin" ), "aaaa" );
   writeFile( roms.src( "b.bin" ), "bbbb" );
   // A non-empty folder where the file belongs: the staged copy cannot take its place.
   writeFile( roms.dst( "a.bin/inside" ), "x" );

   const RomCopyPlan plan = planRomCopy( roms.src(), roms.dst(), QStringLiteral( "./a.bin" ) );
   ASSERT_EQ( plan.files.size(), 1 );

   EXPECT_FALSE( copyRomFiles( plan.files ) );
   EXPECT_FALSE( QFileInfo::exists( roms.dst( "a.bin.gle-new" ) ) );
   EXPECT_TRUE( QFileInfo( roms.dst( "a.bin/inside" ) ).isFile() ) << "what stood there is untouched";

   int asked = 0;
   const Result<> cancelled = copyRomFiles(
      { { roms.src( "a.bin" ), roms.dst( "x/a.bin" ), 4 }, { roms.src( "b.bin" ), roms.dst( "x/b.bin" ), 4 } },
      [&asked]( const RomFile& ) { return ++asked < 2; } );
   EXPECT_FALSE( cancelled );
   EXPECT_TRUE( QFileInfo::exists( roms.dst( "x/a.bin" ) ) );
   EXPECT_FALSE( QFileInfo::exists( roms.dst( "x/b.bin" ) ) );
}

TEST( RomCopy, FreeSpaceRefusesWhatCannotFitAndLooksAtTheNearestExistingParent )
{
   Roms roms;
   EXPECT_TRUE( checkFreeSpace( roms.dst( "not/created/yet" ), 0 ) );
   EXPECT_FALSE( checkFreeSpace( roms.dst( "not/created/yet" ), std::numeric_limits<qint64>::max() ) );
}

}  // namespace
