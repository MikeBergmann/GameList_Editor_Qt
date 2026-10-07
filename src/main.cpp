// Application entry point.

#include "F_Main.h"

#include <QApplication>
#include <QIcon>
#include <QSettings>

int main( int argc, char** argv )
{
   QApplication app( argc, argv );

   QCoreApplication::setOrganizationName( QStringLiteral( "GameListEditor" ) );
   QCoreApplication::setApplicationName( QStringLiteral( "GameListEditor" ) );
   // Without this QSettings writes the registry on Windows, which is neither
   // inspectable nor consistent with the Linux build.
   QSettings::setDefaultFormat( QSettings::IniFormat );

   app.setWindowIcon( QIcon( QStringLiteral( ":/Icons/app.png" ) ) );

   Frm_Editor editor;
   editor.show();
   editor.showTipsAtStart();

   return app.exec();
}
