// GoogleTest's own main() cannot be used: constructing any QWidget needs a
// living QApplication, so the dialog tests need one set up before they run.

#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <gtest/gtest.h>

int main( int argc, char** argv )
{
   // Keep the widget tests runnable without a display. Only a default, so that
   // running them on a real desktop to watch a dialog still works.
   if ( qEnvironmentVariableIsEmpty( "QT_QPA_PLATFORM" ) )
      qputenv( "QT_QPA_PLATFORM", "offscreen" );

   QApplication app( argc, argv );

   QCoreApplication::setOrganizationName( QStringLiteral( "GameListEditorTests" ) );
   QCoreApplication::setApplicationName( QStringLiteral( "tests" ) );
   QSettings::setDefaultFormat( QSettings::IniFormat );

   // Anything that writes settings must not touch the real config.
   QTemporaryDir settingsDir;
   if ( !settingsDir.isValid() )
      return 1;
   QSettings::setPath( QSettings::IniFormat, QSettings::UserScope, settingsDir.path() );

   testing::InitGoogleTest( &argc, argv );
   return RUN_ALL_TESTS();
}
