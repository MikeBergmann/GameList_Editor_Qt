#pragma once

#include "Resources.h"
#include "Result.h"

#include <QImage>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <memory>

// Client for screenscraper.fr's jeuInfos.php. Asynchronous, on the calling thread.

// One requested lookup.
struct ScrapeRequest
{
   QString systemId;
   QString crc32;
   bool manualCrc = false;  // send only the crc, no rom name/size
   QString romName;
   qint64 romSize = 0;

   QString ssLogin;
   QString ssPassword;

};

// A downloadable media entry.
struct MediaInfo
{
   QString fileExt;
   QString fileLink;
};

// The scraped text fields, already resolved for the requested language.
struct ScrapedInfo
{
   QString name;
   QString region;
   QString description;
   QString releaseDate;  // dd/MM/yyyy, MM/yyyy or yyyy
   QString genre;
   QString publisher;
   QString developer;
   QString players;
   QString rating;

   QVector<MediaInfo> pictures;  // already filtered to the requested media types
   QString videoLink;            // empty unless offered and requested

   // Clamped to at least 1: an absent node must not mean zero downloads.
   int maxThreads = 1;
};

// Needs no network. An Error when the XML is invalid or holds no <jeu>.
Result<ScrapedInfo> parseScrapeResponse( const QByteArray& aXml, const QSet<QString>& aMediaTypes,
                                        bool aWantVideo, LangName aLanguage );

// Strips credentials from a message. Pass the whole error text, not just your
// URL: QNetworkReply::errorString() embeds its own copy of the request URL.
QString sanitizeUrl( const QString& aUrl );

// aData must be NUL-terminated.
QString decodeDevSecret( const unsigned char* aData );

// Every value percent-encoded.
QString buildGameQuery( const ScrapeRequest& aRequest );

// False when the build has no developer credentials; requestGame() then fails
// immediately.
bool devCredentialsConfigured();

class Scraper : public QObject
{
   Q_OBJECT

public:
   explicit Scraper( QObject* aParent = nullptr );

   // Empty aServer disables the proxy.
   void setProxy( const QString& aServer, int aPort, const QString& aUser,
                  const QString& aPassword );

   // The video is offered apart from the pictures, hence aWantVideo.
   void requestGame( const ScrapeRequest& aRequest, const QSet<QString>& aMediaTypes,
                     bool aWantVideo, LangName aLanguage );

   // At most aMaxThreads at a time; the server states that limit. mediaReady fires
   // per picture, mediaFinished once at the end.
   void requestMedia( const QVector<MediaInfo>& aMedia, int aMaxThreads = 1 );


   void downloadToFile( const QString& aUrl, const QString& aDestPath );

signals:
   void gameInfoReady( const ScrapedInfo& aInfo );
   // aDetails is the sanitized server body, if any.
   void gameInfoFailed( const QString& aMessage, const QString& aDetails = QString() );

   void mediaReady( int aIndex, const QImage& aImage );
   void mediaFailed( int aIndex, const QString& aMessage );
   void mediaFinished();

   void fileSaved( const QString& aPath );
   void fileSaveFailed( const QString& aPath, const QString& aMessage );

private:
   // One requestMedia() call's state, so overlapping batches cannot interfere.
   struct MediaBatch
   {
      QVector<MediaInfo> media;
      int next = 0;
      int active = 0;
   };
   void startNextMedia( const std::shared_ptr<MediaBatch>& aState );

   QNetworkAccessManager FManager;
};
