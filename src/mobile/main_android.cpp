// Android entry point: the QML front end over the mobile controllers.

#include "GamesController.h"
#include "LibraryController.h"

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
   QSettings::setDefaultFormat( QSettings::IniFormat );

   LibraryController library;
   GamesController games( &library );

   QQmlApplicationEngine engine;
   engine.rootContext()->setContextProperty( QStringLiteral( "library" ), &library );
   engine.rootContext()->setContextProperty( QStringLiteral( "games" ), &games );
   QObject::connect( &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit( 1 ); }, Qt::QueuedConnection );
   engine.loadFromModule( "GleMobile", "Main" );

   return app.exec();
}
