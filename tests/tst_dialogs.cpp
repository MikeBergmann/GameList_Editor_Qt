// Construction smoke tests for the dialogs, plus the Help dialog's opt-out behaviour.

#include "F_About.h"
#include "F_AdvNameEditor.h"
#include "F_ConfigureNetwork.h"
#include "F_Help.h"
#include "F_MoreInfos.h"
#include "Game.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

#include <QLineEdit>
#include <QTimer>

namespace {

template <typename TDialog> void expectConstructs()
{
   TDialog dialog;
   EXPECT_FALSE( dialog.windowTitle().isEmpty() );
   EXPECT_NE( dialog.layout(), nullptr );
}

TEST( Dialogs, AllConstruct )
{
   expectConstructs<Frm_About>();
   expectConstructs<Frm_Help>();
   expectConstructs<Frm_MoreInfos>();
   expectConstructs<Frm_AdvNameEditor>();
   expectConstructs<Frm_Network>();
}

TEST( Dialogs, MoreInfosShowsAGamesFilesAndFields )
{
   Game game;
   game.playcount = QStringLiteral( "12" );
   game.lastplayed = QStringLiteral( "20130101T180000" );
   game.crc32 = QStringLiteral( "352441C2" );
   game.physicalRomPath = QStringLiteral( "/roms/snes/Sonic.zip" );
   game.physicalImagePath = QStringLiteral( "/media/snes/covers/Sonic.png" );
   game.missingImage = true;

   Frm_MoreInfos dialog;
   QTimer::singleShot( 0, &dialog, &QDialog::reject );
   dialog.Execute( game, QStringLiteral( "/appdata/gamelists/snes/gamelist.xml" ) );

   const auto text = [&dialog]( const char* aName ) {
      return dialog.findChild<QLineEdit*>( QLatin1String( aName ) )->text();
   };
   EXPECT_EQ( text( "Edt_Gamelist" ), QStringLiteral( "/appdata/gamelists/snes/gamelist.xml" ) );
   EXPECT_EQ( text( "Edt_Rom" ), QStringLiteral( "/roms/snes/Sonic.zip" ) );
   EXPECT_EQ( text( "Edt_Image" ), QStringLiteral( "/media/snes/covers/Sonic.png (missing)" ) );
   EXPECT_TRUE( text( "Edt_Video" ).isEmpty() ) << "no link, so no path and no \"(missing)\"";
   EXPECT_EQ( text( "Edt_Playcount" ), QStringLiteral( "12" ) );
   EXPECT_EQ( text( "Edt_Crc32" ), QStringLiteral( "352441C2" ) );
}

// With the opt-out hidden, opening Help must hand the "show tips" setting back
// unchanged, in both states.
TEST( Dialogs, HelpLeavesTheSettingAloneWhenTheOptOutIsHidden )
{
   Frm_Help dialog;

   QTimer::singleShot( 0, &dialog, &QDialog::reject );
   EXPECT_TRUE( dialog.Execute( true, false ) );

   QTimer::singleShot( 0, &dialog, &QDialog::reject );
   EXPECT_FALSE( dialog.Execute( false, false ) );
}

// With the opt-out offered but not ticked, tips stay on.
TEST( Dialogs, HelpKeepsTipsOnWhenTheOptOutIsNotTaken )
{
   Frm_Help dialog;

   QTimer::singleShot( 0, &dialog, &QDialog::reject );
   EXPECT_TRUE( dialog.Execute( true, true ) );
}

}  // namespace
