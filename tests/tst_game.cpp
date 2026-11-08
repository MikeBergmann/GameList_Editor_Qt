#include "Game.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>

namespace {

// The ROM name is the last path component; the extension-less form drops only the last extension.
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

// <lastplayed> and <releasedate> are basic ISO 8601 without separators. Parsing is
// strict, so a value that cannot be parsed has to come back invalid rather than as a
// wrong date.
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

TEST( Game, FormatsGamelistDateTimesForDisplay )
{
   EXPECT_EQ( gamelistDateTimeToDisplay( QStringLiteral( "20130101T180530" ) ),
              QStringLiteral( "01/01/2013 18:05:30" ) );
   EXPECT_TRUE( gamelistDateTimeToDisplay( QString() ).isEmpty() );
   EXPECT_TRUE( gamelistDateTimeToDisplay( QStringLiteral( "0" ) ).isEmpty() );
}

TEST( Assets, QrcIsCompiledIntoTheBinary )
{
   EXPECT_TRUE( QFile::exists( QStringLiteral( ":/DefaultPictures/default.png" ) ) );
   EXPECT_EQ( QDir( QStringLiteral( ":/SystemsLogos" ) ).entryList( QDir::Files ).size(), 77 );
}

}  // namespace
