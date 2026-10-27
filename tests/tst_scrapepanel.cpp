// ScrapePanel: the save path and the Save button's arming logic, driven through
// Scraper's own signals rather than a real network round trip.

#include "GamelistFixture.h"
#include "QtPrinters.h"
#include "ScrapePanel.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QToolButton>

namespace {

struct Panel
{
   Fixture fixture;
   Gamelist list;
   ScrapePanel panel;

   Panel() : panel( nullptr, fixture.path( "launchbox" ) )
   {
      EXPECT_TRUE( list.load( fixture.snes ) );
      panel.setGamelist( &list );
   }

   // The source is a stored setting: put it back for the tests that follow.
   ~Panel() { combo( "Cbo_Source" )->setCurrentIndex( 0 ); }

   QComboBox* combo( const char* aName ) const
   {
      return panel.findChild<QComboBox*>( QLatin1String( aName ) );
   }

   QCheckBox* check( const char* aName ) const
   {
      return panel.findChild<QCheckBox*>( QLatin1String( aName ) );
   }

   QPushButton* button( const char* aName ) const
   {
      return panel.findChild<QPushButton*>( QLatin1String( aName ) );
   }

   QLineEdit* edit( const char* aName ) const
   {
      return panel.findChild<QLineEdit*>( QLatin1String( aName ) );
   }
};

ScrapedInfo sampleInfo()
{
   ScrapedInfo info;
   info.name = QStringLiteral( "Sonic the Hedgehog 2" );
   info.genre = QStringLiteral( "Platform" );
   info.publisher = QStringLiteral( "Sega" );
   return info;
}

TEST( ScrapePanel, SelectionDrivesTheScrapeButtonByWhetherTheRomExists )
{
   Panel panel;

   panel.panel.setSelection( 0 );  // Sonic.zip is on disk
   EXPECT_TRUE( panel.button( "Btn_Scrape" )->isEnabled() );

   panel.panel.setSelection( 2 );  // Vanished.zip: orphan, nothing to hash
   EXPECT_FALSE( panel.button( "Btn_Scrape" )->isEnabled() );

   panel.panel.setSelection( -1 );
   EXPECT_FALSE( panel.button( "Btn_Scrape" )->isEnabled() );
   EXPECT_TRUE( panel.edit( "Edt_ScrapeRomPath" )->text().isEmpty() );
}

TEST( ScrapePanel, GameInfoFillsFieldsAndMediaFinishedArmsSave )
{
   Panel panel;
   panel.panel.setSelection( 0 );

   emit panel.panel.scraper().gameInfoReady( sampleInfo() );

   EXPECT_EQ( panel.edit( "Edt_ScrapeName" )->text(), QStringLiteral( "Sonic the Hedgehog 2" ) );

   // sampleInfo() has no pictures, so the empty-batch shortcut fires mediaFinished
   // synchronously inside the gameInfoReady handler. Chk_ScrapeInfos starts checked,
   // so the button is armed at that same instant.
   EXPECT_TRUE( panel.button( "Btn_ScrapeSave" )->isEnabled() );

   panel.check( "Chk_ScrapeInfos" )->setChecked( false );
   EXPECT_FALSE( panel.button( "Btn_ScrapeSave" )->isEnabled() );
}

TEST( ScrapePanel, ClickingAThumbnailSelectsItsPicture )
{
   Panel panel;
   panel.panel.setSelection( 0 );
   emit panel.panel.scraper().gameInfoReady( sampleInfo() );

   QImage thumb( 4, 4, QImage::Format_RGB32 );
   thumb.fill( Qt::red );
   emit panel.panel.scraper().mediaReady( 0, thumb );
   emit panel.panel.scraper().mediaFinished();

   EXPECT_FALSE( panel.check( "Chk_ScrapePicture" )->isChecked() );

   QToolButton* thumbButton = panel.panel.findChild<QToolButton*>();
   ASSERT_NE( thumbButton, nullptr );
   thumbButton->click();

   EXPECT_TRUE( panel.check( "Chk_ScrapePicture" )->isEnabled() );
   EXPECT_TRUE( panel.check( "Chk_ScrapePicture" )->isChecked() );
}

TEST( ScrapePanel, SaveWritesTheCheckedFieldsAndReloadsTheGamelist )
{
   Panel panel;
   panel.panel.setSelection( 0 );
   emit panel.panel.scraper().gameInfoReady( sampleInfo() );
   emit panel.panel.scraper().mediaFinished();

   int gamesChanged = 0;
   QObject::connect( &panel.panel, &ScrapePanel::gamesChanged, [&] { ++gamesChanged; } );

   // Save infos only - no picture chosen, video left unchecked.
   panel.button( "Btn_ScrapeSave" )->click();
   QCoreApplication::processEvents();  // save() defers the write one event-loop turn

   EXPECT_EQ( panel.list.at( 0 ).name, QStringLiteral( "Sonic the Hedgehog 2" ) );
   EXPECT_EQ( panel.list.at( 0 ).genre, QStringLiteral( "Platform" ) );
   EXPECT_EQ( panel.list.at( 0 ).publisher, QStringLiteral( "Sega" ) );
   EXPECT_EQ( gamesChanged, 1 );

   // And it reached the file, not just the vector.
   Gamelist saved;
   ASSERT_TRUE( saved.load( panel.fixture.snes ) );
   EXPECT_EQ( saved.at( 0 ).name, QStringLiteral( "Sonic the Hedgehog 2" ) );
}

TEST( ScrapePanel, UncheckingSaveInfosLeavesTheStoredFieldsAlone )
{
   Panel panel;
   panel.panel.setSelection( 0 );
   emit panel.panel.scraper().gameInfoReady( sampleInfo() );
   emit panel.panel.scraper().mediaFinished();

   panel.check( "Chk_ScrapeInfos" )->setChecked( false );
   EXPECT_FALSE( panel.button( "Btn_ScrapeSave" )->isEnabled() );

   // Nothing is checked, so there is nothing to do and nothing to crash on.
   panel.button( "Btn_ScrapeSave" )->click();
   EXPECT_EQ( panel.list.at( 0 ).name, QStringLiteral( "Sonic The Hedgehog" ) );
}

TEST( ScrapePanel, SaveIsBusyUntilTheWriteEnds )
{
   Panel panel;
   panel.panel.setSelection( 0 );
   emit panel.panel.scraper().gameInfoReady( sampleInfo() );
   emit panel.panel.scraper().mediaFinished();

   QVector<bool> states;
   QObject::connect( &panel.panel, &ScrapePanel::busyChanged,
                     [&]( bool aBusy ) { states.append( aBusy ); } );

   panel.button( "Btn_ScrapeSave" )->click();
   EXPECT_EQ( states, QVector<bool>( { true } ) );
   EXPECT_FALSE( panel.panel.isEnabled() );

   QCoreApplication::processEvents();
   EXPECT_EQ( states, QVector<bool>( { true, false } ) );
   EXPECT_TRUE( panel.panel.isEnabled() );
}

TEST( ScrapePanel, SaveAsksTheGuardFirstAndStopsWhenItSaysNo )
{
   Panel panel;
   panel.panel.setSelection( 0 );
   emit panel.panel.scraper().gameInfoReady( sampleInfo() );
   emit panel.panel.scraper().mediaFinished();

   int asked = 0;
   panel.panel.setLeaveGuard( [&] {
      ++asked;
      return false;
   } );
   int gamesChanged = 0;
   QObject::connect( &panel.panel, &ScrapePanel::gamesChanged, [&] { ++gamesChanged; } );

   panel.button( "Btn_ScrapeSave" )->click();
   QCoreApplication::processEvents();

   EXPECT_EQ( asked, 1 );
   EXPECT_EQ( gamesChanged, 0 );
   EXPECT_TRUE( panel.panel.isEnabled() );
   EXPECT_EQ( panel.list.at( 0 ).name, QStringLiteral( "Sonic The Hedgehog" ) );
}

// Otherwise the next media save writes the unsaved infos out unasked.
TEST( ScrapePanel, AFailedSaveLeavesTheGamelistAsItWas )
{
   Panel panel;
   panel.panel.setSelection( 0 );
   emit panel.panel.scraper().gameInfoReady( sampleInfo() );
   emit panel.panel.scraper().mediaFinished();
   breakSave( panel.fixture, panel.list );

   panel.button( "Btn_ScrapeSave" )->click();
   QTimer::singleShot( 0, [] {
      if ( QWidget* box = QApplication::activeModalWidget() )
         box->close();
   } );
   QCoreApplication::processEvents();  // runs the deferred write, which warns

   EXPECT_EQ( panel.list.at( 0 ).name, QStringLiteral( "Sonic The Hedgehog" ) );
   EXPECT_EQ( panel.list.at( 0 ).genre, QStringLiteral( "Platform" ) );
   EXPECT_EQ( panel.list.at( 0 ).publisher, QStringLiteral( "Sega" ) );
   EXPECT_EQ( panel.list.domText( 0, "name" ), QStringLiteral( "Sonic The Hedgehog" ) );
}

const QByteArray Cst_LbMetadata = R"xml(<LaunchBox>
  <Game>
    <DatabaseID>1</DatabaseID>
    <Name>Sonic the Hedgehog</Name>
    <Platform>Super Nintendo Entertainment System</Platform>
    <Overview>From LaunchBox.</Overview>
    <Genres>Platform; Action</Genres>
  </Game>
</LaunchBox>
)xml";

void writeLaunchBoxCache( const Panel& aPanel, const QByteArray& aMetadata )
{
   for ( const char* name : { "Metadata.xml", "Files.xml", "Mame.xml" } )
      writeFile( aPanel.fixture.path( "launchbox/" ) + QLatin1String( name ),
                 QLatin1String( name ) == QLatin1String( "Metadata.xml" ) ? aMetadata
                                                                           : QByteArray( "<LaunchBox/>" ) );
}

void waitUntilEnabled( const QWidget& aWidget )
{
   QElapsedTimer timer;
   timer.start();
   while ( !aWidget.isEnabled() && timer.elapsed() < 20000 ) {
      QCoreApplication::processEvents( QEventLoop::AllEvents, 20 );
      QThread::msleep( 5 );
   }
}

void closeNextDialog()
{
   QTimer::singleShot( 0, [] {
      if ( QWidget* box = QApplication::activeModalWidget() )
         box->close();
   } );
}

TEST( ScrapePanel, LaunchBoxSourceDisablesWhatItCannotDoAndIsRemembered )
{
   Panel panel;
   panel.panel.setSelection( 0 );
   ASSERT_TRUE( panel.check( "Chk_ManualCRC" )->isEnabled() );

   panel.combo( "Cbo_Source" )->setCurrentIndex( 1 );
   for ( const char* name : { "Chk_Mix1", "Chk_Mix2", "Chk_ArcadeBox", "Chk_Video", "Chk_ManualCRC" } )
      EXPECT_FALSE( panel.check( name )->isEnabled() ) << name;
   EXPECT_TRUE( panel.check( "Chk_Box2D" )->isEnabled() );
   EXPECT_FALSE( panel.button( "Btn_RefreshLaunchBox" )->isHidden() );

   {
      ScrapePanel second;
      second.activate();
      EXPECT_EQ( second.findChild<QComboBox*>( QStringLiteral( "Cbo_Source" ) )->currentIndex(), 1 );
   }

   panel.combo( "Cbo_Source" )->setCurrentIndex( 0 );
   EXPECT_TRUE( panel.check( "Chk_Video" )->isEnabled() );
   EXPECT_TRUE( panel.check( "Chk_ManualCRC" )->isEnabled() );
   EXPECT_TRUE( panel.button( "Btn_RefreshLaunchBox" )->isHidden() );
}

TEST( ScrapePanel, LaunchBoxScrapeIndexesOnceAndFillsTheFields )
{
   Panel panel;
   writeLaunchBoxCache( panel, Cst_LbMetadata );
   panel.combo( "Cbo_Source" )->setCurrentIndex( 1 );
   panel.panel.setSelection( 0 );

   panel.button( "Btn_Scrape" )->click();
   waitUntilEnabled( panel.panel );

   EXPECT_EQ( panel.edit( "Edt_ScrapeName" )->text(), QStringLiteral( "Sonic the Hedgehog" ) );
   EXPECT_EQ( panel.edit( "Edt_ScrapeGenre" )->text(), QStringLiteral( "Platform - Action" ) );
   EXPECT_EQ( panel.panel.findChild<QPlainTextEdit*>( QStringLiteral( "Mmo_ScrapeDescription" ) )->toPlainText(),
              QStringLiteral( "From LaunchBox." ) );
   EXPECT_TRUE( panel.button( "Btn_ScrapeSave" )->isEnabled() );

   // The index is kept in memory, so a second lookup needs no file and no thread.
   ASSERT_TRUE( QFile::remove( panel.fixture.path( "launchbox/Metadata.xml" ) ) );
   writeFile( panel.fixture.path( "launchbox/Metadata.xml" ), "not xml" );
   panel.button( "Btn_Scrape" )->click();
   EXPECT_TRUE( panel.panel.isEnabled() );
   EXPECT_EQ( panel.edit( "Edt_ScrapeName" )->text(), QStringLiteral( "Sonic the Hedgehog" ) );
}

TEST( ScrapePanel, LaunchBoxScrapeWithoutTheDumpWarnsAndStaysUsable )
{
   Panel panel;
   panel.combo( "Cbo_Source" )->setCurrentIndex( 1 );
   panel.panel.setSelection( 0 );

   closeNextDialog();
   panel.button( "Btn_Scrape" )->click();

   EXPECT_TRUE( panel.panel.isEnabled() );
   EXPECT_TRUE( panel.edit( "Edt_ScrapeName" )->text().isEmpty() );
}

TEST( ScrapePanel, LaunchBoxGameNotFoundWarnsAndStaysUsable )
{
   Panel panel;
   writeLaunchBoxCache( panel, "<LaunchBox/>" );
   panel.combo( "Cbo_Source" )->setCurrentIndex( 1 );
   panel.panel.setSelection( 0 );

   QTimer closer;
   QObject::connect( &closer, &QTimer::timeout, [] {
      if ( QWidget* box = QApplication::activeModalWidget() )
         box->close();
   } );
   closer.start( 20 );
   panel.button( "Btn_Scrape" )->click();
   waitUntilEnabled( panel.panel );

   EXPECT_TRUE( panel.edit( "Edt_ScrapeName" )->text().isEmpty() );
}

}  // namespace
