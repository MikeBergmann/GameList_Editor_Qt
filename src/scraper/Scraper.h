#pragma once

#include "Resources.h"

#include <QImage>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <memory>

// screenscraper.fr's jeuInfos.php, replacing GetGameXml/ParseXml/GetPictures/
// GetPicture/ThreadTerminated/DisplayPictures (F_Main 3510-3805) and all of
// U_DownloadThread.pas. QNetworkAccessManager is already asynchronous, so the
// Delphi's worker-thread pool - one TDOwnThread per download, a
// TCriticalSection-guarded counter, FreeOnTerminate - collapses into one
// QNetworkAccessManager and a completion count kept on the main thread: there
// is only one thread, so nothing needs locking.

// One requested lookup. GetGameXml computed Crc32/Size itself from a mix of
// widget state (Chk_ManualCRC) and Game fields; here the caller (ScrapePanel)
// hands over the result, via Game::crc32 or the user's override, and
// QFileInfo( romPath ).size() for romSize - so this stays free of widgets.
struct ScrapeRequest
{
   QString systemId;
   QString crc32;
   bool manualCrc = false;  // Chk_ManualCRC.Checked: send only the crc, no rom name/size
   QString romName;
   qint64 romSize = 0;

   QString ssLogin;
   QString ssPassword;

};

// One <medias><media> the user can choose to download. TMediaInfo, minus the
// leak (3739 never freed one on the success path).
struct MediaInfo
{
   QString fileExt;
   QString fileLink;
};

// FillFields' targets (Edt_ScrapeName...Edt_ScrapeRating), already resolved
// for aLanguage - the per-language dictionary fallback chains run once, here,
// so a future ScrapePanel is nine plain assignments and nothing else.
struct ScrapedInfo
{
   QString name;
   QString region;
   QString description;
   QString releaseDate;  // dd/MM/yyyy, MM/yyyy or yyyy - GetFormatedDate's three forms
   QString genre;
   QString publisher;
   QString developer;
   QString players;
   QString rating;

   QVector<MediaInfo> pictures;  // already filtered to the requested media types
   QString videoLink;            // empty unless offered and requested

   // ssuser/maxthreads, clamped to at least 1. The 0-threads deadlock (3708:
   // an absent node left FMaxThreads at 0, so the fan-out loop ran zero times
   // and the form stayed disabled forever) cannot happen here.
   int maxThreads = 1;
};

// Parses one jeuInfos.php response. Free of QNetworkAccessManager, so it is
// the part a test can exercise against a canned response with no network.
// False (with a message in aError) when the reply holds no <jeu> node (3613:
// an API error document raised out of ParseXml with the form still disabled).
bool parseScrapeResponse( const QByteArray& aXml, const QSet<QString>& aMediaTypes, bool aWantVideo,
                          LangName aLanguage, ScrapedInfo& aInfo, QString* aError = nullptr );

// WarnUserWithSafeUrl (3984) scrubbed only the constant &devpassword=..., not
// the user's own &sspassword=... - so a failed scrape showed the user their
// own password back. A generic value-after-key regex catches both. Callers
// must run this over the *whole* error message, not just the request URL
// they built: QNetworkReply::errorString() embeds its own copy of the
// request URL, credentials included.
QString sanitizeUrl( const QString& aUrl );

// False when the build carries no developer credentials (devid/devpassword);
// requestGame() then fails immediately instead of calling the API.
bool devCredentialsConfigured();

class Scraper : public QObject
{
   Q_OBJECT

public:
   explicit Scraper( QObject* aParent = nullptr );

   // Empty aServer disables the proxy, matching TProxySettings.Create('',0,'','').
   void setProxy( const QString& aServer, int aPort, const QString& aUser,
                  const QString& aPassword );

   // aMediaTypes is the nine Chk_Box2D...Chk_Wheel checkboxes, as the set of
   // Cst_Media* values still checked; aWantVideo is Chk_Video, kept separate
   // because it is offered through a different button than the pictures are.
   void requestGame( const ScrapeRequest& aRequest, const QSet<QString>& aMediaTypes,
                     bool aWantVideo, LangName aLanguage );

   // Downloads aMedia, aMaxThreads at a time - the fixed-size pool GetPictures
   // ran, preserved because the server states that limit and ignoring it risks
   // a ban, not just a slowdown. mediaReady fires once per picture, in
   // aMedia's order; mediaFinished fires once, after all have arrived or failed.
   void requestMedia( const QVector<MediaInfo>& aMedia, int aMaxThreads = 1 );

   // SaveLinkToFile (2473) - GetPictures' sibling for the one video link the
   // user chose to keep.
   void downloadToFile( const QString& aUrl, const QString& aDestPath );

signals:
   void gameInfoReady( const ScrapedInfo& aInfo );
   void gameInfoFailed( const QString& aMessage );

   void mediaReady( int aIndex, const QImage& aImage );
   void mediaFailed( int aIndex, const QString& aMessage );
   void mediaFinished();

   void fileSaved( const QString& aPath );
   void fileSaveFailed( const QString& aPath, const QString& aMessage );

private:
   // Fixed-size download pool state, one per requestMedia() call. A plain
   // member would break if a second scrape started before the first finished;
   // this way each batch is self-contained and needs no lock.
   struct MediaBatch
   {
      QVector<MediaInfo> media;
      int next = 0;
      int active = 0;
   };
   void startNextMedia( const std::shared_ptr<MediaBatch>& aState );

   QNetworkAccessManager FManager;
};
