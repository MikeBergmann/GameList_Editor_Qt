// Frm_Editor, driven the way a user would drive it: point it at a ROM root and
// then work the widgets that decide what the list shows and what the editor holds.

#include "F_Main.h"
#include "GamelistFixture.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
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

// Picks the combo item by what it filters, not by where it sits in the list.
void select( QComboBox* aFilter, FilterCategory aCategory )
{
   const int item = aFilter->findData( static_cast<int>( aCategory ) );
   ASSERT_GE( item, 0 );
   aFilter->setCurrentIndex( item );
}

TEST( MainWindow, ConstructsWithNothingLoaded )
{
   Frm_Editor editor;

   EXPECT_FALSE( editor.windowTitle().isEmpty() );
   EXPECT_EQ( editor.menuBar()->actions().size(), 4 );

   // No folder chosen yet: the system and filter combos stay out of reach.
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

TEST( MainWindow, AMappedFolderIsTreatedAsTheKnownSystem )
{
   Fixture fixture;
   writeFile( fixture.path( "amiga500/gamelist.xml" ), Cst_Gamelist );
   Frm_Editor editor;
   QComboBox* systems = editor.findChild<QComboBox*>( QStringLiteral( "Cbx_Systems" ) );

   editor.openRootFolder( fixture.dir.path() );
   ASSERT_EQ( systems->itemText( 0 ), QStringLiteral( "amiga500" ) );  // unknown: its own name
   EXPECT_EQ( systems->itemData( 0 ).value<SystemEntry>().kind, skOther );

   QSettings settings;
   settings.setValue( QStringLiteral( "SystemMap/amiga500" ), QStringLiteral( "amiga" ) );
   editor.openRootFolder( fixture.dir.path() );
   settings.remove( QStringLiteral( "SystemMap" ) );

   EXPECT_EQ( systems->itemText( 0 ), QStringLiteral( "Amiga (amiga500)" ) );
   EXPECT_EQ( systems->itemData( 0 ).value<SystemEntry>().kind, skAM );
}

TEST( MainWindow, TheFilterComboOffersEveryCategoryOnceWithAName )
{
   Frm_Editor editor;
   QComboBox* filter = editor.findChild<QComboBox*>( QStringLiteral( "Cbx_Filter" ) );

   ASSERT_EQ( filter->count(), static_cast<int>( FilterCategory::Count ) );
   for ( int item = 0; item < filter->count(); ++item ) {
      EXPECT_EQ( filter->itemData( item ).toInt(), item );
      EXPECT_FALSE( filter->itemText( item ).isEmpty() ) << item;
   }
   EXPECT_EQ( filter->itemText( static_cast<int>( FilterCategory::Orphan ) ),
              QStringLiteral( "Orphan" ) );
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

   select( filter, FilterCategory::Orphan );
   EXPECT_EQ( shown( editor ), QStringList( { QStringLiteral( "Vanished" ) } ) );
   // Off "All", the count says how much of the system is being hidden.
   EXPECT_EQ( editor.findChild<QLabel*>( QStringLiteral( "Lbl_NbGamesFound" ) )->text(),
              QStringLiteral( "1 / 3 game(s) found." ) );

   select( filter, FilterCategory::All );
   search->setText( QStringLiteral( "sonic" ) );
   EXPECT_EQ( shown( editor ).size(), 2 );

   // Show full Rom name is meaningless until the list is by ROM, so it is only
   // reachable then.
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

// The "same as" categories compare against the selected game, so the list has to
// re-filter when the selection moves.
TEST( MainWindow, SameAsSelectedFollowsTheSelection )
{
   Fixture fixture;
   Frm_Editor editor;
   editor.openRootFolder( fixture.dir.path() );

   QListView* games = editor.findChild<QListView*>( QStringLiteral( "Lbx_Games" ) );
   QComboBox* filter = editor.findChild<QComboBox*>( QStringLiteral( "Cbx_Filter" ) );

   // The first game is selected on load, and it is the only one in "wor".
   select( filter, FilterCategory::SameRegion );
   EXPECT_EQ( shown( editor ), QStringList( { QStringLiteral( "Sonic The Hedgehog" ) } ) );

   // Select the game in ./sub, then ask for everything in its folder: it is the
   // only one there, so the answer has to follow the selection, not the load.
   select( filter, FilterCategory::All );
   games->setCurrentIndex( games->model()->index( 1, 0 ) );

   select( filter, FilterCategory::SameFolder );
   EXPECT_EQ( shown( editor ), QStringList( { QStringLiteral( "Sonic The Hedgehog" ) } ) );

   // Back to a game in the system root, and the other two come back with it.
   select( filter, FilterCategory::All );
   games->setCurrentIndex( games->model()->index( 0, 0 ) );
   select( filter, FilterCategory::SameFolder );
   EXPECT_EQ( shown( editor ).size(), 2 );
}

// The editor follows the selection, and the list follows a save.
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
   games->setCurrentIndex( games->model()->index( 2, 0 ) );  // sorted last
   EXPECT_EQ( name->text(), QStringLiteral( "Vanished" ) );

   QPushButton* save = editor.findChild<QPushButton*>( QStringLiteral( "Btn_SaveChanges" ) );
   ASSERT_FALSE( save->isEnabled() );

   name->setText( QStringLiteral( "Found" ) );
   ASSERT_TRUE( save->isEnabled() );
   save->click();

   // The list is sorted, so "Found" moves up ahead of the two Sonics.
   EXPECT_EQ( shown( editor ).at( 0 ), QStringLiteral( "Found" ) );
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

QAction* findAction( const Frm_Editor& aEditor, const QString& aText )
{
   for ( QAction* action : aEditor.findChildren<QAction*>() )
      if ( action->text() == aText )
         return action;
   return nullptr;
}

// Answers the next modal message box on the next event-loop turn.
void answerNextBox( QMessageBox::StandardButton aButton )
{
   QTimer::singleShot( 0, [aButton] {
      if ( auto* box = qobject_cast<QMessageBox*>( QApplication::activeModalWidget() ) )
         box->button( aButton )->click();
   } );
}

// A system command runs through Frm_Editor::runAction: the games go, the file is
// saved, and the views start over on what is left.
TEST( MainWindow, DeleteOrphansDropsTheGameSavesAndResetsTheViews )
{
   Fixture fixture;
   Frm_Editor editor;
   editor.openRootFolder( fixture.dir.path() );
   editor.findChild<QComboBox*>( QStringLiteral( "Cbx_Systems" ) )->setCurrentIndex( 1 );  // snes

   ASSERT_EQ( shown( editor ).size(), 3 );
   QAction* action = findAction( editor, QStringLiteral( "Delete orphans from gamelist..." ) );
   ASSERT_NE( action, nullptr );

   answerNextBox( QMessageBox::Yes );
   action->trigger();

   EXPECT_EQ( shown( editor ).size(), 2 );
   EXPECT_EQ( reload( fixture ).count(), 2 );
   EXPECT_EQ( editor.findChild<QLabel*>( QStringLiteral( "Lbl_NbGamesFound" ) )->text(),
              QStringLiteral( "2 game(s) found." ) );
   // The first row is selected again, so the editor shows a game, not a stale one.
   EXPECT_EQ( editor.findChild<QListView*>( QStringLiteral( "Lbx_Games" ) )->currentIndex().row(), 0 );
}

// Saying No leaves the gamelist alone.
TEST( MainWindow, DecliningADeleteChangesNothing )
{
   Fixture fixture;
   Frm_Editor editor;
   editor.openRootFolder( fixture.dir.path() );
   editor.findChild<QComboBox*>( QStringLiteral( "Cbx_Systems" ) )->setCurrentIndex( 1 );  // snes

   answerNextBox( QMessageBox::No );
   findAction( editor, QStringLiteral( "Delete orphans from gamelist..." ) )->trigger();

   EXPECT_EQ( shown( editor ).size(), 3 );
   EXPECT_EQ( reload( fixture ).count(), 3 );
}

// Every command that works on the loaded system follows one rule, including after a
// folder with no systems replaces the loaded one.
TEST( MainWindow, SystemCommandsAreOnlyEnabledWhileASystemIsLoaded )
{
   Fixture fixture;
   QTemporaryDir empty;
   Frm_Editor editor;

   const QStringList commands = { QStringLiteral( "Add missing ROMs..." ),
                                  QStringLiteral( "Link existing media..." ),
                                  QStringLiteral( "Rename media after ROMs..." ),
                                  QStringLiteral( "Shrink large pictures..." ),
                                  QStringLiteral( "Set system type..." ),
                                  QStringLiteral( "Delete orphans from gamelist..." ),
                                  QStringLiteral( "Delete duplicates from gamelist..." ),
                                  QStringLiteral( "Export games list to txt file..." ) };
   const auto expectAll = [&]( bool aEnabled ) {
      for ( const QString& text : commands ) {
         QAction* action = findAction( editor, text );
         ASSERT_NE( action, nullptr ) << qPrintable( text );
         EXPECT_EQ( action->isEnabled(), aEnabled ) << qPrintable( text );
      }
   };

   expectAll( false );
   editor.openRootFolder( fixture.dir.path() );
   expectAll( true );

   answerNextBox( QMessageBox::Ok );  // "you selected the wrong folder"
   editor.openRootFolder( empty.path() );
   expectAll( false );
}

}  // namespace
