#include "LaunchBoxDump.h"

#include "Resources.h"

#include <QDir>
#include <QFile>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QtEndian>

#include <zlib.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

namespace {

const QLatin1String Cst_ZipUrl( "https://gamesdb.launchbox-app.com/Metadata.zip" );
const QLatin1String Cst_ZipName( "Metadata.zip" );

// Out of range reads 0, which every caller then rejects as damaged.
quint16 le16( const QByteArray& aData, qsizetype aPos )
{
   if ( aPos < 0 || aPos + 2 > aData.size() )
      return 0;
   return qFromLittleEndian<quint16>( aData.constData() + aPos );
}

quint32 le32( const QByteArray& aData, qsizetype aPos )
{
   if ( aPos < 0 || aPos + 4 > aData.size() )
      return 0;
   return qFromLittleEndian<quint32>( aData.constData() + aPos );
}

struct ZipEntry
{
   QString name;
   quint16 flags = 0;
   quint16 method = 0;
   quint32 crc = 0;
   quint32 compressedSize = 0;
   quint32 size = 0;
   quint32 localOffset = 0;
};

constexpr quint32 Cst_Zip64 = 0xFFFFFFFF;

Result<std::vector<ZipEntry>> readDirectory( QFile& aZip )
{
   const qint64 tailSize = qMin<qint64>( aZip.size(), 22 + 0xFFFF );
   if ( !aZip.seek( aZip.size() - tailSize ) )
      return Error{ QStringLiteral( "Cannot read %1." ).arg( aZip.fileName() ) };
   const QByteArray tail = aZip.read( tailSize );
   const qsizetype end = tail.lastIndexOf( QByteArrayLiteral( "PK\x05\x06" ) );
   if ( end < 0 || tail.size() - end < 22 )
      return Error{ QStringLiteral( "%1 is not a zip file." ).arg( aZip.fileName() ) };

   // A plain array: GCC 13 misreads le16( QByteArray ) here as a read of QByteArray's empty string.
   char eocd[22];
   std::memcpy( eocd, tail.constData() + end, sizeof eocd );
   const quint16 count = qFromLittleEndian<quint16>( eocd + 10 );
   const quint32 dirSize = qFromLittleEndian<quint32>( eocd + 12 );
   const quint32 dirOffset = qFromLittleEndian<quint32>( eocd + 16 );
   if ( count == 0xFFFF || dirOffset == Cst_Zip64 )
      return Error{ QStringLiteral( "Zip64 archives are not supported." ) };
   if ( !aZip.seek( dirOffset ) )
      return Error{ QStringLiteral( "The zip directory is out of range." ) };
   const QByteArray dir = aZip.read( dirSize );
   if ( static_cast<quint32>( dir.size() ) != dirSize )
      return Error{ QStringLiteral( "The zip directory is truncated." ) };

   std::vector<ZipEntry> entries;
   qsizetype pos = 0;
   for ( quint16 i = 0; i < count; ++i ) {
      if ( dir.size() - pos < 46 || le32( dir, pos ) != 0x02014b50 )
         return Error{ QStringLiteral( "The zip directory is damaged." ) };
      const qsizetype nameLength = le16( dir, pos + 28 );
      const qsizetype next = pos + 46 + nameLength + le16( dir, pos + 30 ) + le16( dir, pos + 32 );
      if ( next > dir.size() )
         return Error{ QStringLiteral( "The zip directory is damaged." ) };

      ZipEntry entry;
      entry.name = QString::fromUtf8( dir.constData() + pos + 46, nameLength );
      entry.flags = le16( dir, pos + 8 );
      entry.method = le16( dir, pos + 10 );
      entry.crc = le32( dir, pos + 16 );
      entry.compressedSize = le32( dir, pos + 20 );
      entry.size = le32( dir, pos + 24 );
      entry.localOffset = le32( dir, pos + 42 );
      entries.push_back( entry );
      pos = next;
   }
   return entries;
}

struct Inflater
{
   z_stream stream{};
   bool open = false;
   ~Inflater()
   {
      if ( open )
         inflateEnd( &stream );
   }
};

// Streams one entry into aOut and checks its size and CRC.
Result<> extract( QFile& aZip, const ZipEntry& aEntry, QSaveFile& aOut )
{
   const QString damaged = QStringLiteral( "%1 is damaged in the archive." ).arg( aEntry.name );
   if ( ( aEntry.flags & 1 ) != 0 || ( aEntry.method != 0 && aEntry.method != 8 ) )
      return Error{ QStringLiteral( "%1 is encrypted or uses an unsupported compression." )
                       .arg( aEntry.name ) };
   if ( aEntry.compressedSize == Cst_Zip64 || aEntry.size == Cst_Zip64 )
      return Error{ QStringLiteral( "Zip64 archives are not supported." ) };

   // The local header's name/extra lengths can differ from the directory's, so read them here.
   if ( !aZip.seek( aEntry.localOffset ) )
      return Error{ damaged };
   const QByteArray local = aZip.read( 30 );
   if ( local.size() != 30 || le32( local, 0 ) != 0x04034b50 )
      return Error{ damaged };
   if ( !aZip.seek( qint64( aEntry.localOffset ) + 30 + le16( local, 26 ) + le16( local, 28 ) ) )
      return Error{ damaged };

   quint32 crc = static_cast<quint32>( crc32( 0, nullptr, 0 ) );
   quint64 total = 0;
   bool writeFailed = false;
   const auto sink = [&]( const char* aData, qsizetype aSize ) {
      crc = static_cast<quint32>(
         crc32( crc, reinterpret_cast<const Bytef*>( aData ), static_cast<uInt>( aSize ) ) );
      total += static_cast<quint64>( aSize );
      if ( aOut.write( aData, aSize ) != aSize )
         writeFailed = true;
   };

   QByteArray in( 1 << 16, Qt::Uninitialized );
   QByteArray out( 1 << 18, Qt::Uninitialized );
   qint64 remaining = aEntry.compressedSize;
   const auto fill = [&]() -> qsizetype {
      const qint64 got = aZip.read( in.data(), qMin<qint64>( in.size(), remaining ) );
      remaining -= qMax<qint64>( got, 0 );
      return static_cast<qsizetype>( got );
   };

   if ( aEntry.method == 0 ) {
      for ( qsizetype got = fill(); got > 0 && !writeFailed; got = fill() )
         sink( in.constData(), got );
   } else {
      Inflater inflater;
      if ( inflateInit2( &inflater.stream, -15 ) != Z_OK )
         return Error{ QStringLiteral( "Cannot start inflating %1." ).arg( aEntry.name ) };
      inflater.open = true;

      int rc = Z_OK;
      while ( rc != Z_STREAM_END && !writeFailed ) {
         z_stream& s = inflater.stream;
         if ( s.avail_in == 0 ) {
            const qsizetype got = fill();
            if ( got <= 0 )
               return Error{ damaged };
            s.next_in = reinterpret_cast<Bytef*>( in.data() );
            s.avail_in = static_cast<uInt>( got );
         }
         s.next_out = reinterpret_cast<Bytef*>( out.data() );
         s.avail_out = static_cast<uInt>( out.size() );
         rc = inflate( &s, Z_NO_FLUSH );
         if ( rc != Z_OK && rc != Z_STREAM_END )
            return Error{ damaged };
         sink( out.constData(), out.size() - static_cast<qsizetype>( s.avail_out ) );
      }
   }

   if ( writeFailed )
      return Error{ QStringLiteral( "Cannot write %1: %2" ).arg( aEntry.name, aOut.errorString() ) };
   if ( total != aEntry.size || crc != aEntry.crc )
      return Error{ damaged };
   return {};
}

QString describeError( const QNetworkReply* aReply )
{
   const QVariant status = aReply->attribute( QNetworkRequest::HttpStatusCodeAttribute );
   QString description = aReply->errorString();
   if ( status.isValid() )
      description += QStringLiteral( " (HTTP %1)" ).arg( status.toInt() );
   return description;
}

}  // namespace

Result<> unzipEntries( const QString& aZipPath, const QStringList& aNames, const QString& aDestDir )
{
   QFile zip( aZipPath );
   if ( !zip.open( QIODevice::ReadOnly ) )
      return Error{ QStringLiteral( "Cannot open %1: %2" ).arg( aZipPath, zip.errorString() ) };

   const Result<std::vector<ZipEntry>> directory = readDirectory( zip );
   if ( !directory )
      return directory;

   std::vector<std::unique_ptr<QSaveFile>> outputs;
   for ( const QString& name : aNames ) {
      const auto found = std::find_if( directory.value().begin(), directory.value().end(),
                                       [&]( const ZipEntry& aEntry ) { return aEntry.name == name; } );
      if ( found == directory.value().end() )
         return Error{ QStringLiteral( "%1 is not in the archive." ).arg( name ) };

      auto output = std::make_unique<QSaveFile>( aDestDir + QLatin1Char( '/' ) + name );
      if ( !output->open( QIODevice::WriteOnly ) )
         return Error{ QStringLiteral( "Cannot write %1: %2" )
                          .arg( output->fileName(), output->errorString() ) };
      const Result<> extracted = extract( zip, *found, *output );
      if ( !extracted )
         return extracted;
      outputs.push_back( std::move( output ) );
   }

   for ( const auto& output : outputs ) {
      if ( !output->commit() )
         return Error{ QStringLiteral( "Cannot write %1: %2" )
                          .arg( output->fileName(), output->errorString() ) };
   }
   return {};
}

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
      settings.beginGroup( QLatin1String( Cst_IniOptions ) );
      const QByteArray etag = settings.value( QLatin1String( Cst_IniLaunchBoxETag ) ).toString().toUtf8();
      const QByteArray modified =
         settings.value( QLatin1String( Cst_IniLaunchBoxModified ) ).toString().toUtf8();
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
      settings.beginGroup( QLatin1String( Cst_IniOptions ) );
      settings.setValue( QLatin1String( Cst_IniLaunchBoxETag ), FETag );
      settings.setValue( QLatin1String( Cst_IniLaunchBoxModified ), FModified );
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
