#pragma once

#include <QQuickImageProvider>

class ScrapeController;

// image://scrape/<generation>/<index>: a scraped picture, scaled to the size the Image asks for.
// Not async, so it runs on the GUI thread, where the controller's pictures live.
class ScrapeImageProvider : public QQuickImageProvider
{
public:
   explicit ScrapeImageProvider( const ScrapeController* aController );

   QImage requestImage( const QString& aId, QSize* aSize, const QSize& aRequestedSize ) override;

private:
   const ScrapeController* FController;
};
