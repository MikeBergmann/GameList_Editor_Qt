// Phase 1 smoke test: load every gamelist under fixed folders and log the counts (adb logcat).

#include "Gamelist.h"
#include "Systems.h"

#include <QGuiApplication>
#include <QSslSocket>

int main( int argc, char** argv )
{
   QGuiApplication app( argc, argv );

   qInfo( "GLE smoke: ssl=%d", QSslSocket::supportsSsl() ? 1 : 0 );

   // Limitation: throwaway paths (internal storage, this device's SD card).
   const QStringList roots{ QStringLiteral( "/storage/emulated/0/gle_smoke" ),
                             QStringLiteral( "/storage/1107-2E5F/ROMs" ) };
   for ( const QString& root : roots )
   {
      const QVector<SystemEntry> systems = scanSystems( root );
      qInfo( "GLE smoke: %lld systems under %s", static_cast<long long>( systems.size() ),
             qPrintable( root ) );

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

   return app.exec();
}
