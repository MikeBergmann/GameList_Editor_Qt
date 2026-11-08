#include "ImageOps.h"

#include <QBuffer>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QSaveFile>

#include <cmath>

Result<qint64> shrinkImageFile( const QString& aPath, qint64 aMaxBytes )
{
   const auto failed = [&]( const QString& aMessage ) {
      return Error{ QStringLiteral( "%1: %2" ).arg( QFileInfo( aPath ).fileName(), aMessage ) };
   };

   // The reader is scoped so its file handle is closed before QSaveFile renames over
   // aPath: Windows refuses to replace a file that is still open.
   QByteArray format;
   QImage picture;
   {
      QImageReader reader( aPath );
      reader.setAutoTransform( false );
      format = reader.format();
      if ( reader.imageCount() > 1 )
         return failed( QStringLiteral( "animated pictures are left alone." ) );

      picture = reader.read();
      if ( picture.isNull() )
         return failed( reader.errorString() );
   }

   const qint64 original = QFileInfo( aPath ).size();
   if ( original <= aMaxBytes )
      return original;

   // Size follows the pixel count, so aim at the area ratio and correct from what
   // the encoder really produced. Always scaled from the original: scaling the
   // scaled copy over and over would blur it. 0.95 aims a little under the limit.
   double factor = std::sqrt( static_cast<double>( aMaxBytes ) / static_cast<double>( original ) ) * 0.95;
   QByteArray encoded;
   for ( int pass = 0; pass < 8; ++pass ) {
      const QSize size( qMax( 1, qRound( picture.width() * factor ) ),
                        qMax( 1, qRound( picture.height() * factor ) ) );
      const QImage scaled = picture.scaled( size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );

      encoded.clear();
      QBuffer buffer( &encoded );
      buffer.open( QIODevice::WriteOnly );
      QImageWriter writer( &buffer, format );
      if ( !writer.write( scaled ) )
         return failed( writer.errorString() );

      if ( encoded.size() <= aMaxBytes )
         break;

      factor *= std::sqrt( static_cast<double>( aMaxBytes ) / static_cast<double>( encoded.size() ) ) * 0.95;
      encoded.clear();
   }

   if ( encoded.isEmpty() )
      return failed( QStringLiteral( "cannot be made that small." ) );

   QSaveFile file( aPath );
   if ( !file.open( QIODevice::WriteOnly ) || file.write( encoded ) != encoded.size() ||
        !file.commit() )
      return failed( file.errorString() );

   return qint64( encoded.size() );
}

ShrinkReport shrinkImages( const QVector<OversizedImage>& aImages, qint64 aMaxBytes,
                           const std::function<bool( int aDone )>& aProgress )
{
   ShrinkReport report;
   for ( int row = 0; row < aImages.size(); ++row ) {
      if ( aProgress && !aProgress( row ) )
         break;

      const Result<qint64> size = shrinkImageFile( aImages.at( row ).path, aMaxBytes );
      if ( !size ) {
         report.problems << size.error();
         continue;
      }
      ++report.shrunk;
      report.saved += aImages.at( row ).size - size.value();
   }
   return report;
}
