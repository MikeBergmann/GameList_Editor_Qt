// Android entry point: the QML front end over the mobile controllers.

#include "ActionsController.h"
#include "AndroidStorage.h"
#include "FileBrowserController.h"
#include "GameEditController.h"
#include "GamesController.h"
#include "LibraryController.h"
#include "ScrapeController.h"
#include "ScrapeImageProvider.h"
#include "SettingsController.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>

int main( int argc, char** argv )
{
   QGuiApplication app( argc, argv );

   // Same names as the desktop app, so the settings keys mean the same thing.
   QCoreApplication::setOrganizationName( QStringLiteral( "GameListEditorQt" ) );
   QCoreApplication::setApplicationName( QStringLiteral( "GameListEditorQt" ) );
   QCoreApplication::setApplicationVersion( QStringLiteral( GLE_VERSION ) );
   QSettings::setDefaultFormat( QSettings::IniFormat );

   LibraryController library;
   ActionsController actions( &library );
   GamesController games( &library );
   GameEditController edit( &library, &games );
   ScrapeController scrape( &library, &games );
   SettingsController settings;
   FileBrowserController browser;
   browser.setVolumeSource( &AndroidStorage::externalVolumes );
   edit.setOpener( &AndroidStorage::openFile );
   AndroidStorage storage;

   QQmlApplicationEngine engine;
   engine.addImageProvider( QStringLiteral( "scrape" ), new ScrapeImageProvider( &scrape ) );
   QQmlContext* context = engine.rootContext();
   context->setContextProperty( QStringLiteral( "library" ), &library );
   context->setContextProperty( QStringLiteral( "actions" ), &actions );
   context->setContextProperty( QStringLiteral( "games" ), &games );
   context->setContextProperty( QStringLiteral( "edit" ), &edit );
   context->setContextProperty( QStringLiteral( "scrape" ), &scrape );
   context->setContextProperty( QStringLiteral( "settings" ), &settings );
   context->setContextProperty( QStringLiteral( "browser" ), &browser );
   context->setContextProperty( QStringLiteral( "storage" ), &storage );
   QObject::connect( &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit( 1 ); }, Qt::QueuedConnection );
   engine.loadFromModule( "GleMobile", "Main" );

   return app.exec();
}
