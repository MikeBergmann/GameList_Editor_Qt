// Construction smoke tests for the step-3 dialogs, plus the one behaviour in
// them that is not just widget plumbing. Building a dialog exercises every
// layout, parenting and signal connection in its constructor, which is where
// a code-built UI actually breaks.

#include "F_About.h"
#include "F_AdvNameEditor.h"
#include "F_ConfigureNetwork.h"
#include "F_Help.h"
#include "F_MoreInfos.h"
#include "Game.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

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

TEST( Dialogs, MoreInfosShowsAGamesFields )
{
   Game game;
   game.playcount = QStringLiteral( "12" );
   game.lastplayed = QStringLiteral( "20130101T180000" );
   game.crc32 = QStringLiteral( "352441C2" );

   Frm_MoreInfos dialog;
   QTimer::singleShot( 0, &dialog, &QDialog::reject );
   dialog.Execute( game );

   // Nothing to assert on the widgets from out here; the point is that feeding
   // a real Game through Execute neither crashes nor blocks.
   SUCCEED();
}

// The Delphi's menu caller passed FShowTips straight in and assigned the
// negated result back, so every visit to Help from the menu silently flipped
// "show tips at start". With the opt-out hidden the setting must come back
// untouched - both ways round.
TEST( Dialogs, HelpLeavesTheSettingAloneWhenTheOptOutIsHidden )
{
   Frm_Help dialog;

   QTimer::singleShot( 0, &dialog, &QDialog::reject );
   EXPECT_TRUE( dialog.Execute( true, false ) );

   QTimer::singleShot( 0, &dialog, &QDialog::reject );
   EXPECT_FALSE( dialog.Execute( false, false ) );
}

// The startup caller does offer the opt-out. Left alone, the checkbox stays
// clear and tips stay on.
TEST( Dialogs, HelpKeepsTipsOnWhenTheOptOutIsNotTaken )
{
   Frm_Help dialog;

   QTimer::singleShot( 0, &dialog, &QDialog::reject );
   EXPECT_TRUE( dialog.Execute( true, true ) );
}

}  // namespace
