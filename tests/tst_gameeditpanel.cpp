// GameEditPanel: the dirty flag and the two save paths it chooses between.
//
// CheckIfChangesToSave compared 12 widgets against the TGame on every keystroke
// and left the answer in Btn_SaveChanges.Enabled, so none of it could be checked
// without a window. It is a comparison of two GameFields values now.

#include "GameEditPanel.h"
#include "GamelistFixture.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>

namespace {

// The panel edits a Gamelist by index, so both come together.
struct Panel
{
   Fixture fixture;
   Gamelist list;
   GameEditPanel panel;

   Panel()
   {
      EXPECT_TRUE( list.load( fixture.snes ) );
      panel.setGamelist( &list );
   }

   QLineEdit* edit( const char* aName ) const
   {
      return panel.findChild<QLineEdit*>( QLatin1String( aName ) );
   }

   QPushButton* button( const char* aName ) const
   {
      return panel.findChild<QPushButton*>( QLatin1String( aName ) );
   }
};

TEST( GameEditPanel, LoadsTheSelectedGameAndIsNotDirtyForIt )
{
   Panel panel;
   panel.panel.setSelection( { 0 } );

   EXPECT_EQ( panel.edit( "Edt_Name" )->text(), QStringLiteral( "Sonic The Hedgehog" ) );
   EXPECT_EQ( panel.edit( "Edt_ReleaseDate" )->text(), QStringLiteral( "23/06/1991" ) );
   EXPECT_EQ( panel.edit( "Edt_RomPath" )->text(), QStringLiteral( "./Sonic.zip" ) );
   EXPECT_EQ( panel.panel.fields().favorite, 1 );

   // Loading is not editing: FIsLoading was form-wide state for exactly this.
   EXPECT_FALSE( panel.panel.isDirty() );
   EXPECT_FALSE( panel.button( "Btn_SaveChanges" )->isEnabled() );
}

TEST( GameEditPanel, TypingMakesItDirtyAndSavingClearsIt )
{
   Panel panel;
   panel.panel.setSelection( { 0 } );

   // QSignalSpy lives in Qt6::Test, which this build does not link for one
   // counter; the counter is the counter.
   int dirtyChanges = 0;
   bool lastDirty = false;
   QObject::connect( &panel.panel, &GameEditPanel::dirtyChanged, [&]( bool aDirty ) {
      ++dirtyChanges;
      lastDirty = aDirty;
   } );
   int listChanges = 0;
   QObject::connect( &panel.panel, &GameEditPanel::gamesChanged, [&] { ++listChanges; } );

   panel.edit( "Edt_Genre" )->setText( QStringLiteral( "Platformer" ) );

   EXPECT_TRUE( panel.panel.isDirty() );
   EXPECT_TRUE( panel.button( "Btn_SaveChanges" )->isEnabled() );
   EXPECT_EQ( dirtyChanges, 1 );
   EXPECT_TRUE( lastDirty );

   panel.panel.save();

   EXPECT_EQ( panel.list.at( 0 ).genre, QStringLiteral( "Platformer" ) );
   EXPECT_EQ( listChanges, 1 );
   EXPECT_FALSE( panel.panel.isDirty() );
   EXPECT_FALSE( lastDirty );
   EXPECT_FALSE( panel.button( "Btn_SaveChanges" )->isEnabled() );

   // And it reached the file, not just the vector.
   Gamelist saved;
   ASSERT_TRUE( saved.load( panel.fixture.snes ) );
   EXPECT_EQ( saved.at( 0 ).genre, QStringLiteral( "Platformer" ) );
}

TEST( GameEditPanel, DeleteGameNeedsGodModeAndDropsUnsavedEdits )
{
   Panel panel;
   panel.panel.setSelection( { 0 } );
   EXPECT_TRUE( panel.button( "Btn_DeleteGame" )->isHidden() );

   // Prompt skipped, so the click goes straight through.
   panel.panel.setGodMode( true, true );
   EXPECT_FALSE( panel.button( "Btn_DeleteGame" )->isHidden() );

   panel.edit( "Edt_Genre" )->setText( QStringLiteral( "Platformer" ) );
   int deleted = -1;
   QObject::connect( &panel.panel, &GameEditPanel::gameDeleted, [&]( int aIndex ) { deleted = aIndex; } );

   panel.button( "Btn_DeleteGame" )->click();

   EXPECT_EQ( deleted, 0 );
   EXPECT_EQ( panel.list.count(), 2 );
   EXPECT_FALSE( panel.panel.isDirty() );
   EXPECT_TRUE( panel.panel.selection().isEmpty() );

   // Switching God Mode off hides the button again, and a skip never outlives it.
   panel.panel.setGodMode( false, true );
   EXPECT_TRUE( panel.button( "Btn_DeleteGame" )->isHidden() );
}

// A failed write must not leave the edit in the Gamelist only: the panel would
// then compare clean against it, and closing would lose it without asking.
TEST( GameEditPanel, AFailedSaveKeepsTheEditAndLeavesTheGamelistAlone )
{
   Panel panel;
   panel.panel.setSelection( { 0 } );
   panel.edit( "Edt_Genre" )->setText( QStringLiteral( "Platformer" ) );

   // No folder, no write - and unlike permissions, this holds when run as root.
   ASSERT_TRUE( QDir( panel.fixture.path( "snes" ) ).removeRecursively() );

   QTimer::singleShot( 0, [] {
      if ( QWidget* box = QApplication::activeModalWidget() )
         box->close();
   } );
   EXPECT_FALSE( panel.panel.save() );

   EXPECT_EQ( panel.list.at( 0 ).genre, QStringLiteral( "Platform" ) );
   EXPECT_EQ( panel.edit( "Edt_Genre" )->text(), QStringLiteral( "Platformer" ) );
   EXPECT_TRUE( panel.panel.isDirty() );
}

// Several games: the fields start blank and only the filled-in ones are written,
// which is the whole difference between SaveChangesToGamelist and its batch twin.
TEST( GameEditPanel, SeveralGamesStartBlankAndWriteOnlyWhatIsFilledIn )
{
   Panel panel;
   panel.panel.setSelection( { 0, 1, 2 } );

   EXPECT_TRUE( panel.edit( "Edt_Genre" )->text().isEmpty() );
   EXPECT_EQ( panel.panel.fields().favorite, -1 );
   EXPECT_FALSE( panel.panel.isDirty() );

   // The name is one game's own, so it is not on offer for a batch.
   EXPECT_FALSE( panel.edit( "Edt_Name" )->isEnabled() );
   EXPECT_TRUE( panel.edit( "Edt_Genre" )->isEnabled() );
   EXPECT_FALSE( panel.button( "Btn_ChangeImage" )->isEnabled() );

   panel.edit( "Edt_Developer" )->setText( QStringLiteral( "Sega AM7" ) );
   EXPECT_TRUE( panel.panel.isDirty() );
   panel.panel.save();

   for ( int index = 0; index < panel.list.count(); ++index )
      EXPECT_EQ( panel.list.at( index ).developer, QStringLiteral( "Sega AM7" ) );

   // Untouched fields stayed as they were on each game, rather than being
   // flattened to the blank the editor was showing.
   EXPECT_EQ( panel.list.at( 0 ).name, QStringLiteral( "Sonic The Hedgehog" ) );
   EXPECT_EQ( panel.list.at( 0 ).description, QStringLiteral( "Gotta go fast" ) );
   EXPECT_EQ( panel.list.at( 1 ).kidGame, 1 );
}

TEST( GameEditPanel, NoSelectionMeansNothingToEditAndNothingToSave )
{
   Panel panel;
   panel.panel.setSelection( { 0 } );
   panel.panel.setSelection( {} );

   EXPECT_TRUE( panel.edit( "Edt_Name" )->text().isEmpty() );
   EXPECT_FALSE( panel.edit( "Edt_Genre" )->isEnabled() );
   EXPECT_FALSE( panel.panel.isDirty() );
   EXPECT_FALSE( panel.button( "Btn_MoreInfos" )->isEnabled() );

   // Nothing to write to, and no crash for asking.
   panel.panel.save();
}

// The three Delete buttons are the only ones that are not simply "is one game
// selected": they are also "is there anything to delete".
TEST( GameEditPanel, TheDeleteMediaButtonsFollowWhatTheGameHas )
{
   Panel panel;

   panel.panel.setSelection( { 0 } );  // picture and video both on disk
   EXPECT_TRUE( panel.button( "Btn_RemovePicture" )->isEnabled() );
   EXPECT_TRUE( panel.button( "Btn_RemoveVideo" )->isEnabled() );

   panel.panel.setSelection( { 1 } );  // <image> points at a file that is gone
   EXPECT_FALSE( panel.button( "Btn_RemovePicture" )->isEnabled() );
   EXPECT_FALSE( panel.button( "Btn_RemoveVideo" )->isEnabled() );
}

}  // namespace
