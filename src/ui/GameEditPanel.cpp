#include "GameEditPanel.h"

#include "Gamelist.h"
#include "Resources.h"

#include <QAudioOutput>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>

namespace {

constexpr int Cst_ImageTab = 0;
constexpr int Cst_VideoTab = 1;

// The bundled generic picture. The per-system <system>-default.png lookup that
// used to come first (1621-1630, 1646-1655) is statically dead now that only the
// generic is in the .qrc, so it is not ported.
const QString Cst_DefaultPicture =
   QLatin1String( Cst_DefaultPicsFolderPath ) + QLatin1String( Cst_DefaultImageName );

// Both flag combos of the DFM, and the third one. -1 is "no answer", which is
// what several selected games show and what the batch save reads as "leave it".
QComboBox* flagCombo( QWidget* aParent, const char* aName )
{
   QComboBox* combo = new QComboBox( aParent );
   combo->setObjectName( QLatin1String( aName ) );
   combo->addItem( QStringLiteral( "No" ) );
   combo->addItem( QStringLiteral( "Yes" ) );
   combo->setCurrentIndex( -1 );
   return combo;
}

}  // namespace

GameEditPanel::GameEditPanel( QWidget* aParent ) : QWidget( aParent )
{
   buildLayout();
   setSelection( {} );
}

void GameEditPanel::buildLayout()
{
   const auto edit = [this]( const char* aName ) {
      QLineEdit* line = new QLineEdit( this );
      line->setObjectName( QLatin1String( aName ) );
      connect( line, &QLineEdit::textChanged, this, &GameEditPanel::updateDirty );
      return line;
   };

   Edt_Name = edit( "Edt_Name" );
   Edt_ReleaseDate = edit( "Edt_ReleaseDate" );
   Edt_ReleaseDate->setPlaceholderText( QStringLiteral( "dd/mm/yyyy" ) );
   Edt_ReleaseDate->setToolTip(
      QStringLiteral( "Following formats are accepted:\n\ndd/mm/yyyy\nmm/yyyy\nyyyy\n\n"
                      "Everything else will be saved as blank." ) );
   Edt_Genre = edit( "Edt_Genre" );
   Edt_Region = edit( "Edt_Region" );
   Edt_NbPlayers = edit( "Edt_NbPlayers" );
   Edt_Rating = edit( "Edt_Rating" );
   Edt_Developer = edit( "Edt_Developer" );
   Edt_Publisher = edit( "Edt_Publisher" );

   // Read-only: the ROM path identifies the game, it is not one of its fields.
   Edt_RomPath = new QLineEdit( this );
   Edt_RomPath->setObjectName( QStringLiteral( "Edt_RomPath" ) );
   Edt_RomPath->setReadOnly( true );

   Mmo_Description = new QPlainTextEdit( this );
   Mmo_Description->setObjectName( QStringLiteral( "Mmo_Description" ) );
   connect( Mmo_Description, &QPlainTextEdit::textChanged, this, &GameEditPanel::updateDirty );

   Cbx_KidGame = flagCombo( this, "Cbx_KidGame" );
   Cbx_Hidden = flagCombo( this, "Cbx_Hidden" );
   Cbx_Favorite = flagCombo( this, "Cbx_Favorite" );
   for ( QComboBox* combo : { Cbx_KidGame, Cbx_Hidden, Cbx_Favorite } )
      connect( combo, &QComboBox::currentIndexChanged, this, &GameEditPanel::updateDirty );

   QFormLayout* fields = new QFormLayout;
   fields->addRow( QStringLiteral( "Name" ), Edt_Name );
   fields->addRow( QStringLiteral( "Rom" ), Edt_RomPath );
   fields->addRow( QStringLiteral( "Date" ), Edt_ReleaseDate );
   fields->addRow( QStringLiteral( "Genre" ), Edt_Genre );
   fields->addRow( QStringLiteral( "Region" ), Edt_Region );
   fields->addRow( QStringLiteral( "Players" ), Edt_NbPlayers );
   fields->addRow( QStringLiteral( "Rating" ), Edt_Rating );
   fields->addRow( QStringLiteral( "Developer" ), Edt_Developer );
   fields->addRow( QStringLiteral( "Publisher" ), Edt_Publisher );
   fields->addRow( QStringLiteral( "Kid Game" ), Cbx_KidGame );
   fields->addRow( QStringLiteral( "Hidden" ), Cbx_Hidden );
   fields->addRow( QStringLiteral( "Favorite" ), Cbx_Favorite );

   // Img_BackGround was a picture whose only job was to say "this game has
   // none", so it is the placeholder text of the label that shows the art.
   Img_Game = new QLabel( this );
   Img_Game->setObjectName( QStringLiteral( "Img_Game" ) );
   Img_Game->setAlignment( Qt::AlignCenter );
   Img_Game->setMinimumSize( 200, 200 );
   // A QLabel's size hints follow its pixmap; Ignored stops the art from
   // driving the layout, so the label is sized by the window alone.
   Img_Game->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Ignored );
   Img_Game->setFrameShape( QFrame::StyledPanel );
   Img_Game->installEventFilter( this );

   // The video page. Muted and paused by default: clicking through the list must
   // not make noise, and the player is not even loaded until the tab is shown.
   QVideoWidget* video = new QVideoWidget( this );
   video->setMinimumSize( 200, 200 );
   video->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Ignored );

   QAudioOutput* audio = new QAudioOutput( this );
   audio->setMuted( true );
   FPlayer = new QMediaPlayer( this );
   FPlayer->setAudioOutput( audio );
   FPlayer->setVideoOutput( video );

   Btn_PlayVideo = new QPushButton( QStringLiteral( "Play" ), this );
   Btn_PlayVideo->setObjectName( QStringLiteral( "Btn_PlayVideo" ) );
   connect( Btn_PlayVideo, &QPushButton::clicked, this, [this] {
      if ( FPlayer->playbackState() == QMediaPlayer::PlayingState )
         FPlayer->pause();
      else
         FPlayer->play();
   } );
   connect( FPlayer, &QMediaPlayer::playbackStateChanged, this,
            [this]( QMediaPlayer::PlaybackState aState ) {
               Btn_PlayVideo->setText( aState == QMediaPlayer::PlayingState
                                          ? QStringLiteral( "Pause" )
                                          : QStringLiteral( "Play" ) );
            } );

   QCheckBox* mute = new QCheckBox( QStringLiteral( "Mute" ), this );
   mute->setObjectName( QStringLiteral( "Chk_MuteVideo" ) );
   mute->setChecked( true );
   connect( mute, &QCheckBox::toggled, audio, &QAudioOutput::setMuted );

   Lbl_VideoStatus = new QLabel( this );
   Lbl_VideoStatus->setObjectName( QStringLiteral( "Lbl_VideoStatus" ) );
   connect( FPlayer, &QMediaPlayer::errorOccurred, this,
            [this]( QMediaPlayer::Error, const QString& aText ) {
               Lbl_VideoStatus->setText( aText );
            } );

   QHBoxLayout* controls = new QHBoxLayout;
   controls->addWidget( Btn_PlayVideo );
   controls->addWidget( mute );
   controls->addWidget( Lbl_VideoStatus, 1 );

   QWidget* videoPage = new QWidget( this );
   QVBoxLayout* videoColumn = new QVBoxLayout( videoPage );
   videoColumn->setContentsMargins( 0, 0, 0, 0 );
   videoColumn->addWidget( video, 1 );
   videoColumn->addLayout( controls );

   Tbs_Media = new QTabWidget( this );
   Tbs_Media->setObjectName( QStringLiteral( "Tbs_Media" ) );
   Tbs_Media->addTab( Img_Game, QStringLiteral( "Image" ) );
   Tbs_Media->addTab( videoPage, QStringLiteral( "Video" ) );
   // Sized by the window alone, like the label was: Ignored drops the content's
   // own minimum too, so pin the one the pages need (the video must not spill
   // over the buttons below).
   Tbs_Media->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Ignored );
   Tbs_Media->setMinimumSize( Tbs_Media->minimumSizeHint() );
   connect( Tbs_Media, &QTabWidget::currentChanged, this, [this]( int aIndex ) {
      if ( FSyncingTabs )
         return;
      FPreferVideo = aIndex == Cst_VideoTab;
      syncVideo();
   } );

   const auto button = [this]( const char* aName, const QString& aText,
                               void ( GameEditPanel::*aSlot )() ) {
      QPushButton* push = new QPushButton( aText, this );
      push->setObjectName( QLatin1String( aName ) );
      connect( push, &QPushButton::clicked, this, aSlot );
      return push;
   };

   Btn_ChangeImage =
      button( "Btn_ChangeImage", QStringLiteral( "Change Picture" ), &GameEditPanel::chooseImage );
   Btn_SetDefaultPicture =
      button( "Btn_SetDefaultPicture", QStringLiteral( "Picture to default" ),
              &GameEditPanel::setDefaultPicture );
   Btn_ChangeAll =
      button( "Btn_ChangeAll", QStringLiteral( "All missing to default" ),
              &GameEditPanel::setDefaultPictureForAll );
   Btn_ChangeAll->setToolTip( QStringLiteral( "Change all missing pictures to default" ) );
   Btn_ChangeVideo =
      button( "Btn_ChangeVideo", QStringLiteral( "Change Video" ), &GameEditPanel::chooseVideo );

   // These delete the file from disk, so they ask first.
   const auto confirmDelete = [this]( const QString& aWhat ) {
      return QMessageBox::question(
                this, QStringLiteral( "GameList Editor" ),
                QStringLiteral( "Delete the %1 of this game?\n"
                                "The file is deleted from disk unless another game uses it." )
                   .arg( aWhat ) ) == QMessageBox::Yes;
   };

   Btn_RemovePicture = new QPushButton( QStringLiteral( "Delete Picture" ), this );
   Btn_RemovePicture->setObjectName( QStringLiteral( "Btn_RemovePicture" ) );
   connect( Btn_RemovePicture, &QPushButton::clicked, this, [this, confirmDelete] {
      if ( !confirmDelete( QStringLiteral( "picture" ) ) )
         return;
      QString error;
      report( FGamelist && FGamelist->removeImage( FSelection.value( 0, -1 ), &error ), error );
   } );

   Btn_RemoveVideo = new QPushButton( QStringLiteral( "Delete Video" ), this );
   Btn_RemoveVideo->setObjectName( QStringLiteral( "Btn_RemoveVideo" ) );
   connect( Btn_RemoveVideo, &QPushButton::clicked, this, [this, confirmDelete] {
      if ( !confirmDelete( QStringLiteral( "video" ) ) )
         return;
      FPlayer->setSource( QUrl() );  // let go of the file before it is deleted
      QString error;
      report( FGamelist && FGamelist->removeVideo( FSelection.value( 0, -1 ), &error ), error );
   } );

   Btn_MoreInfos = new QPushButton( QStringLiteral( "More infos..." ), this );
   Btn_MoreInfos->setObjectName( QStringLiteral( "Btn_MoreInfos" ) );
   connect( Btn_MoreInfos, &QPushButton::clicked, this,
            [this] { emit moreInfosRequested( FSelection.value( 0, -1 ) ); } );

   Btn_DeleteGame = new QPushButton( QStringLiteral( "Delete Game" ), this );
   Btn_DeleteGame->setObjectName( QStringLiteral( "Btn_DeleteGame" ) );
   Btn_DeleteGame->setVisible( false );
   connect( Btn_DeleteGame, &QPushButton::clicked, this, &GameEditPanel::deleteGame );

   Btn_SaveChanges = new QPushButton( QStringLiteral( "Save Changes" ), this );
   Btn_SaveChanges->setObjectName( QStringLiteral( "Btn_SaveChanges" ) );
   Btn_SaveChanges->setEnabled( false );
   connect( Btn_SaveChanges, &QPushButton::clicked, this, &GameEditPanel::save );

   QGridLayout* media = new QGridLayout;
   media->addWidget( Tbs_Media, 0, 0, 1, 2 );
   media->setRowStretch( 0, 1 );
   media->addWidget( Btn_ChangeImage, 1, 0 );
   media->addWidget( Btn_RemovePicture, 1, 1 );
   media->addWidget( Btn_ChangeVideo, 2, 0 );
   media->addWidget( Btn_RemoveVideo, 2, 1 );
   media->addWidget( Btn_SetDefaultPicture, 3, 0 );
   media->addWidget( Btn_ChangeAll, 3, 1 );

   QHBoxLayout* top = new QHBoxLayout;
   top->addLayout( fields, 1 );
   top->addLayout( media );

   QHBoxLayout* bottom = new QHBoxLayout;
   bottom->addWidget( Btn_MoreInfos );
   bottom->addWidget( Btn_DeleteGame );
   bottom->addStretch( 1 );
   bottom->addWidget( Btn_SaveChanges );

   QVBoxLayout* column = new QVBoxLayout( this );
   column->addLayout( top );
   column->addWidget( new QLabel( QStringLiteral( "Description" ), this ) );
   column->addWidget( Mmo_Description, 1 );
   column->addLayout( bottom );
}

void GameEditPanel::setGamelist( Gamelist* aGamelist )
{
   FGamelist = aGamelist;
   setSelection( {} );
}

void GameEditPanel::setSelection( const QVector<int>& aIndices )
{
   FSelection = aIndices;

   FIsLoading = true;
   setFields( {} );
   Edt_RomPath->clear();
   FPicture = QPixmap();
   Img_Game->clear();

   // One game shows what is stored; several start blank, so that every field
   // left alone stays as it is on each of them.
   if ( FGamelist && aIndices.size() == 1 )
      loadGame( aIndices.first() );

   FIsLoading = false;

   enableComponents();
   updateDirty();
}

void GameEditPanel::loadGame( int aIndex )
{
   if ( aIndex < 0 || aIndex >= FGamelist->count() )
      return;

   const Game& game = FGamelist->at( aIndex );
   setFields( gameFields( game ) );
   Edt_RomPath->setText( game.romPath );
   showPicture( aIndex );
}

void GameEditPanel::showPicture( int aIndex )
{
   const Game& game = FGamelist->at( aIndex );

   // No extension test: QPixmap sniffs the content, which is what made an image
   // called .PNG - or .jpeg where the Delphi only knew .jpg - fall through both
   // branches of LoadGame and leave the previous game's art on screen.
   if ( game.missingImage || !FPicture.load( game.physicalImagePath ) ) {
      FPicture = QPixmap();
      Img_Game->setText( QStringLiteral( "No picture" ) );
      return;
   }

   scalePicture();
}

void GameEditPanel::scalePicture()
{
   if ( FPicture.isNull() )
      return;

   const QSize fit = Img_Game->contentsRect().size().expandedTo( Img_Game->minimumSize() );
   Img_Game->setPixmap( FPicture.scaled( fit, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
}

bool GameEditPanel::eventFilter( QObject* aWatched, QEvent* aEvent )
{
   if ( aWatched == Img_Game && aEvent->type() == QEvent::Resize )
      scalePicture();

   return QWidget::eventFilter( aWatched, aEvent );
}

void GameEditPanel::showEvent( QShowEvent* aEvent )
{
   QWidget::showEvent( aEvent );
   syncVideo();
}

void GameEditPanel::hideEvent( QHideEvent* aEvent )
{
   QWidget::hideEvent( aEvent );
   FPlayer->setSource( QUrl() );
}

void GameEditPanel::syncVideo()
{
   FPlayer->setSource( QUrl() );
   Lbl_VideoStatus->clear();

   if ( !isVisible() || Tbs_Media->currentIndex() != Cst_VideoTab || !FGamelist ||
        FSelection.size() != 1 )
      return;

   const Game& game = FGamelist->at( FSelection.first() );
   if ( game.missingVideo )
      return;

   FPlayer->setSource( QUrl::fromLocalFile( game.physicalVideoPath ) );
   if ( FAutoplay )
      FPlayer->play();
   else
      FPlayer->pause();  // shows the first frame instead of starting
}

// The Video tab is only there for a game that has one. The user's choice of tab
// is remembered, so browsing a list on the Video tab stays on it.
void GameEditPanel::updateVideoTab( bool aHasVideo )
{
   FSyncingTabs = true;
   Tbs_Media->setTabEnabled( Cst_VideoTab, aHasVideo );
   Tbs_Media->setCurrentIndex( aHasVideo && FPreferVideo ? Cst_VideoTab : Cst_ImageTab );
   FSyncingTabs = false;

   syncVideo();
}

GameFields GameEditPanel::fields() const
{
   GameFields fields;
   fields.name = Edt_Name->text();
   fields.genre = Edt_Genre->text();
   fields.rating = Edt_Rating->text();
   fields.players = Edt_NbPlayers->text();
   fields.developer = Edt_Developer->text();
   fields.publisher = Edt_Publisher->text();
   fields.releaseDate = Edt_ReleaseDate->text();
   fields.description = Mmo_Description->toPlainText();
   fields.region = Edt_Region->text();
   fields.kidGame = Cbx_KidGame->currentIndex();
   fields.hidden = Cbx_Hidden->currentIndex();
   fields.favorite = Cbx_Favorite->currentIndex();
   return fields;
}

void GameEditPanel::setFields( const GameFields& aFields )
{
   const bool wasLoading = FIsLoading;
   FIsLoading = true;

   Edt_Name->setText( aFields.name );
   Edt_Genre->setText( aFields.genre );
   Edt_Rating->setText( aFields.rating );
   Edt_NbPlayers->setText( aFields.players );
   Edt_Developer->setText( aFields.developer );
   Edt_Publisher->setText( aFields.publisher );
   Edt_ReleaseDate->setText( aFields.releaseDate );
   Mmo_Description->setPlainText( aFields.description );
   Edt_Region->setText( aFields.region );
   Cbx_KidGame->setCurrentIndex( aFields.kidGame );
   Cbx_Hidden->setCurrentIndex( aFields.hidden );
   Cbx_Favorite->setCurrentIndex( aFields.favorite );

   FIsLoading = wasLoading;
   updateDirty();
}

bool GameEditPanel::isDirty() const
{
   if ( !FGamelist || FSelection.isEmpty() )
      return false;

   if ( FSelection.size() > 1 )
      return filledFields( fields() ) != GameFieldSet();

   return changedFields( FGamelist->at( FSelection.first() ), fields() ) != GameFieldSet();
}

void GameEditPanel::updateDirty()
{
   if ( FIsLoading )
      return;

   const bool dirty = isDirty();
   Btn_SaveChanges->setEnabled( dirty );

   if ( dirty == FDirty )
      return;

   FDirty = dirty;
   emit dirtyChanged( FDirty );
}

void GameEditPanel::enableComponents()
{
   const bool any = FGamelist && !FSelection.isEmpty();
   const bool single = FGamelist && FSelection.size() == 1;

   // The eight text fields and the three flags edit every selected game at once; the
   // name and the ROM path are one game's own, so they are single-only.
   const QList<QWidget*> batchEditable = { Edt_Genre,       Edt_Region,      Edt_NbPlayers,
                                           Edt_Rating,      Edt_Developer,   Edt_Publisher,
                                           Edt_ReleaseDate, Mmo_Description, Cbx_KidGame,
                                           Cbx_Hidden,      Cbx_Favorite };
   for ( QWidget* widget : batchEditable )
      widget->setEnabled( any );

   Edt_Name->setEnabled( single );
   Edt_RomPath->setEnabled( single );
   Btn_MoreInfos->setEnabled( single );

   Btn_ChangeImage->setEnabled( single );
   Btn_SetDefaultPicture->setEnabled( single );
   Btn_ChangeVideo->setEnabled( single );
   Btn_DeleteGame->setEnabled( single );

   // Btn_ChangeAll is the system's, not the selection's. The Delphi only let it
   // through on filter "Missing Picture", because it had no missing test of its
   // own and would otherwise overwrite every game's art; it has one now.
   Btn_ChangeAll->setEnabled( FGamelist != nullptr );

   const bool hasPicture = single && !FGamelist->at( FSelection.first() ).missingImage;
   const bool hasVideo = single && !FGamelist->at( FSelection.first() ).missingVideo;
   Btn_RemovePicture->setEnabled( hasPicture );
   Btn_RemoveVideo->setEnabled( hasVideo );
   updateVideoTab( hasVideo );
}

bool GameEditPanel::save()
{
   if ( !FGamelist || FSelection.isEmpty() )
      return true;

   const GameFields values = fields();

   // The one difference between the 263-line single save and the 211-line batch
   // one: which fields count. Everything after this is the same operation.
   const GameFieldSet which = FSelection.size() > 1
                                 ? filledFields( values )
                                 : changedFields( FGamelist->at( FSelection.first() ), values );
   if ( !which )
      return true;

   // Kept to undo a failed write. Otherwise the Gamelist would hold edits the
   // file does not, the panel would compare clean against them, and closing
   // would lose them without a prompt.
   QVector<GameFields> before;
   for ( int index : FSelection )
      before.append( gameFields( FGamelist->at( index ) ) );

   FGamelist->setFields( FSelection, values, which );

   QString error;
   if ( !FGamelist->save( &error ) ) {
      // ponytail: the undo writes stored values back, so it can leave an empty or
      // "false" element where the <game> had none. Harmless; a true undo would
      // need Gamelist to snapshot the DOM nodes.
      for ( qsizetype n = 0; n < FSelection.size(); ++n )
         FGamelist->setFields( { FSelection.at( n ) }, before.at( n ), which );

      QMessageBox::warning( this, QStringLiteral( "GameList Editor" ), error );
      return false;
   }

   reload();
   return true;
}

void GameEditPanel::reload()
{
   // Reload before announcing: a date that would not parse has come back blank
   // (2326 did that by hand, to the widget), and the filter may now reject the
   // game, which moves the selection.
   setSelection( FSelection );
   emit gamesChanged();
}

void GameEditPanel::setGodMode( bool aEnabled, bool aSkipPrompt )
{
   FGodMode = aEnabled;
   FSkipDeletePrompt = aEnabled && aSkipPrompt;
   Btn_DeleteGame->setVisible( aEnabled );
}

void GameEditPanel::deleteGame()
{
   if ( !FGodMode || !FGamelist || FSelection.size() != 1 )
      return;

   const int index = FSelection.first();
   if ( !FSkipDeletePrompt &&
        QMessageBox::question( this, QStringLiteral( "GameList Editor" ),
                               QStringLiteral( "Delete \"%1\"?\n"
                                               "Its gamelist entry, picture, video and ROM file "
                                               "are deleted from disk, unless another game uses "
                                               "them." )
                                  .arg( FGamelist->at( index ).name ) ) != QMessageBox::Yes )
      return;

   FPlayer->setSource( QUrl() );  // let go of the file before it is deleted
   QString error;
   if ( !FGamelist->removeGame( index, &error ) ) {
      QMessageBox::warning( this, QStringLiteral( "GameList Editor" ), error );
      syncVideo();
      return;
   }

   if ( !error.isEmpty() )
      QMessageBox::warning( this, QStringLiteral( "GameList Editor" ), error );

   // The edits were for a game that no longer exists, so they must not prompt.
   setSelection( {} );
   emit gameDeleted( index );
}

void GameEditPanel::report( bool aOk, const QString& aError )
{
   if ( !aOk )
      QMessageBox::warning( this, QStringLiteral( "GameList Editor" ), aError );

   reload();
}

void GameEditPanel::chooseImage()
{
   const QString file =
      QFileDialog::getOpenFileName( this, QStringLiteral( "Choose a picture" ), QString(),
                                    QStringLiteral( "Images (*.png *.jpg *.jpeg)" ) );
   if ( file.isEmpty() )
      return;

   QString error;
   report( FGamelist->setImage( FSelection.value( 0, -1 ), file, &error ), error );
}

void GameEditPanel::chooseVideo()
{
   const QString file =
      QFileDialog::getOpenFileName( this, QStringLiteral( "Choose a video" ), QString(),
                                    QStringLiteral( "Videos (*.mp4)" ) );
   if ( file.isEmpty() )
      return;

   FPlayer->setSource( QUrl() );  // the new file replaces the one that is open
   QString error;
   report( FGamelist->setVideo( FSelection.value( 0, -1 ), file, &error ), error );
}

void GameEditPanel::setDefaultPicture()
{
   QString error;
   report( FGamelist->setImage( FSelection.value( 0, -1 ), Cst_DefaultPicture, &error ), error );
}

void GameEditPanel::setDefaultPictureForAll()
{
   QString error;
   const int changed = FGamelist->setDefaultImageForMissing( Cst_DefaultPicture, &error );

   // A failure partway through still saves the games done before it, so the
   // count is shown whenever there is one, not only on full success.
   if ( changed > 0 || error.isEmpty() ) {
      QMessageBox::information( this, QStringLiteral( "GameList Editor" ),
                                QStringLiteral(
                                   "%1 game(s) had no picture and now have the default one." )
                                   .arg( changed ) );
   }

   report( error.isEmpty(), error );
}
