#include "GamelistFixture.h"
#include "QtPrinters.h"
#include "RomFiles.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>

namespace {

QStringList relativeTo( const QString& aDir, const QStringList& aPaths )
{
   QStringList names;
   for ( const QString& path : aPaths )
      names << QDir( aDir ).relativeFilePath( path );
   return names;
}

}  // namespace

TEST( RomFiles, APlainRomIsItsOwnOnlyPart )
{
   QTemporaryDir dir;
   const QString rom = dir.filePath( QStringLiteral( "Game.zip" ) );
   writeFile( rom, "rom" );

   EXPECT_EQ( romParts( rom ), QStringList( { rom } ) );
}

TEST( RomFiles, AnM3uListsItsCuesAndTheirTracksAndMissingFilesToo )
{
   QTemporaryDir dir;
   const QString path = dir.path();
   writeFile( path + "/Game.m3u", "#EXTM3U\nDisc1.cue\r\nsub\\Disc2.cue\n" );
   writeFile( path + "/Disc1.cue", "FILE \"Disc1.bin\" BINARY\nFILE gone.bin BINARY\n" );
   writeFile( path + "/sub/Disc2.cue", "FILE Disc2.bin BINARY\n" );

   EXPECT_EQ( relativeTo( path, romParts( path + "/Game.m3u" ) ),
              QStringList( { "Game.m3u", "Disc1.cue", "sub/Disc2.cue", "Disc1.bin", "gone.bin", "sub/Disc2.bin" } ) );
}

TEST( RomFiles, ASheetNamingItselfOrAnotherSheetBackIsListedOnce )
{
   QTemporaryDir dir;
   const QString path = dir.path();
   writeFile( path + "/A.m3u", "B.m3u\nA.m3u\n" );
   writeFile( path + "/B.m3u", "A.m3u\n" );

   EXPECT_EQ( relativeTo( path, romParts( path + "/A.m3u" ) ), QStringList( { "A.m3u", "B.m3u" } ) );
}

TEST( RomFiles, AGdiNamesItsTracksQuotedOrNot )
{
   QTemporaryDir dir;
   const QString path = dir.path();
   writeFile( path + "/g.gdi", "2\n1 0 4 2352 track01.bin 0\n2 450 0 2352 \"track 02.raw\" 0\n" );

   EXPECT_EQ( relativeTo( path, romParts( path + "/g.gdi" ) ),
              QStringList( { "g.gdi", "track01.bin", "track 02.raw" } ) );
}

TEST( RomFiles, ExclusiveTracksSkipsWhatAnotherListedRomNeedsAndWhatLiesOutsideTheFolder )
{
   QTemporaryDir dir;
   const QString path = dir.path();
   writeFile( path + "/roms/A.cue", "FILE Shared.bin BINARY\nFILE OnlyA.bin BINARY\nFILE ../out.bin BINARY\n" );
   writeFile( path + "/roms/B.cue", "FILE Shared.bin BINARY\n" );
   writeFile( path + "/roms/Shared.bin", "s" );
   writeFile( path + "/roms/OnlyA.bin", "a" );
   writeFile( path + "/out.bin", "o" );

   const QStringList listed{ path + "/roms/B.cue" };
   EXPECT_EQ( relativeTo( path, exclusiveTracks( path + "/roms/A.cue", listed, path + "/roms", false ) ),
              QStringList( { "roms/OnlyA.bin" } ) );
}
