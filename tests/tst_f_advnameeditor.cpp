// The name transformation behind the Advanced Name Editor, including the
// out-of-range values the UI can feed it.

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

   // The flag gates both fields.
   edit.removeChars = false;
   EXPECT_EQ( applyNameEdit( Cst_Name, edit ), Cst_Name );
}

// Removing more characters than the name holds (easy to type into the field) clamps
// to an empty name.
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

// The case group can be on with no option picked; that leaves the name alone.
TEST( NameEdit, LeavesCaseAloneWhenNoOptionIsPicked )
{
   NameEdit edit;
   edit.changeCase = true;
   edit.caseIndex = -1;

   EXPECT_EQ( applyNameEdit( Cst_Name, edit ), Cst_Name );
}

// "Delete everything, then capitalise" must not index into an empty name.
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
