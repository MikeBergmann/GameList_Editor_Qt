// ScrapePanel: the save path and the Save button's arming logic, driven
// through Scraper's own signals rather than a real network round trip -
// firing gameInfoReady/mediaReady/mediaFinished directly is the same thing
// tst_scraper.cpp does for parseScrapeResponse, one layer up.

#include "GamelistFixture.h"
#include "QtPrinters.h"
#include "ScrapePanel.h"

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QImage>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>

namespace {

struct Panel
{
   Fixture fixture;
   Gamelist list;
   ScrapePanel panel;

   Panel()
   {
      EXPECT_TRUE( list.load( fixture.snes ) );
      panel.setGamelist( &list );
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

   // sampleInfo() has no pictures, so requestMedia's empty-batch shortcut
   // fires mediaFinished synchronously, right inside gameInfoReady's handler -
   // Chk_ScrapeInfos starts checked, so the button is armed at that same
   // instant. EnableScrapeComponents(True) plus Chk_ScrapeClick folded into
   // one flag.
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

   EXPECT_EQ( panel.list.at( 0 ).name, QStringLiteral( "Sonic the Hedgehog 2" ) );
   EXPECT_EQ( panel.list.at( 0 ).genre, QStringLiteral( "Platform" ) );
   EXPECT_EQ( panel.list.at( 0 ).publisher, QStringLiteral( "Sega" ) );
   EXPECT_EQ( gamesChanged, 1 );

   // And it reached the file, not just the vector - setFields() alone does
   // not save, so this also covers finishSave() asking for one explicitly.
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

}  // namespace
