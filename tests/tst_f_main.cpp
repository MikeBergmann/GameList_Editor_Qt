// Frm_Editor, driven the way a user would drive it: point it at a ROM root and
// then work the four widgets that decide what the list shows. This is the chain
// - scan, load, filter, display - that step 4.2 exists to prove.

#include "F_Main.h"
#include "GamelistFixture.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

namespace {

QStringList shown( const Frm_Editor& aEditor )
{
   const QAbstractItemModel* model =
      aEditor.findChild<QListView*>( QStringLiteral( "Lbx_Games" ) )->model();

   QStringList rows;
   for ( int row = 0; row < model->rowCount(); ++row )
      rows.append( model->index( row, 0 ).data().toString() );
   return rows;
}

TEST( MainWindow, ConstructsWithNothingLoaded )
{
   Frm_Editor editor;

   EXPECT_FALSE( editor.windowTitle().isEmpty() );
   EXPECT_EQ( editor.menuBar()->actions().size(), 4 );

   // No folder chosen yet: the system and filter combos stay out of reach, as
   // BuildSystemsList's opening block did by hand.
   EXPECT_FALSE( editor.findChild<QComboBox*>( QStringLiteral( "Cbx_Systems" ) )->isEnabled() );
   EXPECT_TRUE( shown( editor ).isEmpty() );
}

TEST( MainWindow, ListsTheGamesOfTheSelectedSystem )
{
   Fixture fixture;
   Frm_Editor editor;

   editor.openRootFolder( fixture.dir.path() );

   QComboBox* systems = editor.findChild<QComboBox*>( QStringLiteral( "Cbx_Systems" ) );
   ASSERT_EQ( systems->count(), 2 );
   EXPECT_TRUE( systems->isEnabled() );
   // Folder names, resolved through the tables: "megadrive" and "snes".
   EXPECT_EQ( systems->itemText( 0 ), QStringLiteral( "MegaDrive" ) );

   EXPECT_EQ( shown( editor ).size(), 3 );
   EXPECT_EQ( editor.findChild<QLabel*>( QStringLiteral( "Lbl_NbGamesFound" ) )->text(),
              QStringLiteral( "3 game(s) found." ) );

   // Switching systems reloads: the other folder holds the same gamelist.
   systems->setCurrentIndex( 1 );
   EXPECT_EQ( shown( editor ).size(), 3 );
}

TEST( MainWindow, TheFourListWidgetsDriveTheFilter )
{
   Fixture fixture;
   Frm_Editor editor;
   editor.openRootFolder( fixture.dir.path() );

   QComboBox* filter = editor.findChild<QComboBox*>( QStringLiteral( "Cbx_Filter" ) );
   QLineEdit* search = editor.findChild<QLineEdit*>( QStringLiteral( "Edt_Search" ) );
   QCheckBox* listByRom = editor.findChild<QCheckBox*>( QStringLiteral( "Chk_ListByRom" ) );
   QCheckBox* fullRomName = editor.findChild<QCheckBox*>( QStringLiteral( "Chk_FullRomName" ) );

   // "snes" is the system whose ROMs are actually on disk; "megadrive" holds the
   // same gamelist with nothing behind it, so there every game is an orphan.
   editor.findChild<QComboBox*>( QStringLiteral( "Cbx_Systems" ) )->setCurrentIndex( 1 );

   filter->setCurrentIndex( 14 );  // orphan
   EXPECT_EQ( shown( editor ), QStringList( { QStringLiteral( "Vanished" ) } ) );
   // Off "All", the count says how much of the system is being hidden.
   EXPECT_EQ( editor.findChild<QLabel*>( QStringLiteral( "Lbl_NbGamesFound" ) )->text(),
              QStringLiteral( "1 / 3 game(s) found." ) );

   filter->setCurrentIndex( 0 );
   search->setText( QStringLiteral( "sonic" ) );
   EXPECT_EQ( shown( editor ).size(), 2 );

   // Show full Rom name is meaningless until the list is by ROM, so it is only
   // reachable then - which is the one thing Chk_ListByRomClick did besides
   // rebuilding the list.
   EXPECT_FALSE( fullRomName->isEnabled() );
   listByRom->setChecked( true );
   EXPECT_TRUE( fullRomName->isEnabled() );
   EXPECT_EQ( shown( editor ), QStringList( { QStringLiteral( "Sonic.zip" ),
                                               QStringLiteral( "Sonic.zip" ) } ) );

   fullRomName->setChecked( true );
   EXPECT_EQ( shown( editor ), QStringList( { QStringLiteral( "Sonic.zip" ),
                                               QStringLiteral( "sub/Sonic.zip" ) } ) );

   // A search that only the ROM path can satisfy, to prove the display string
   // and the searched string are the same string.
   search->setText( QStringLiteral( "sub/" ) );
   EXPECT_EQ( shown( editor ), QStringList( { QStringLiteral( "sub/Sonic.zip" ) } ) );
}

// Filters 15-28 compare against the selected game, so the list has to re-filter
// when the selection moves. The Delphi only got this right by accident, by
// reading the reference out of the listbox before it cleared it.
TEST( MainWindow, SameAsSelectedFollowsTheSelection )
{
   Fixture fixture;
   Frm_Editor editor;
   editor.openRootFolder( fixture.dir.path() );

   QListView* games = editor.findChild<QListView*>( QStringLiteral( "Lbx_Games" ) );
   QComboBox* filter = editor.findChild<QComboBox*>( QStringLiteral( "Cbx_Filter" ) );

   // The first game is selected on load, and it is the only one in "wor".
   filter->setCurrentIndex( 15 );  // same region
   EXPECT_EQ( shown( editor ), QStringList( { QStringLiteral( "Sonic The Hedgehog" ) } ) );

   // Select the game in ./sub, then ask for everything in its folder: it is the
   // only one there, so the answer has to follow the selection, not the load.
   filter->setCurrentIndex( 0 );
   games->setCurrentIndex( games->model()->index( 1, 0 ) );

   filter->setCurrentIndex( 26 );  // same folder
   EXPECT_EQ( shown( editor ), QStringList( { QStringLiteral( "Sonic The Hedgehog" ) } ) );

   // Back to a game in the system root, and the other two come back with it.
   filter->setCurrentIndex( 0 );
   games->setCurrentIndex( games->model()->index( 0, 0 ) );
   filter->setCurrentIndex( 26 );
   EXPECT_EQ( shown( editor ).size(), 2 );
}

// The two halves of step 4.3's wiring: the editor follows the selection, and the
// list follows a save. The Delphi had the listbox itself hold the TGame and the
// new name written into Items[] by hand, from inside the XML write.
TEST( MainWindow, TheEditorFollowsTheSelectionAndTheListFollowsTheEditor )
{
   Fixture fixture;
   Frm_Editor editor;
   editor.openRootFolder( fixture.dir.path() );

   QLineEdit* name = editor.findChild<QLineEdit*>( QStringLiteral( "Edt_Name" ) );
   ASSERT_NE( name, nullptr );
   // The first game is selected on load, so the editor is showing it.
   EXPECT_EQ( name->text(), QStringLiteral( "Sonic The Hedgehog" ) );

   QListView* games = editor.findChild<QListView*>( QStringLiteral( "Lbx_Games" ) );
   games->setCurrentIndex( games->model()->index( 2, 0 ) );
   EXPECT_EQ( name->text(), QStringLiteral( "Vanished" ) );

   QPushButton* save = editor.findChild<QPushButton*>( QStringLiteral( "Btn_SaveChanges" ) );
   ASSERT_FALSE( save->isEnabled() );

   name->setText( QStringLiteral( "Found" ) );
   ASSERT_TRUE( save->isEnabled() );
   save->click();

   EXPECT_EQ( shown( editor ).at( 2 ), QStringLiteral( "Found" ) );
   EXPECT_FALSE( save->isEnabled() );
}

// Moving to another game with an unsaved edit asks first, and Cancel puts the
// list back where it was with the edit still on screen.
TEST( MainWindow, CancellingTheUnsavedChangesPromptKeepsTheEditAndTheSelection )
{
   Fixture fixture;
   Frm_Editor editor;
   editor.openRootFolder( fixture.dir.path() );

   QLineEdit* name = editor.findChild<QLineEdit*>( QStringLiteral( "Edt_Name" ) );
   QListView* games = editor.findChild<QListView*>( QStringLiteral( "Lbx_Games" ) );
   name->setText( QStringLiteral( "Edited" ) );

   bool asked = false;
   QTimer::singleShot( 0, [&asked] {
      if ( auto* box = qobject_cast<QMessageBox*>( QApplication::activeModalWidget() ) ) {
         asked = true;
         box->button( QMessageBox::Cancel )->click();
      }
   } );
   games->setCurrentIndex( games->model()->index( 2, 0 ) );

   EXPECT_TRUE( asked );
   EXPECT_EQ( name->text(), QStringLiteral( "Edited" ) );
   EXPECT_EQ( games->currentIndex().row(), 0 );
   EXPECT_TRUE( games->selectionModel()->isRowSelected( 0 ) );
   EXPECT_FALSE( games->selectionModel()->isRowSelected( 2 ) );
}

}  // namespace
