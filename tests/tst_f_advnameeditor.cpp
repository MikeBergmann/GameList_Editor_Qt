// The transformation behind the Advanced Name Editor. Worth its own checks
// because the Delphi had two copies of it - the dialog's live preview and
// F_Main.TransformGamesNames - and because both of them could be driven out of
// bounds from the UI.

#include "NameEdit.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

namespace {

const QString Cst_Name = QStringLiteral( "[EU] Sonic The Hedgehog 2 (v1)" );

TEST( NameEdit, DoesNothingWhenEveryGroupIsOff )
{
   EXPECT_EQ( applyNameEdit( Cst_Name, NameEdit() ), Cst_Name );
}

TEST( NameEdit, RemovesCharactersFromEitherEnd )
{
   NameEdit edit;
   edit.removeChars = true;
   edit.nbStart = 5;
   EXPECT_EQ( applyNameEdit( Cst_Name, edit ), QStringLiteral( "Sonic The Hedgehog 2 (v1)" ) );

   edit.nbEnd = 5;
   EXPECT_EQ( applyNameEdit( Cst_Name, edit ), QStringLiteral( "Sonic The Hedgehog 2" ) );

   // The flag gates both fields, as the checkbox did.
   edit.removeChars = false;
   EXPECT_EQ( applyNameEdit( Cst_Name, edit ), Cst_Name );
}

// The Delphi's Copy/SetLength pair was unguarded: removing more than the name
// holds called SetLength with a negative length. Reachable by typing 99 into a
// field, so it has to clamp to empty instead.
TEST( NameEdit, ClampsRemovalToTheLengthOfTheName )
{
   NameEdit edit;
   edit.removeChars = true;

   edit.nbStart = 999;
   EXPECT_EQ( applyNameEdit( Cst_Name, edit ), QString() );

   edit.nbStart = 0;
   edit.nbEnd = 999;
   EXPECT_EQ( applyNameEdit( Cst_Name, edit ), QString() );

   edit.nbStart = 999;
   edit.nbEnd = 999;
   EXPECT_EQ( applyNameEdit( Cst_Name, edit ), QString() );

   EXPECT_EQ( applyNameEdit( QString(), edit ), QString() );
}

TEST( NameEdit, ChangesCase )
{
   NameEdit edit;
   edit.changeCase = true;

   edit.caseIndex = 0;
   EXPECT_EQ( applyNameEdit( QStringLiteral( "sonic" ), edit ), QStringLiteral( "Sonic" ) );

   edit.caseIndex = 1;
   EXPECT_EQ( applyNameEdit( QStringLiteral( "sonic" ), edit ), QStringLiteral( "SONIC" ) );

   edit.caseIndex = 2;
   EXPECT_EQ( applyNameEdit( QStringLiteral( "SONIC" ), edit ), QStringLiteral( "sonic" ) );
}

// Chk_Case could be ticked with no radio selected, and the Delphi's case
// statement then fell through. Keep that: the group is on, nothing is picked,
// nothing happens.
TEST( NameEdit, LeavesCaseAloneWhenNoOptionIsPicked )
{
   NameEdit edit;
   edit.changeCase = true;
   edit.caseIndex = -1;

   EXPECT_EQ( applyNameEdit( Cst_Name, edit ), Cst_Name );
}

// The Delphi wrote TmpStr[1] without checking, which on an empty string is a
// write past the end - and "delete everything, then capitalise" gets there.
TEST( NameEdit, CapitalisingAnEmptyNameIsSafe )
{
   NameEdit edit;
   edit.removeChars = true;
   edit.nbStart = 999;
   edit.changeCase = true;
   edit.caseIndex = 0;

   EXPECT_EQ( applyNameEdit( Cst_Name, edit ), QString() );
   EXPECT_EQ( applyNameEdit( QString(), edit ), QString() );
}

TEST( NameEdit, AddsFixedTextAtEitherEnd )
{
   NameEdit edit;
   edit.addChars = true;
   edit.startString = QStringLiteral( ">> " );
   edit.endString = QStringLiteral( " <<" );

   EXPECT_EQ( applyNameEdit( QStringLiteral( "Sonic" ), edit ), QStringLiteral( ">> Sonic <<" ) );

   edit.startString.clear();
   EXPECT_EQ( applyNameEdit( QStringLiteral( "Sonic" ), edit ), QStringLiteral( "Sonic <<" ) );
}

// Order matters: remove, then case, then add. Adding first would let the
// removal eat the text that was just added.
TEST( NameEdit, AppliesRemoveThenCaseThenAdd )
{
   NameEdit edit;
   edit.removeChars = true;
   edit.nbStart = 5;
   edit.changeCase = true;
   edit.caseIndex = 1;
   edit.addChars = true;
   edit.startString = QStringLiteral( "eu " );

   EXPECT_EQ( applyNameEdit( QStringLiteral( "[EU] sonic" ), edit ), QStringLiteral( "eu SONIC" ) );
}

}  // namespace
