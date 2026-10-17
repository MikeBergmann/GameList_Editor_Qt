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

// Client for screenscraper.fr's jeuInfos.php. Asynchronous (QNetworkAccessManager); everything
// runs on the calling thread, so nothing needs locking.

// One requested lookup. The caller supplies crc32 and romSize, so this stays free of widgets.
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

// One <medias><media> the user can choose to download.
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

   // ssuser/maxthreads, clamped to at least 1 (an absent node must not mean zero downloads).
   int maxThreads = 1;
};

// Parses one jeuInfos.php response; needs no network. An Error when the XML is invalid or
// the reply holds no <jeu> node (an API error document).
Result<ScrapedInfo> parseScrapeResponse( const QByteArray& aXml, const QSet<QString>& aMediaTypes,
                                        bool aWantVideo, LangName aLanguage );

// Strips the developer's and the user's ids and passwords from a message. Callers must pass
// the whole error text, not just the URL they built: QNetworkReply::errorString() embeds its
// own copy of the request URL, credentials included.
QString sanitizeUrl( const QString& aUrl );

// Decodes one of the XOR-encoded, NUL-terminated Cst_Dev*Xor arrays of DevCredentials.h.
// aData must be terminated (the generated arrays always are).
QString decodeDevSecret( const unsigned char* aData );

// The jeuInfos.php URL for aRequest, every value percent-encoded. Exposed so a
// test can check the encoding without a network.
QString buildGameQuery( const ScrapeRequest& aRequest );

// False when the build carries no developer credentials (devid/devpassword);
// requestGame() then fails immediately instead of calling the API.
bool devCredentialsConfigured();

class Scraper : public QObject
{
   Q_OBJECT

public:
   explicit Scraper( QObject* aParent = nullptr );

   // Empty aServer disables the proxy.
   void setProxy( const QString& aServer, int aPort, const QString& aUser,
                  const QString& aPassword );

   // aMediaTypes holds the Cst_Media* picture types to offer; aWantVideo is separate because
   // the video is offered apart from the pictures.
   void requestGame( const ScrapeRequest& aRequest, const QSet<QString>& aMediaTypes,
                     bool aWantVideo, LangName aLanguage );

   // Downloads aMedia, at most aMaxThreads at a time (the server states that limit; ignoring
   // it risks a ban). mediaReady fires once per picture that arrives, mediaFinished once after
   // all have arrived or failed.
   void requestMedia( const QVector<MediaInfo>& aMedia, int aMaxThreads = 1 );

   // Downloads one file (the chosen video) to aDestPath; emits fileSaved or fileSaveFailed.
   void downloadToFile( const QString& aUrl, const QString& aDestPath );

signals:
   void gameInfoReady( const ScrapedInfo& aInfo );
   // aDetails is the (sanitized) body the server sent; empty when there was none.
   void gameInfoFailed( const QString& aMessage, const QString& aDetails = QString() );

   void mediaReady( int aIndex, const QImage& aImage );
   void mediaFailed( int aIndex, const QString& aMessage );
   void mediaFinished();

   void fileSaved( const QString& aPath );
   void fileSaveFailed( const QString& aPath, const QString& aMessage );

private:
   // State of one requestMedia() call, so overlapping batches cannot interfere.
   struct MediaBatch
   {
      QVector<MediaInfo> media;
      int next = 0;
      int active = 0;
   };
   void startNextMedia( const std::shared_ptr<MediaBatch>& aState );

   QNetworkAccessManager FManager;
};
