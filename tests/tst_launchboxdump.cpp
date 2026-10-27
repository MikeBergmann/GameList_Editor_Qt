// LaunchBoxDump: unzipping with zlib, and the cached refresh against a local HTTP server.

#include "LaunchBoxDump.h"

#include "QtPrinters.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

#include <zlib.h>

namespace {

struct Item
{
   QString name;
   QByteArray data;
   bool stored = false;
};

void put16( QByteArray& aOut, quint16 aValue )
{
   aOut.append( static_cast<char>( aValue & 0xFF ) ).append( static_cast<char>( aValue >> 8 ) );
}

void put32( QByteArray& aOut, quint32 aValue )
{
   put16( aOut, static_cast<quint16>( aValue & 0xFFFF ) );
   put16( aOut, static_cast<quint16>( aValue >> 16 ) );
}

QByteArray rawDeflate( const QByteArray& aData )
{
   z_stream s{};
   deflateInit2( &s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY );
   QByteArray out( static_cast<qsizetype>( deflateBound( &s, static_cast<uLong>( aData.size() ) ) ),
                   Qt::Uninitialized );
   s.next_in = reinterpret_cast<Bytef*>( const_cast<char*>( aData.constData() ) );
   s.avail_in = static_cast<uInt>( aData.size() );
   s.next_out = reinterpret_cast<Bytef*>( out.data() );
   s.avail_out = static_cast<uInt>( out.size() );
   deflate( &s, Z_FINISH );
   out.truncate( static_cast<qsizetype>( s.total_out ) );
   deflateEnd( &s );
   return out;
}

// Deflated entries get a data descriptor (zero sizes in the local header) and a local extra
// field the directory does not have, as the real archive's writers may produce.
QByteArray makeZip( const QVector<Item>& aItems, quint32 aCrcXor = 0 )
{
   QByteArray body, directory;
   for ( const Item& item : aItems ) {
      const QByteArray name = item.name.toUtf8();
      const QByteArray packed = item.stored ? item.data : rawDeflate( item.data );
      const quint32 crc = static_cast<quint32>(
         crc32( 0, reinterpret_cast<const Bytef*>( item.data.constData() ),
                static_cast<uInt>( item.data.size() ) ) );
      const quint16 flags = item.stored ? 0 : 8;
      const quint16 method = item.stored ? 0 : 8;
      const auto offset = static_cast<quint32>( body.size() );

      put32( body, 0x04034b50 );
      put16( body, 20 );
      put16( body, flags );
      put16( body, method );
      put32( body, 0 );
      put32( body, item.stored ? crc : 0 );
      put32( body, item.stored ? static_cast<quint32>( packed.size() ) : 0 );
      put32( body, item.stored ? static_cast<quint32>( item.data.size() ) : 0 );
      put16( body, static_cast<quint16>( name.size() ) );
      put16( body, 4 );
      body += name;
      put32( body, 0 );
      body += packed;
      if ( !item.stored ) {
         put32( body, 0x08074b50 );
         put32( body, crc );
         put32( body, static_cast<quint32>( packed.size() ) );
         put32( body, static_cast<quint32>( item.data.size() ) );
      }

      put32( directory, 0x02014b50 );
      put16( directory, 20 );
      put16( directory, 20 );
      put16( directory, flags );
      put16( directory, method );
      put32( directory, 0 );
      put32( directory, crc ^ aCrcXor );
      put32( directory, static_cast<quint32>( packed.size() ) );
      put32( directory, static_cast<quint32>( item.data.size() ) );
      put16( directory, static_cast<quint16>( name.size() ) );
      put16( directory, 0 );
      put16( directory, 0 );
      put16( directory, 0 );
      put16( directory, 0 );
      put32( directory, 0 );
      put32( directory, offset );
      directory += name;
   }

   QByteArray zip = body + directory;
   put32( zip, 0x06054b50 );
   put16( zip, 0 );
   put16( zip, 0 );
   put16( zip, static_cast<quint16>( aItems.size() ) );
   put16( zip, static_cast<quint16>( aItems.size() ) );
   put32( zip, static_cast<quint32>( directory.size() ) );
   put32( zip, static_cast<quint32>( body.size() ) );
   put16( zip, 0 );
   return zip;
}

// Bigger than the 64 KiB read and 256 KiB write buffers, one compressible, one not.
QByteArray repeated()
{
   QByteArray data;
   while ( data.size() < 700000 )
      data += "<Game><Name>Zelda</Name></Game>\n";
   return data;
}

QByteArray noise()
{
   QByteArray data( 200000, Qt::Uninitialized );
   quint32 state = 12345;
   for ( char& c : data ) {
      state = state * 1664525u + 1013904223u;
      c = static_cast<char>( state >> 24 );
   }
   return data;
}

QByteArray readAll( const QString& aPath )
{
   QFile file( aPath );
   return file.open( QIODevice::ReadOnly ) ? file.readAll() : QByteArray( "<unreadable>" );
}

void writeFile( const QString& aPath, const QByteArray& aData )
{
   QFile file( aPath );
   ASSERT_TRUE( file.open( QIODevice::WriteOnly ) );
   file.write( aData );
}

TEST( UnzipEntries, ExtractsDeflatedAndStoredEntriesAndLeavesTheRest )
{
   QTemporaryDir dir;
   const QVector<Item> items{ { "big.xml", repeated(), false },
                              { "noise.bin", noise(), false },
                              { "plain.txt", "stored", true },
                              { "skipped.xml", "x", false } };
   writeFile( dir.filePath( "a.zip" ), makeZip( items ) );

   const Result<> result = unzipEntries(
      dir.filePath( "a.zip" ), { "big.xml", "noise.bin", "plain.txt" }, dir.path() );

   ASSERT_TRUE( result ) << result.error().toStdString();
   EXPECT_EQ( readAll( dir.filePath( "big.xml" ) ), items[0].data );
   EXPECT_EQ( readAll( dir.filePath( "noise.bin" ) ), items[1].data );
   EXPECT_EQ( readAll( dir.filePath( "plain.txt" ) ), QByteArray( "stored" ) );
   EXPECT_FALSE( QFile::exists( dir.filePath( "skipped.xml" ) ) );
}

TEST( UnzipEntries, ABadCrcFailsAndWritesNothing )
{
   QTemporaryDir dir;
   writeFile( dir.filePath( "a.zip" ),
              makeZip( { { "ok.xml", "fine", false }, { "bad.xml", "oops", false } }, 1 ) );

   const Result<> result = unzipEntries( dir.filePath( "a.zip" ), { "ok.xml", "bad.xml" }, dir.path() );

   EXPECT_FALSE( result );
   EXPECT_FALSE( QFile::exists( dir.filePath( "ok.xml" ) ) ) << "an all-or-nothing extract";
   EXPECT_FALSE( QFile::exists( dir.filePath( "bad.xml" ) ) );
}

TEST( UnzipEntries, MissingEntryAndNotAZipAreErrors )
{
   QTemporaryDir dir;
   writeFile( dir.filePath( "a.zip" ), makeZip( { { "ok.xml", "fine", false } } ) );
   writeFile( dir.filePath( "b.zip" ), "this is not a zip file at all" );

   const Result<> missing = unzipEntries( dir.filePath( "a.zip" ), { "nope.xml" }, dir.path() );
   EXPECT_FALSE( missing );
   EXPECT_TRUE( missing.error().contains( "nope.xml" ) ) << missing.error().toStdString();

   EXPECT_FALSE( unzipEntries( dir.filePath( "b.zip" ), { "ok.xml" }, dir.path() ) );
   EXPECT_FALSE( unzipEntries( dir.filePath( "none.zip" ), { "ok.xml" }, dir.path() ) );
}

// Answers every GET with the zip, or 304 when the request carries the matching ETag.
class FakeServer
{
public:
   explicit FakeServer( QByteArray aZip ) : FZip( std::move( aZip ) )
   {
      EXPECT_TRUE( FServer.listen( QHostAddress::LocalHost ) );
      QObject::connect( &FServer, &QTcpServer::newConnection, &FServer, [this] {
         while ( QTcpSocket* socket = FServer.nextPendingConnection() ) {
            QObject::connect( socket, &QTcpSocket::readyRead, socket, [this, socket] {
               FRequest += socket->readAll();
               if ( !FRequest.contains( "\r\n\r\n" ) )
                  return;
               respond( socket );
               FRequest.clear();
            } );
         }
      } );
   }

   QUrl url() const { return QUrl( QStringLiteral( "http://127.0.0.1:%1/Metadata.zip" ).arg( FServer.serverPort() ) ); }
   int requests = 0;
   QVector<bool> sentValidator;

private:
   void respond( QTcpSocket* aSocket )
   {
      ++requests;
      const bool validator = FRequest.toLower().contains( "if-none-match: \"v1\"" );
      sentValidator.push_back( validator );
      QByteArray reply = validator ? QByteArray( "HTTP/1.1 304 Not Modified\r\n" )
                                   : QByteArray( "HTTP/1.1 200 OK\r\n" );
      reply += "ETag: \"v1\"\r\nConnection: close\r\n";
      if ( validator )
         reply += "\r\n";
      else
         reply += "Content-Length: " + QByteArray::number( FZip.size() ) + "\r\n\r\n" + FZip;
      aSocket->write( reply );
      aSocket->disconnectFromHost();
   }

   QTcpServer FServer;
   QByteArray FZip;
   QByteArray FRequest;
};

struct Outcome
{
   bool done = false;
   bool ok = false;
   bool updated = false;
   QString error;
};

Outcome refreshAndWait( LaunchBoxDump& aDump )
{
   Outcome outcome;
   QEventLoop loop;
   QObject::connect( &aDump, &LaunchBoxDump::finished, &loop, [&]( bool aUpdated ) {
      outcome = { true, true, aUpdated, {} };
      loop.quit();
   } );
   QObject::connect( &aDump, &LaunchBoxDump::failed, &loop, [&]( const QString& aMessage ) {
      outcome = { true, false, false, aMessage };
      loop.quit();
   } );
   QTimer::singleShot( 20000, &loop, &QEventLoop::quit );
   aDump.refresh();
   if ( !outcome.done )
      loop.exec();
   QObject::disconnect( &aDump, nullptr, &loop, nullptr );
   return outcome;
}

TEST( LaunchBoxDump, RefreshDownloadsUnpacksAndThenAsksIfModified )
{
   QSettings().remove( QStringLiteral( "Options/LaunchBoxETag" ) );
   QSettings().remove( QStringLiteral( "Options/LaunchBoxModified" ) );
   QTemporaryDir dir;
   const QByteArray metadata = repeated();
   FakeServer server( makeZip( { { "Metadata.xml", metadata, false },
                                 { "Files.xml", "<LaunchBox/>", false },
                                 { "Mame.xml", "<LaunchBox/>", false },
                                 { "Platforms.xml", "<LaunchBox/>", false } } ) );
   LaunchBoxDump dump( dir.filePath( "cache" ) );
   dump.setProxy( {}, 0, {}, {} );
   dump.setUrl( server.url() );
   ASSERT_FALSE( dump.isCached() );

   const Outcome first = refreshAndWait( dump );
   ASSERT_TRUE( first.ok ) << first.error.toStdString();
   EXPECT_TRUE( first.updated );
   EXPECT_TRUE( dump.isCached() );
   EXPECT_EQ( readAll( dump.filePath( "Metadata.xml" ) ), metadata );
   EXPECT_FALSE( QFile::exists( dump.filePath( "Metadata.zip" ) ) ) << "the zip is not kept";
   EXPECT_FALSE( QFile::exists( dump.filePath( "Platforms.xml" ) ) ) << "only the files we read";

   const Outcome second = refreshAndWait( dump );
   ASSERT_TRUE( second.ok ) << second.error.toStdString();
   EXPECT_FALSE( second.updated ) << "304 must not unpack again";
   EXPECT_EQ( server.sentValidator, ( QVector<bool>{ false, true } ) );

   QFile::remove( dump.filePath( "Files.xml" ) );
   const Outcome third = refreshAndWait( dump );
   ASSERT_TRUE( third.ok ) << third.error.toStdString();
   EXPECT_TRUE( third.updated ) << "a validator is useless once files are missing";
   EXPECT_EQ( server.sentValidator.back(), false );
   EXPECT_TRUE( dump.isCached() );
}

TEST( LaunchBoxDump, ABrokenDownloadFailsAndKeepsNoValidator )
{
   QSettings().remove( QStringLiteral( "Options/LaunchBoxETag" ) );
   QTemporaryDir dir;
   FakeServer server( "this is not a zip" );
   LaunchBoxDump dump( dir.filePath( "cache" ) );
   dump.setProxy( {}, 0, {}, {} );
   dump.setUrl( server.url() );

   const Outcome outcome = refreshAndWait( dump );

   EXPECT_FALSE( outcome.ok );
   EXPECT_FALSE( outcome.error.isEmpty() );
   EXPECT_FALSE( dump.isCached() );
   EXPECT_FALSE( QSettings().contains( QStringLiteral( "Options/LaunchBoxETag" ) ) )
      << "the next refresh must download again";
}

}  // namespace
