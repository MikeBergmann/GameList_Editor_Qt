// PathUtil: the file helper that keeps a write atomic.

#include "GamelistFixture.h"
#include "PathUtil.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace {

QByteArray readAll( const QString& aPath )
{
   QFile file( aPath );
   return file.open( QIODevice::ReadOnly ) ? file.readAll() : QByteArray();
}

TEST( PathUtil, WriteFileAtomicallyReplacesTheContent )
{
   QTemporaryDir dir;
   const QString path = dir.filePath( QStringLiteral( "list.txt" ) );
   writeFile( path, "old" );

   ASSERT_TRUE( writeFileAtomically( path, "new\n" ) );
   EXPECT_EQ( readAll( path ), QByteArray( "new\n" ) );
}

TEST( PathUtil, WriteFileAtomicallyFailsWithAMessageWhenTheFolderIsMissing )
{
   QTemporaryDir dir;
   const QString path = dir.filePath( QStringLiteral( "nowhere/list.txt" ) );

   const Result<> written = writeFileAtomically( path, "x" );
   ASSERT_FALSE( written );
   EXPECT_TRUE( written.error().contains( path ) );
}

TEST( PathUtil, DiscardFileRemovesTheFileAndReportsAMissingOne )
{
   QTemporaryDir dir;
   const QString path = dir.filePath( QStringLiteral( "gone.txt" ) );
   writeFile( path, "x" );

   EXPECT_TRUE( discardFile( path ) );
   EXPECT_FALSE( QFileInfo::exists( path ) );
   EXPECT_FALSE( discardFile( path ) );
}

TEST( PathUtil, FileKeyAndSameFileFoldCaseOnlyWhenAsked )
{
   EXPECT_EQ( fileKey( QStringLiteral( "/a/./Sonic.zip" ), false ), QStringLiteral( "/a/Sonic.zip" ) );
   EXPECT_EQ( fileKey( QStringLiteral( "/a/Sonic.zip" ), true ), QStringLiteral( "/a/sonic.zip" ) );
   EXPECT_FALSE( sameFile( QStringLiteral( "/a/Sonic.zip" ), QStringLiteral( "/a/sonic.zip" ), false ) );
   EXPECT_TRUE( sameFile( QStringLiteral( "/a/Sonic.zip" ), QStringLiteral( "/a/sonic.zip" ), true ) );
}

TEST( PathUtil, IsInsideDirIsTrueForDescendantsOnly )
{
   EXPECT_TRUE( isInsideDir( QStringLiteral( "/roms/pcenginecd" ), QStringLiteral( "/roms/pcenginecd/a/Game.cue" ) ) );
   EXPECT_FALSE( isInsideDir( QStringLiteral( "/roms/pcenginecd" ), QStringLiteral( "/roms/Game.cue" ) ) );
   EXPECT_FALSE( isInsideDir( QStringLiteral( "/roms/pcenginecd" ), QStringLiteral( "/roms/pcenginecd2/Game.cue" ) ) );
}

TEST( PathUtil, SameLocationSeesThroughSpellingAndIsFalseForAPathThatIsNotThere )
{
   QTemporaryDir dir;
   ASSERT_TRUE( QDir().mkpath( dir.filePath( QStringLiteral( "a" ) ) ) );
   ASSERT_TRUE( QDir().mkpath( dir.filePath( QStringLiteral( "b" ) ) ) );

   EXPECT_TRUE( sameLocation( dir.filePath( QStringLiteral( "a" ) ), dir.filePath( QStringLiteral( "b/../a/" ) ) ) );
   EXPECT_FALSE( sameLocation( dir.filePath( QStringLiteral( "a" ) ), dir.filePath( QStringLiteral( "b" ) ) ) );
   EXPECT_FALSE( sameLocation( dir.filePath( QStringLiteral( "nope" ) ), dir.filePath( QStringLiteral( "nope" ) ) ) );
}

// Uppercase hex: existing gamelists hold these already, and a
// lowercase digest would make every scraped game look changed.
TEST( PathUtil, HashesAreUppercaseHex )
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
TEST( PathUtil, HashesAreEmptyForAnUnreadableFile )
{
   const QString missing = QStringLiteral( "/nonexistent/absent.bin" );

   EXPECT_TRUE( fileHash( missing, QCryptographicHash::Md5 ).isEmpty() );
   EXPECT_TRUE( fileCrc32( missing ).isEmpty() );
}

}  // namespace
