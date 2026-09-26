#include "Game.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

namespace {

// Delphi split on the path separator, then dropped the last extension.
TEST( Game, RomNameIsTheFileNameOfRomPath )
{
   Game game;

   game.romPath = QStringLiteral( "./Sonic The Hedgehog 2.zip" );
   EXPECT_EQ( game.romName(), QStringLiteral( "Sonic The Hedgehog 2.zip" ) );
   EXPECT_EQ( game.romNameWoExt(), QStringLiteral( "Sonic The Hedgehog 2" ) );

   game.romPath = QStringLiteral( "no-directory-part" );
   EXPECT_EQ( game.romName(), QStringLiteral( "no-directory-part" ) );
   EXPECT_EQ( game.romNameWoExt(), QStringLiteral( "no-directory-part" ) );

   // Only the last extension goes, so .tar.gz keeps its .tar.
   game.romPath = QStringLiteral( "./sub/dir/rom.tar.gz" );
   EXPECT_EQ( game.romName(), QStringLiteral( "rom.tar.gz" ) );
   EXPECT_EQ( game.romNameWoExt(), QStringLiteral( "rom.tar" ) );

   game.romPath = QString();
   EXPECT_TRUE( game.romName().isEmpty() );
   EXPECT_TRUE( game.romNameWoExt().isEmpty() );
}

TEST( Game, GamelistFlagOnlyAcceptsTheLiteralTrue )
{
   EXPECT_EQ( gamelistFlag( QStringLiteral( "true" ) ), 1 );
   EXPECT_EQ( gamelistFlag( QStringLiteral( "false" ) ), 0 );
   EXPECT_EQ( gamelistFlag( QStringLiteral( "True" ) ), 0 );
   EXPECT_EQ( gamelistFlag( QString() ), 0 );
}

// Uppercase hex: existing gamelists hold these already, and a
// lowercase digest would make every scraped game look changed.
TEST( Game, HashesAreUppercaseHex )
{
   QTemporaryDir dir;
   ASSERT_TRUE( dir.isValid() );

   const QString romPath = dir.filePath( QStringLiteral( "rom.bin" ) );
   QFile rom( romPath );
   ASSERT_TRUE( rom.open( QIODevice::WriteOnly ) );
   ASSERT_EQ( rom.write( "abc" ), 3 );
   rom.close();

   EXPECT_EQ( fileHash( romPath, QCryptographicHash::Md5 ),
              QStringLiteral( "900150983CD24FB0D6963F7D28E17F72" ) );
   EXPECT_EQ( fileHash( romPath, QCryptographicHash::Sha1 ),
              QStringLiteral( "A9993E364706816ABA3E25717850C26C9CD0D89D" ) );
   EXPECT_EQ( fileCrc32( romPath ), QStringLiteral( "352441C2" ) );
}

// Orphan ROMs are routine here, so a missing file has to come back empty rather
// than raise: callers test emptiness to decide whether to rescrape.
TEST( Game, HashesAreEmptyForAnUnreadableFile )
{
   const QString missing = QStringLiteral( "/nonexistent/absent.bin" );

   EXPECT_TRUE( fileHash( missing, QCryptographicHash::Md5 ).isEmpty() );
   EXPECT_TRUE( fileCrc32( missing ).isEmpty() );
}

// <lastplayed> and <releasedate> are basic ISO 8601 without separators. Delphi
// went through ISO8601ToDate, which is lenient about a lot; this is not, so a
// value it cannot parse has to come back invalid rather than as a wrong date.
TEST( Game, ParsesGamelistDateTimes )
{
   const QDateTime parsed = gamelistDateTime( QStringLiteral( "20130101T180000" ) );
   ASSERT_TRUE( parsed.isValid() );
   EXPECT_EQ( parsed.date(), QDate( 2013, 1, 1 ) );
   EXPECT_EQ( parsed.time(), QTime( 18, 0, 0 ) );

   // EmulationStation writes local time with no zone designator, so no shift.
   EXPECT_EQ( parsed.timeSpec(), Qt::LocalTime );
}

TEST( Game, RejectsUnparseableGamelistDateTimes )
{
   EXPECT_FALSE( gamelistDateTime( QString() ).isValid() );
   EXPECT_FALSE( gamelistDateTime( QStringLiteral( "0" ) ).isValid() );
   EXPECT_FALSE( gamelistDateTime( QStringLiteral( "not a date" ) ).isValid() );

   // Right shape, impossible day: there is no 31st of February.
   EXPECT_FALSE( gamelistDateTime( QStringLiteral( "20130231T180000" ) ).isValid() );
}

TEST( Assets, QrcIsCompiledIntoTheBinary )
{
   EXPECT_TRUE( QFile::exists( QStringLiteral( ":/DefaultPictures/default.png" ) ) );
   EXPECT_EQ( QDir( QStringLiteral( ":/SystemsLogos" ) ).entryList( QDir::Files ).size(), 77 );
}

}  // namespace
