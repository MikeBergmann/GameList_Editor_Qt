#pragma once

#include "Game.h"
#include "Result.h"

#include <QPixmap>
#include <QVector>
#include <QWidget>

#include <functional>

class Gamelist;
class QComboBox;
class QLabel;
class QHideEvent;
class QLineEdit;
class QMediaPlayer;
class QPlainTextEdit;
class QPushButton;
class QShowEvent;
class QTabWidget;

// The 12 editable fields, the picture and the media buttons. Addresses games by
// index. The dirty flag compares the widgets' GameFields against what is stored.
class GameEditPanel : public QWidget
{
   Q_OBJECT

public:
   explicit GameEditPanel( QWidget* aParent = nullptr );

   // Borrowed, not owned. Null means no system is loaded.
   void setGamelist( Gamelist* aGamelist );

   // More than one index is batch mode: fields start blank and mean "set on all".
   void setSelection( const QVector<int>& aIndices );
   const QVector<int>& selection() const { return FSelection; }

   GameFields fields() const;
   void setFields( const GameFields& aFields );

   bool isDirty() const;

   // Asked before an action that would drop typed edits; false cancels. Unset: go ahead.
   void setLeaveGuard( std::function<bool()> aGuard ) { FLeaveGuard = std::move( aGuard ); }

   // Reloads the fields from the Gamelist afterwards. False if the write failed;
   // the edit then stays dirty.
   bool save();

   // aSkipPrompt drops the delete confirmation.
   void setGodMode( bool aEnabled, bool aSkipPrompt );

   // Still muted unless the Mute box is unticked.
   void setAutoplay( bool aEnabled ) { FAutoplay = aEnabled; }

signals:
   void dirtyChanged( bool aDirty );

   void gamesChanged();

   void moreInfosRequested( int aIndex );

   // Later indices shifted, so the list view must reset. aIndex is where it was.
   void gameDeleted( int aIndex );

protected:
   bool eventFilter( QObject* aWatched, QEvent* aEvent ) override;

   // The player holds the video file only while the panel is visible, so the
   // scrape tab can replace it.
   void showEvent( QShowEvent* aEvent ) override;
   void hideEvent( QHideEvent* aEvent ) override;

private:
   void buildLayout();
   void loadGame( int aIndex );
   void updateDirty();
   void enableComponents();
   void showPicture( int aIndex );
   void scalePicture();
   void reload();

   // Only loads while the Video tab is showing.
   void syncVideo();
   void updateVideoTab( bool aHasVideo );

   void chooseImage();
   void chooseVideo();
   void setDefaultPicture();
   void setDefaultPictureForAll();
   void deleteGame();
   void report( const Result<>& aResult );
   bool mayLeave() const { return !FLeaveGuard || FLeaveGuard(); }

   std::function<bool()> FLeaveGuard;
   Gamelist* FGamelist = nullptr;
   QVector<int> FSelection;

   // Filling the widgets must not look like the user typing.
   bool FIsLoading = false;
   bool FDirty = false;
   bool FGodMode = false;
   bool FSkipDeletePrompt = false;
   bool FAutoplay = false;

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
   QTabWidget* Tbs_Media = nullptr;  // Image | Video
   QMediaPlayer* FPlayer = nullptr;
   QPushButton* Btn_PlayVideo = nullptr;
   QLabel* Lbl_VideoStatus = nullptr;
   bool FPreferVideo = false;  // the user's last tab choice, kept across games
   bool FSyncingTabs = false;  // tab changes made by the panel, not the user
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
