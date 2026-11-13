#include "ScrapeImageProvider.h"

#include "ScrapeController.h"

ScrapeImageProvider::ScrapeImageProvider( const ScrapeController* aController )
   : QQuickImageProvider( QQuickImageProvider::Image ), FController( aController )
{
}

QImage ScrapeImageProvider::requestImage( const QString& aId, QSize* aSize, const QSize& aRequestedSize )
{
   const QImage source = FController->picture( aId.section( QLatin1Char( '/' ), 1 ).toInt() );

   // A request that gives one side only (sourceSize.height) leaves the other at 0.
   QSize target = aRequestedSize;
   if ( target.width() <= 0 )
      target.setWidth( source.width() );
   if ( target.height() <= 0 )
      target.setHeight( source.height() );

   const QImage result = source.isNull() || ( target.width() >= source.width() && target.height() >= source.height() )
                            ? source
                            : source.scaled( target, Qt::KeepAspectRatio, Qt::SmoothTransformation );
   if ( aSize )
      *aSize = result.size();
   return result;
}
