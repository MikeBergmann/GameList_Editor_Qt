// PathUtil: the file helper that keeps a write atomic.

#include "GamelistFixture.h"
#include "PathUtil.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

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

}  // namespace
