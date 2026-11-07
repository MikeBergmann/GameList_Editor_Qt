#include "LaunchBoxDump.h"

#include "Resources.h"
#include "Unzip.h"

#include <QDir>
#include <QFile>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>
#include <memory>

namespace {

const QLatin1String Cst_ZipUrl( "https://gamesdb.launchbox-app.com/Metadata.zip" );
const QLatin1String Cst_ZipName( "Metadata.zip" );

QString describeError( const QNetworkReply* aReply )
{
   const QVariant status = aReply->attribute( QNetworkRequest::HttpStatusCodeAttribute );
   QString description = aReply->errorString();
   if ( status.isValid() )
      description += QStringLiteral( " (HTTP %1)" ).arg( status.toInt() );
   return description;
}

}  // namespace

LaunchBoxDump::LaunchBoxDump( const QString& aCacheDir, QObject* aParent )
   : QObject( aParent ), FCacheDir( aCacheDir ), FUrl( Cst_ZipUrl )
{
}

LaunchBoxDump::~LaunchBoxDump()
{
   if ( FThread )
      FThread->wait();
}

QString LaunchBoxDump::defaultCacheDir()
{
   return QStandardPaths::writableLocation( QStandardPaths::CacheLocation ) +
          QStringLiteral( "/launchbox" );
}

QStringList LaunchBoxDump::fileNames()
{
   return { QStringLiteral( "Metadata.xml" ), QStringLiteral( "Files.xml" ),
            QStringLiteral( "Mame.xml" ) };
}

QString LaunchBoxDump::filePath( const QString& aName ) const
{
   return FCacheDir + QLatin1Char( '/' ) + aName;
}

bool LaunchBoxDump::isCached() const
{
   const QStringList names = fileNames();
   return std::all_of( names.begin(), names.end(),
                       [this]( const QString& aName ) { return QFile::exists( filePath( aName ) ); } );
}

void LaunchBoxDump::setProxy( const QString& aServer, int aPort, const QString& aUser,
                              const QString& aPassword )
{
   if ( aServer.isEmpty() ) {
      FManager.setProxy( QNetworkProxy::NoProxy );
      return;
   }
   FManager.setProxy( QNetworkProxy( QNetworkProxy::HttpProxy, aServer,
                                     static_cast<quint16>( aPort ), aUser, aPassword ) );
}

void LaunchBoxDump::refresh()
{
   if ( FBusy )
      return;
   FBusy = true;
   if ( !QDir().mkpath( FCacheDir ) ) {
      fail( QStringLiteral( "Cannot create %1." ).arg( FCacheDir ) );
      return;
   }

   QNetworkRequest request( FUrl );
   request.setTransferTimeout( QNetworkRequest::DefaultTransferTimeoutConstant );
   request.setAttribute( QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy );
   // Validators only count while the files they describe are there.
   if ( isCached() ) {
      QSettings settings;
      settings.beginGroup( Cst_IniOptions );
      const QByteArray etag = settings.value( Cst_IniLaunchBoxETag ).toString().toUtf8();
      const QByteArray modified =
         settings.value( Cst_IniLaunchBoxModified ).toString().toUtf8();
      if ( !etag.isEmpty() )
         request.setRawHeader( "If-None-Match", etag );
      if ( !modified.isEmpty() )
         request.setRawHeader( "If-Modified-Since", modified );
   }

   FWriteError.clear();
   FReply = FManager.get( request );
   connect( FReply, &QNetworkReply::downloadProgress, this, &LaunchBoxDump::downloadProgress );
   connect( FReply, &QNetworkReply::readyRead, this, [this] {
      const QByteArray chunk = FReply->readAll();
      if ( FReply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt() != 200 ||
           !FWriteError.isEmpty() )
         return;
      if ( !FZip ) {
         FZip = std::make_unique<QSaveFile>( filePath( Cst_ZipName ) );
         if ( !FZip->open( QIODevice::WriteOnly ) ) {
            FWriteError = QStringLiteral( "Cannot write %1: %2" )
                             .arg( FZip->fileName(), FZip->errorString() );
            FReply->abort();
            return;
         }
      }
      if ( FZip->write( chunk ) != chunk.size() ) {
         FWriteError = QStringLiteral( "Cannot write %1: %2" )
                          .arg( FZip->fileName(), FZip->errorString() );
         FReply->abort();
      }
   } );
   connect( FReply, &QNetworkReply::finished, this, [this] { onDownloaded( FReply ); } );
}

void LaunchBoxDump::onDownloaded( QNetworkReply* aReply )
{
   FReply = nullptr;
   aReply->deleteLater();
   const int status = aReply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();

   if ( !FWriteError.isEmpty() ) {
      fail( FWriteError );
      return;
   }
   if ( aReply->error() != QNetworkReply::NoError ) {
      fail( QStringLiteral( "Download failed: %1" ).arg( describeError( aReply ) ) );
      return;
   }
   if ( status == 304 ) {
      FBusy = false;
      emit finished( false );
      return;
   }
   if ( status != 200 || !FZip ) {
      fail( QStringLiteral( "Download failed: unexpected reply (HTTP %1)" ).arg( status ) );
      return;
   }
   if ( !FZip->commit() ) {
      fail( QStringLiteral( "Cannot write %1: %2" ).arg( FZip->fileName(), FZip->errorString() ) );
      return;
   }
   FZip.reset();

   FETag = QString::fromLatin1( aReply->rawHeader( "ETag" ) );
   FModified = QString::fromLatin1( aReply->rawHeader( "Last-Modified" ) );
   unpack();
}

void LaunchBoxDump::unpack()
{
   emit unpacking();

   const QString zipPath = filePath( Cst_ZipName );
   const QStringList names = fileNames();
   const QString dir = FCacheDir;
   const auto result = std::make_shared<Result<>>();
   FThread = QThread::create( [=] { *result = unzipEntries( zipPath, names, dir ); } );
   FThread->setParent( this );
   connect( FThread, &QThread::finished, this, [this, result, zipPath] {
      FThread->deleteLater();
      FThread = nullptr;
      if ( !*result ) {
         fail( result->error() );
         return;
      }

      QFile::remove( zipPath );
      // Stored only now: a failed unpack must make the next refresh download again.
      QSettings settings;
      settings.beginGroup( Cst_IniOptions );
      settings.setValue( Cst_IniLaunchBoxETag, FETag );
      settings.setValue( Cst_IniLaunchBoxModified, FModified );
      FBusy = false;
      emit finished( true );
   } );
   FThread->start();
}

void LaunchBoxDump::fail( const QString& aMessage )
{
   FZip.reset();
   FBusy = false;
   emit failed( aMessage );
}
