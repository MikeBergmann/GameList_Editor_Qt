// Phase 1/2 smoke test: load real gamelists, then write to a scratch folder on each storage root
// and log PASS/FAIL (adb logcat | findstr GLE).

#include "Gamelist.h"
#include "PathUtil.h"
#include "Systems.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QSslSocket>

namespace
{
   void check( bool aOk, const QString& aWhat )
   {
      qInfo( "GLE write: %s %s", aOk ? "PASS" : "FAIL", qPrintable( aWhat ) );
   }

   template <typename T>
   bool checkResult( const Result<T>& aResult, const QString& aWhat )
   {
      check( static_cast<bool>( aResult ), aResult ? aWhat : aWhat + ": " + aResult.error() );
      return static_cast<bool>( aResult );
   }

   bool writeFile( const QString& aPath, const QByteArray& aBytes )
   {
      QFile file( aPath );
      return file.open( QIODevice::WriteOnly ) && file.write( aBytes ) == aBytes.size();
   }

   void readTest( const QString& aRoot )
   {
      const QVector<SystemEntry> systems = scanSystems( aRoot );
      qInfo( "GLE smoke: %lld systems under %s", static_cast<long long>( systems.size() ),
             qPrintable( aRoot ) );

      for ( const SystemEntry& system : systems )
      {
         Gamelist list;
         const Result<> loaded = list.load( system );
         if ( loaded )
            qInfo( "GLE smoke: %s: %d games", qPrintable( system.folderName ), list.count() );
         else
            qInfo( "GLE smoke: %s: FAILED %s", qPrintable( system.folderName ),
                   qPrintable( loaded.error() ) );
      }
   }

   // Writes only under <aRoot>/gle_write_test, which it deletes first and last.
   void writeTest( const QString& aRoot )
   {
      const QString base = aRoot + "/gle_write_test";
      qInfo( "GLE write: === %s", qPrintable( base ) );
      QDir( base ).removeRecursively();

      if ( !QDir().mkpath( base + "/nes" ) )
      {
         check( false, "mkpath" );
         return;
      }
      const SystemEntry system = systemAt( base, "nes" );
      const QString romPath = base + "/nes/Sonic.nes";
      check( writeFile( romPath, "rom" ), "write ROM file" );
      if ( !checkResult( createGamelist( system.gamelistPath ), "createGamelist" ) )
         return;
      qInfo( "GLE write: volume folds case: %d", volumeFoldsCase( system.gamelistPath ) ? 1 : 0 );

      Gamelist list;
      if ( !checkResult( list.load( system, true ), "load empty" ) ||
           !checkResult( list.addGames( { romPath } ), "addGames" ) )
         return;
      check( list.count() == 1, "one game" );

      GameFields fields;
      fields.name = QStringLiteral( "Sonic Test" );
      checkResult( list.setFields( { 0 }, fields, gfName ), "setFields (QSaveFile)" );

      QImage picture( 64, 64, QImage::Format_RGB32 );
      picture.fill( Qt::red );
      checkResult( list.setImage( 0, picture ), "setImage (QImage::save + rename)" );

      const QString videoSource = base + "/source.mp4";
      check( writeFile( videoSource, "video" ), "write video source" );
      checkResult( list.setVideo( 0, videoSource ), "setVideo (QFile::copy)" );

      Gamelist again;
      if ( checkResult( again.load( system ), "reload" ) )
      {
         check( again.at( 0 ).name == fields.name, "name survived the save" );
         const QString image = physicalPath( again.romDir(), again.at( 0 ).imagePath );
         const QString video = physicalPath( again.romDir(), again.at( 0 ).videoPath );
         check( QFile::exists( image ), "image file exists: " + image );
         check( QFile::exists( video ), "video file exists: " + video );
      }

      const QString cased = base + "/nes/CaseTest.PNG";
      check( writeFile( cased, "x" ), "write CaseTest.PNG" );
      const QString found = resolvePath( base + "/nes/casetest.png" );
      check( QFile::exists( found ) && QFileInfo( found ).fileName().compare( "casetest.png", Qt::CaseInsensitive ) == 0,
             "resolvePath finds a differently-cased file: " + found );

      checkResult( list.removeGame( 0, true ), "removeGame with files" );
      check( !QFile::exists( romPath ), "ROM file removed" );

      check( QDir( base ).removeRecursively(), "cleanup" );
   }
}  // namespace

int main( int argc, char** argv )
{
   QGuiApplication app( argc, argv );

   qInfo( "GLE smoke: ssl=%d", QSslSocket::supportsSsl() ? 1 : 0 );

   // Limitation: throwaway paths (internal storage, this device's SD card).
   readTest( QStringLiteral( "/storage/emulated/0/gle_smoke" ) );
   readTest( QStringLiteral( "/storage/1107-2E5F/ROMs" ) );
   writeTest( QStringLiteral( "/storage/emulated/0" ) );
   writeTest( QStringLiteral( "/storage/1107-2E5F" ) );
   qInfo( "GLE write: done" );

   return app.exec();
}
