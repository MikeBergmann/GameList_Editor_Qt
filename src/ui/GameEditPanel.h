#pragma once

#include "Game.h"

#include <QPixmap>
#include <QVector>
#include <QWidget>

class Gamelist;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

// The 12 editable fields, the picture and the media buttons - the right-hand
// half of Tbs_Main.
//
// The Delphi had no such thing: SaveChangesToGamelist read 15 widgets inline,
// CheckIfChangesToSave compared 12 of them against the TGame on every keystroke
// and stored the answer in Btn_SaveChanges.Enabled, and FIsLoading was form-wide
// state guarding the round trip. Here the widgets produce one GameFields value,
// the dirty flag is a comparison of two of those, and FIsLoading is private.
//
// It addresses games by index into the Gamelist, never by pointer, and it drives
// both save paths: one game compares against what is stored, several compare
// against "filled in at all".
class GameEditPanel : public QWidget
{
   Q_OBJECT

public:
   explicit GameEditPanel( QWidget* aParent = nullptr );

   // Borrowed, not owned. Null means no system is loaded.
   void setGamelist( Gamelist* aGamelist );

   // Indices into the Gamelist. Empty disables everything; more than one is the
   // batch mode, where the fields start blank and mean "set this on all of them".
   void setSelection( const QVector<int>& aIndices );
   const QVector<int>& selection() const { return FSelection; }

   GameFields fields() const;
   void setFields( const GameFields& aFields );

   // CheckIfChangesToSave (1039), as one comparison instead of twelve.
   bool isDirty() const;

   // Btn_SaveChangesClick (1958). Writes the gamelist and reloads the fields
   // from it, so what is on screen is what was actually stored. False if the
   // write failed: the Gamelist is then left as it was and the edit stays dirty.
   bool save();

   // God mode shows Delete Game; aSkipPrompt drops its confirmation.
   void setGodMode( bool aEnabled, bool aSkipPrompt );

signals:
   void dirtyChanged( bool aDirty );

   // The gamelist changed under the list view: names, and the fields the filter
   // looks at. The form re-runs the filter and repaints.
   void gamesChanged();

   // Btn_MoreInfos. The hash prompt reads a setting the form owns, so it stays
   // there; the panel only says which game.
   void moreInfosRequested( int aIndex );

   // A game is gone from the Gamelist and every index above it moved, so the
   // list view has to be reset, not repainted. aIndex is where it was.
   void gameDeleted( int aIndex );

protected:
   bool eventFilter( QObject* aWatched, QEvent* aEvent ) override;

private:
   void buildLayout();
   void loadGame( int aIndex );  // LoadGame (1511)
   void updateDirty();
   void enableComponents();  // EnableComponents (1400)
   void showPicture( int aIndex );
   void scalePicture();
   void reload();

   // The five picture/video buttons (1585-1815). Each one is a file dialog and a
   // call into Gamelist, which owns the gamelist document and the media folders.
   void chooseImage();
   void chooseVideo();
   void setDefaultPicture();
   void setDefaultPictureForAll();
   void deleteGame();
   void report( bool aOk, const QString& aError );

   Gamelist* FGamelist = nullptr;
   QVector<int> FSelection;

   // Guards the load: filling the widgets must not look like the user typing.
   bool FIsLoading = false;
   bool FDirty = false;
   bool FGodMode = false;
   bool FSkipDeletePrompt = false;

   QLineEdit* Edt_Name = nullptr;
   QLineEdit* Edt_RomPath = nullptr;
   QLineEdit* Edt_ReleaseDate = nullptr;
   QLineEdit* Edt_Genre = nullptr;
   QLineEdit* Edt_Region = nullptr;
   QLineEdit* Edt_NbPlayers = nullptr;
   QLineEdit* Edt_Rating = nullptr;
   QLineEdit* Edt_Developer = nullptr;
   QLineEdit* Edt_Publisher = nullptr;
   QPlainTextEdit* Mmo_Description = nullptr;
   QComboBox* Cbx_KidGame = nullptr;
   QComboBox* Cbx_Hidden = nullptr;
   QComboBox* Cbx_Favorite = nullptr;

   QLabel* Img_Game = nullptr;
   QPixmap FPicture;  // unscaled, so a resize can rescale from the original
   QPushButton* Btn_ChangeImage = nullptr;
   QPushButton* Btn_SetDefaultPicture = nullptr;
   QPushButton* Btn_ChangeAll = nullptr;
   QPushButton* Btn_RemovePicture = nullptr;
   QPushButton* Btn_ChangeVideo = nullptr;
   QPushButton* Btn_RemoveVideo = nullptr;
   QPushButton* Btn_MoreInfos = nullptr;
   QPushButton* Btn_DeleteGame = nullptr;
   QPushButton* Btn_SaveChanges = nullptr;
};
