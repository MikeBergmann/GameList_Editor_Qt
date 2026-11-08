// ImageOps: shrinking picture files in place.

#include "GamelistFixture.h"
#include "ImageOps.h"
#include "QtPrinters.h"

#include <QImage>

#include <gtest/gtest.h>

namespace {

// Noise does not compress, so the PNG is big and the test does not depend on the encoder.
QImage noisePicture( int aWidth, int aHeight, quint32 aSeed )
{
   QImage noise( aWidth, aHeight, QImage::Format_RGB32 );
   quint32 state = aSeed;
   for ( int y = 0; y < noise.height(); ++y )
      for ( int x = 0; x < noise.width(); ++x ) {
         state = state * 1664525u + 1013904223u;
         noise.setPixel( x, y, state | 0xFF000000u );
      }
   return noise;
}

TEST( ImageOps, ShrinkImageFileGoesToJustUnderTheLimitInPlace )
{
   Fixture fixture;
   const QString big = fixture.path( "snes/media/images/Sonic.png" );
   ASSERT_TRUE( noisePicture( 600, 400, 12345 ).save( big ) );
   const qint64 before = QFileInfo( big ).size();
   const qint64 limit = before / 3;

   const Result<qint64> shrunk = shrinkImageFile( big, limit );
   ASSERT_TRUE( shrunk ) << shrunk.error().toStdString();
   const qint64 after = shrunk.value();
   ASSERT_GT( after, 0 );
   EXPECT_LE( after, limit );
   EXPECT_GT( after, limit / 2 );  // "ca.": close to the limit, not shrunk to a thumbnail
   EXPECT_EQ( QFileInfo( big ).size(), after );

   const QImage result( big );
   EXPECT_LT( result.width(), 600 );
   EXPECT_NEAR( static_cast<double>( result.width() ) / result.height(), 1.5, 0.02 );

   // Already small enough: left alone, and an unreadable file is an error that leaves it be.
   const Result<qint64> again = shrinkImageFile( big, limit );
   ASSERT_TRUE( again );
   EXPECT_EQ( again.value(), after );
   const QString junk = fixture.path( "snes/media/images/junk.png" );
   writeFile( junk, QByteArray( 5000, 'x' ) );
   const Result<qint64> unreadable = shrinkImageFile( junk, 100 );
   EXPECT_FALSE( unreadable );
   EXPECT_FALSE( unreadable.error().isEmpty() );
   EXPECT_EQ( QFileInfo( junk ).size(), 5000 );
}

TEST( ImageOps, ShrinkImagesTalliesTheBatchAndGoesOnPastAFailure )
{
   Fixture fixture;
   const QImage noise = noisePicture( 300, 200, 7 );
   const QString first = fixture.path( "a.png" );
   const QString junk = fixture.path( "junk.png" );
   const QString last = fixture.path( "b.png" );
   ASSERT_TRUE( noise.save( first ) );
   ASSERT_TRUE( noise.save( last ) );
   writeFile( junk, QByteArray( 5000, 'x' ) );
   const qint64 size = QFileInfo( first ).size();
   const qint64 limit = size / 3;
   const QVector<OversizedImage> batch{ { 0, first, size }, { 1, junk, 5000 }, { 2, last, size } };

   QVector<int> seen;
   const ShrinkReport report = shrinkImages( batch, limit, [&seen]( int aDone ) {
      seen << aDone;
      return true;
   } );

   EXPECT_EQ( seen, ( QVector<int>{ 0, 1, 2 } ) );
   EXPECT_EQ( report.shrunk, 2 );
   ASSERT_EQ( report.problems.size(), 1 );
   EXPECT_TRUE( report.problems.first().contains( QStringLiteral( "junk.png" ) ) );
   EXPECT_EQ( report.saved, ( size - QFileInfo( first ).size() ) + ( size - QFileInfo( last ).size() ) );
   EXPECT_GT( report.saved, 0 );

   const ShrinkReport stopped = shrinkImages( batch, limit, []( int aDone ) { return aDone < 1; } );
   EXPECT_EQ( stopped.shrunk, 1 ) << "stops before the second picture";
   EXPECT_TRUE( stopped.problems.isEmpty() );
}

}  // namespace
