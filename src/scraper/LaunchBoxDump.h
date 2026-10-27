#pragma once

#include "Result.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <memory>

class QNetworkReply;
class QSaveFile;
class QThread;

// Extracts the named entries of a zip into aDestDir, each written atomically and only once all
// of them inflated and matched their CRC. Stored and deflated entries; no zip64, no encryption.
Result<> unzipEntries( const QString& aZipPath, const QStringList& aNames, const QString& aDestDir );

// Keeps a local copy of LaunchBox's Metadata.zip, unpacked, in a cache folder. Asynchronous, on
// the calling thread; only the inflating runs on a worker. The zip is deleted after unpacking.
// The unpacked files are rewritten in place: close any reader of them before refresh().
class LaunchBoxDump : public QObject
{
   Q_OBJECT

public:
   explicit LaunchBoxDump( const QString& aCacheDir, QObject* aParent = nullptr );
   ~LaunchBoxDump() override;

   // <CacheLocation>/launchbox
   static QString defaultCacheDir();
   // The XML files we read: Metadata.xml, Files.xml, Mame.xml.
   static QStringList fileNames();

   QString filePath( const QString& aName ) const;
   // Every file of fileNames() is present.
   bool isCached() const;
   bool busy() const { return FBusy; }

   // Empty aServer disables the proxy.
   void setProxy( const QString& aServer, int aPort, const QString& aUser,
                  const QString& aPassword );
   // For tests.
   void setUrl( const QUrl& aUrl ) { FUrl = aUrl; }

   // Does nothing while busy(). Sends the stored ETag / Last-Modified, so an unchanged dump
   // costs no download. Exactly one of finished() / failed() follows.
   void refresh();

signals:
   // aTotal is -1 when the server did not say.
   void downloadProgress( qint64 aReceived, qint64 aTotal );
   void unpacking();
   // aUpdated is false when the cached copy was already current.
   void finished( bool aUpdated );
   void failed( const QString& aMessage );

private:
   void onDownloaded( QNetworkReply* aReply );
   void unpack();
   void fail( const QString& aMessage );

   QString FCacheDir;
   QUrl FUrl;
   QNetworkAccessManager FManager{ this };
   QNetworkReply* FReply = nullptr;
   std::unique_ptr<QSaveFile> FZip;
   QThread* FThread = nullptr;
   QString FWriteError;
   QString FETag;
   QString FModified;
   bool FBusy = false;
};
