#include "ScrapePanel.h"

#include "Gamelist.h"
#include "Resources.h"

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int Cst_ThumbnailSize = 200;

}  // namespace

ScrapePanel::ScrapePanel( QWidget* aParent ) : QWidget( aParent )
{
   buildLayout();

   connect( &FScraper, &Scraper::gameInfoReady, this, &ScrapePanel::handleGameInfo );
   connect( &FScraper, &Scraper::gameInfoFailed, this, &ScrapePanel::handleGameInfoFailed );
   connect( &FScraper, &Scraper::mediaReady, this, &ScrapePanel::handleMediaReady );
   connect( &FScraper, &Scraper::mediaFinished, this, &ScrapePanel::handleMediaFinished );

   setSelection( -1 );
}

void ScrapePanel::buildLayout()
{
   const auto edit = [this]( const char* aName ) {
      QLineEdit* line = new QLineEdit( this );
      line->setObjectName( QLatin1String( aName ) );
      return line;
   };

   Edt_ScrapeName = edit( "Edt_ScrapeName" );
   Edt_ScrapeRegion = edit( "Edt_ScrapeRegion" );
   Edt_ScrapeDate = edit( "Edt_ScrapeDate" );
   Edt_ScrapeDate->setToolTip(
      QStringLiteral( "Following formats are accepted:\n\ndd/mm/yyyy\nmm/yyyy\nyyyy\n\n"
                      "Everything else will be saved as blank." ) );
   Edt_ScrapeGenre = edit( "Edt_ScrapeGenre" );
   Edt_ScrapeDeveloper = edit( "Edt_ScrapeDeveloper" );
   Edt_ScrapePublisher = edit( "Edt_ScrapePublisher" );
   Edt_ScrapeRating = edit( "Edt_ScrapeRating" );
   Edt_ScrapePlayers = edit( "Edt_ScrapePlayers" );

   Edt_ScrapeRomPath = new QLineEdit( this );
   Edt_ScrapeRomPath->setObjectName( QStringLiteral( "Edt_ScrapeRomPath" ) );
   Edt_ScrapeRomPath->setReadOnly( true );

   Mmo_ScrapeDescription = new QPlainTextEdit( this );
   Mmo_ScrapeDescription->setObjectName( QStringLiteral( "Mmo_ScrapeDescription" ) );

   const auto mediaCheck = [this]( const char* aName, const QString& aText ) {
      QCheckBox* check = new QCheckBox( aText, this );
      check->setObjectName( QLatin1String( aName ) );
      check->setChecked( true );
      return check;
   };

   Chk_Box2D = mediaCheck( "Chk_Box2D", QStringLiteral( "Box 2D" ) );
   Chk_Box3D = mediaCheck( "Chk_Box3D", QStringLiteral( "Box 3D" ) );
   Chk_Mix1 = mediaCheck( "Chk_Mix1", QStringLiteral( "Mix 1" ) );
   Chk_Mix2 = mediaCheck( "Chk_Mix2", QStringLiteral( "Mix 2" ) );
   Chk_Screenshot = mediaCheck( "Chk_Screenshot", QStringLiteral( "Screenshot" ) );
   Chk_Title = mediaCheck( "Chk_Title", QStringLiteral( "Title" ) );
   Chk_ArcadeBox = mediaCheck( "Chk_ArcadeBox", QStringLiteral( "Arcade Box" ) );
   Chk_Wheel = mediaCheck( "Chk_Wheel", QStringLiteral( "Wheel" ) );
   Chk_Video = mediaCheck( "Chk_Video", QStringLiteral( "Video" ) );

   QHBoxLayout* mediaRow = new QHBoxLayout;
   for ( QCheckBox* check : { Chk_Box2D, Chk_Box3D, Chk_Mix1, Chk_Mix2, Chk_Screenshot, Chk_Title,
                              Chk_ArcadeBox, Chk_Wheel, Chk_Video } )
      mediaRow->addWidget( check );
   mediaRow->addStretch( 1 );

   Btn_Scrape = new QPushButton( QStringLiteral( "Scrape" ), this );
   Btn_Scrape->setObjectName( QStringLiteral( "Btn_Scrape" ) );
   connect( Btn_Scrape, &QPushButton::clicked, this, &ScrapePanel::requestScrape );

   QFormLayout* fields = new QFormLayout;
   fields->addRow( QStringLiteral( "Rom" ), Edt_ScrapeRomPath );
   fields->addRow( QStringLiteral( "Name" ), Edt_ScrapeName );
   fields->addRow( QStringLiteral( "Date" ), Edt_ScrapeDate );
   fields->addRow( QStringLiteral( "Genre" ), Edt_ScrapeGenre );
   fields->addRow( QStringLiteral( "Region" ), Edt_ScrapeRegion );
   fields->addRow( QStringLiteral( "Players" ), Edt_ScrapePlayers );
   fields->addRow( QStringLiteral( "Rating" ), Edt_ScrapeRating );
   fields->addRow( QStringLiteral( "Developer" ), Edt_ScrapeDeveloper );
   fields->addRow( QStringLiteral( "Publisher" ), Edt_ScrapePublisher );

   QVBoxLayout* left = new QVBoxLayout;
   left->addLayout( mediaRow );
   left->addWidget( Btn_Scrape );
   left->addLayout( fields );
   left->addWidget( new QLabel( QStringLiteral( "Description" ), this ) );
   left->addWidget( Mmo_ScrapeDescription, 1 );

   // Right column: the chosen picture, the result strip, the manual-CRC and
   // save-selection controls.
   Img_Scrape = new QLabel( this );
   Img_Scrape->setObjectName( QStringLiteral( "Img_Scrape" ) );
   Img_Scrape->setAlignment( Qt::AlignCenter );
   Img_Scrape->setMinimumSize( 260, 200 );
   Img_Scrape->setFrameShape( QFrame::StyledPanel );

   Img_Loading = new QProgressBar( this );
   Img_Loading->setObjectName( QStringLiteral( "Img_Loading" ) );
   Img_Loading->setRange( 0, 0 );  // indeterminate: there is no total to count against
   Img_Loading->setVisible( false );

   QWidget* picturesHost = new QWidget;
   FPicturesLayout = new QHBoxLayout( picturesHost );
   FPicturesLayout->addStretch( 1 );

   Scl_Pictures = new QScrollArea( this );
   Scl_Pictures->setObjectName( QStringLiteral( "Scl_Pictures" ) );
   Scl_Pictures->setWidget( picturesHost );
   Scl_Pictures->setWidgetResizable( true );
   Scl_Pictures->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
   Scl_Pictures->setMinimumHeight( Cst_ThumbnailSize + 20 );

   Chk_ManualCRC = new QCheckBox( QStringLiteral( "Enter CRC32 manually" ), this );
   Chk_ManualCRC->setObjectName( QStringLiteral( "Chk_ManualCRC" ) );
   Edt_ManualCRC = new QLineEdit( this );
   Edt_ManualCRC->setObjectName( QStringLiteral( "Edt_ManualCRC" ) );
   Edt_ManualCRC->setEnabled( false );
   connect( Chk_ManualCRC, &QCheckBox::toggled, Edt_ManualCRC, &QLineEdit::setEnabled );

   Btn_ScrapeUpper = new QPushButton( QStringLiteral( "Convert all text to uppercase" ), this );
   Btn_ScrapeUpper->setObjectName( QStringLiteral( "Btn_ScrapeUpper" ) );
   connect( Btn_ScrapeUpper, &QPushButton::clicked, this, [this] { convertCase( true ); } );

   Btn_ScrapeLower = new QPushButton( QStringLiteral( "Convert all text to lowercase" ), this );
   Btn_ScrapeLower->setObjectName( QStringLiteral( "Btn_ScrapeLower" ) );
   connect( Btn_ScrapeLower, &QPushButton::clicked, this, [this] { convertCase( false ); } );

   Chk_ScrapePicture = new QCheckBox( QStringLiteral( "Save picture" ), this );
   Chk_ScrapePicture->setObjectName( QStringLiteral( "Chk_ScrapePicture" ) );
   Chk_ScrapeVideo = new QCheckBox( QStringLiteral( "Save video" ), this );
   Chk_ScrapeVideo->setObjectName( QStringLiteral( "Chk_ScrapeVideo" ) );
   Chk_ScrapeInfos = new QCheckBox( QStringLiteral( "Save infos" ), this );
   Chk_ScrapeInfos->setObjectName( QStringLiteral( "Chk_ScrapeInfos" ) );
   Chk_ScrapeInfos->setChecked( true );
   for ( QCheckBox* check : { Chk_ScrapePicture, Chk_ScrapeVideo, Chk_ScrapeInfos } )
      connect( check, &QCheckBox::toggled, this, &ScrapePanel::updateSaveEnabled );

   Btn_ScrapeSave = new QPushButton( QStringLiteral( "Save Changes for this game" ), this );
   Btn_ScrapeSave->setObjectName( QStringLiteral( "Btn_ScrapeSave" ) );
   connect( Btn_ScrapeSave, &QPushButton::clicked, this, &ScrapePanel::save );

   QVBoxLayout* right = new QVBoxLayout;
   right->addWidget( Img_Scrape, 1 );
   right->addWidget( Img_Loading );

   right->addWidget( Chk_ManualCRC );
   right->addWidget( Edt_ManualCRC );
   right->addWidget( Btn_ScrapeUpper );
   right->addWidget( Btn_ScrapeLower );
   right->addWidget( Chk_ScrapePicture );
   right->addWidget( Chk_ScrapeVideo );
   right->addWidget( Chk_ScrapeInfos );
   right->addWidget( Btn_ScrapeSave );

   QHBoxLayout* columns = new QHBoxLayout;
   columns->addLayout( left, 1 );
   columns->addLayout( right );

   // The showcase spans the full panel width above both columns.
   QVBoxLayout* outer = new QVBoxLayout( this );
   outer->addWidget( Scl_Pictures );
   outer->addLayout( columns, 1 );
}

void ScrapePanel::setGamelist( Gamelist* aGamelist )
{
   FGamelist = aGamelist;
   setSelection( -1 );
}

void ScrapePanel::setSelection( int aIndex )
{
   FIndex = aIndex;
   emptyScrapeFields();
   clearScrapeMedia();

   const bool valid = FGamelist && aIndex >= 0 && aIndex < FGamelist->count();
   Edt_ScrapeRomPath->setText( valid ? FGamelist->at( aIndex ).romPath : QString() );

   // Btn_Scrape.Enabled := FileExists( aGame.PhysicalRomPath ) (1551): a game
   // whose ROM has vanished has nothing to hash.
   const bool canScrape = valid && !FGamelist->at( aIndex ).isOrphan;
   Btn_Scrape->setEnabled( canScrape );
   Chk_ManualCRC->setEnabled( canScrape );
   Edt_ManualCRC->setEnabled( canScrape && Chk_ManualCRC->isChecked() );
}

void ScrapePanel::activate()
{
   QSettings settings;
   settings.beginGroup( QLatin1String( Cst_IniOptions ) );

   if ( settings.value( QLatin1String( Cst_IniProxyUse ), false ).toBool() ) {
      FScraper.setProxy( settings.value( QLatin1String( Cst_IniProxyServer ) ).toString(),
                         settings.value( QLatin1String( Cst_IniProxyPort ), 0 ).toInt(),
                         settings.value( QLatin1String( Cst_IniProxyUser ) ).toString(),
                         settings.value( QLatin1String( Cst_IniProxyPwd ) ).toString() );
   } else {
      FScraper.setProxy( QString(), 0, QString(), QString() );
   }

   FSSLogin = settings.value( QLatin1String( Cst_IniSSUser ) ).toString();
   FSSPassword = settings.value( QLatin1String( Cst_IniSSPwd ) ).toString();

   FLanguage = langFromIndex(
      settings.value( QLatin1String( Cst_IniLanguage ), Cst_IniLanguageDefault ).toInt() );

   clearScrapeMedia();
}

GameFields ScrapePanel::fields() const
{
   GameFields f;
   f.name = Edt_ScrapeName->text();
   f.genre = Edt_ScrapeGenre->text();
   f.rating = Edt_ScrapeRating->text();
   f.players = Edt_ScrapePlayers->text();
   f.developer = Edt_ScrapeDeveloper->text();
   f.publisher = Edt_ScrapePublisher->text();
   f.releaseDate = Edt_ScrapeDate->text();
   f.description = Mmo_ScrapeDescription->toPlainText();
   f.region = Edt_ScrapeRegion->text();
   return f;
}

void ScrapePanel::setFields( const GameFields& aFields )
{
   Edt_ScrapeName->setText( aFields.name );
   Edt_ScrapeGenre->setText( aFields.genre );
   Edt_ScrapeRating->setText( aFields.rating );
   Edt_ScrapePlayers->setText( aFields.players );
   Edt_ScrapeDeveloper->setText( aFields.developer );
   Edt_ScrapePublisher->setText( aFields.publisher );
   Edt_ScrapeDate->setText( aFields.releaseDate );
   Mmo_ScrapeDescription->setPlainText( aFields.description );
   Edt_ScrapeRegion->setText( aFields.region );
}

QSet<QString> ScrapePanel::mediaTypes() const
{
   QSet<QString> types;
   const QVector<QPair<QCheckBox*, const char*>> map = {
      { Chk_Box2D, Cst_MediaBox2d },
      { Chk_Box3D, Cst_MediaBox3d },
      { Chk_Mix1, Cst_MediaMix1 },
      { Chk_Mix2, Cst_MediaMix2 },
      { Chk_Screenshot, Cst_MediaScreenShot },
      { Chk_Title, Cst_MediaSsTitle },
      { Chk_ArcadeBox, Cst_MediaArcadeBox1 },
      { Chk_Wheel, Cst_MediaWheel },
   };
   for ( const auto& pair : map )
      if ( pair.first->isChecked() )
         types.insert( QLatin1String( pair.second ) );
   return types;
}

void ScrapePanel::emptyScrapeFields()
{
   setFields( {} );
   FSelectedPicture = QImage();
   showSelectedPicture();
   FLastInfo = ScrapedInfo();
   FHasScrapedData = false;
   enableScrapeComponents( false );
   updateSaveEnabled();

   // FImgList.Clear / FPictureLinks.Clear (1989-1990): drop every thumbnail.
   for ( QToolButton* button : Scl_Pictures->widget()->findChildren<QToolButton*>() )
      delete button;
}

void ScrapePanel::clearScrapeMedia()
{
   Chk_ScrapePicture->setChecked( false );
   Chk_ScrapePicture->setEnabled( false );
   Chk_ScrapeVideo->setChecked( false );
   Chk_ScrapeVideo->setEnabled( false );
   updateSaveEnabled();
}

void ScrapePanel::enableScrapeComponents( bool aEnabled )
{
   const QList<QWidget*> widgets = { Edt_ScrapeGenre,   Edt_ScrapeDate,      Edt_ScrapeName,
                                     Edt_ScrapeRegion,  Edt_ScrapeDeveloper, Edt_ScrapeRating,
                                     Edt_ScrapePlayers, Edt_ScrapePublisher, Mmo_ScrapeDescription,
                                     Btn_ScrapeLower,   Btn_ScrapeUpper,     Chk_ScrapeInfos };
   for ( QWidget* widget : widgets )
      widget->setEnabled( aEnabled );
}

// Chk_ScrapeClick (4042) folded with EnableScrapeComponents' Btn_ScrapeSave
// line: the button needs data to save and something checked to save it with.
void ScrapePanel::updateSaveEnabled()
{
   Btn_ScrapeSave->setEnabled( FHasScrapedData &&
                               ( Chk_ScrapeInfos->isChecked() || Chk_ScrapePicture->isChecked() ||
                                 Chk_ScrapeVideo->isChecked() ) );
}

void ScrapePanel::showSelectedPicture()
{
   if ( FSelectedPicture.isNull() ) {
      Img_Scrape->setText( QStringLiteral( "No picture selected" ) );
      Img_Scrape->setPixmap( {} );
      return;
   }

   const QSize fit = Img_Scrape->size().expandedTo( Img_Scrape->minimumSize() );
   Img_Scrape->setPixmap( QPixmap::fromImage( FSelectedPicture )
                             .scaled( fit, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
}

void ScrapePanel::selectPicture( const QImage& aImage )
{
   FSelectedPicture = aImage;
   showSelectedPicture();
   Chk_ScrapePicture->setEnabled( true );
   Chk_ScrapePicture->setChecked( true );
}

void ScrapePanel::convertCase( bool aUpper )
{
   const auto convert = [aUpper]( const QString& aText ) {
      return aUpper ? aText.toUpper() : aText.toLower();
   };
   for ( QLineEdit* edit : { Edt_ScrapeGenre, Edt_ScrapeName, Edt_ScrapeRegion, Edt_ScrapeDeveloper,
                             Edt_ScrapeRating, Edt_ScrapePlayers, Edt_ScrapePublisher } )
      edit->setText( convert( edit->text() ) );
   Mmo_ScrapeDescription->setPlainText( convert( Mmo_ScrapeDescription->toPlainText() ) );
}

void ScrapePanel::requestScrape()
{
   if ( !FGamelist || FIndex < 0 )
      return;

   emptyScrapeFields();
   clearScrapeMedia();

   Img_Loading->setVisible( true );
   setEnabled( false );

   // The hash is cached on the Game once computed, so a rescrape does not
   // rehash a ROM that has not changed.
   FGamelist->ensureHashes( FIndex );
   const Game& game = FGamelist->at( FIndex );

   ScrapeRequest request;
   request.systemId = QLatin1String( Cst_SystemKindId[FGamelist->system().kind] );
   request.manualCrc = Chk_ManualCRC->isChecked();
   request.crc32 = request.manualCrc ? Edt_ManualCRC->text() : game.crc32;
   request.romName = game.romName();
   request.romSize = QFileInfo( game.physicalRomPath ).size();
   request.ssLogin = FSSLogin;
   request.ssPassword = FSSPassword;


   FScraper.requestGame( request, mediaTypes(), Chk_Video->isChecked(), FLanguage );
}

void ScrapePanel::handleGameInfo( const ScrapedInfo& aInfo )
{
   FLastInfo = aInfo;

   GameFields fields;
   fields.name = aInfo.name;
   fields.region = aInfo.region;
   fields.releaseDate = aInfo.releaseDate;
   fields.genre = aInfo.genre;
   fields.developer = aInfo.developer;
   fields.publisher = aInfo.publisher;
   fields.rating = aInfo.rating;
   fields.players = aInfo.players;
   fields.description = aInfo.description;
   setFields( fields );

   if ( !aInfo.videoLink.isEmpty() ) {
      Chk_ScrapeVideo->setEnabled( true );
      Chk_ScrapeVideo->setChecked( true );
   }

   FScraper.requestMedia( aInfo.pictures, aInfo.maxThreads );
}

void ScrapePanel::handleGameInfoFailed( const QString& aMessage )
{
   Img_Loading->setVisible( false );
   setEnabled( true );
   QMessageBox::warning( this, QStringLiteral( "GameList Editor" ), aMessage );
}

void ScrapePanel::handleMediaReady( int /*aIndex*/, const QImage& aImage )
{
   QToolButton* button = new QToolButton( Scl_Pictures->widget() );
   button->setIcon( QIcon(
      QPixmap::fromImage( aImage.scaled( Cst_ThumbnailSize, Cst_ThumbnailSize, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation ) ) ) );
   button->setIconSize( QSize( Cst_ThumbnailSize, Cst_ThumbnailSize ) );
   button->setAutoRaise( true );
   connect( button, &QToolButton::clicked, this, [this, aImage] { selectPicture( aImage ); } );

   // Inserted before the trailing stretch, so new thumbnails keep packing left.
   FPicturesLayout->insertWidget( FPicturesLayout->count() - 1, button );
}

void ScrapePanel::handleMediaFinished()
{
   Img_Loading->setVisible( false );
   setEnabled( true );
   FHasScrapedData = true;
   enableScrapeComponents( true );
   updateSaveEnabled();
}

void ScrapePanel::save()
{
   if ( !FGamelist || FIndex < 0 )
      return;

   // The video is downloaded now, not at scrape time (FVideoScrapeLink was
   // only ever a link until SaveChangesToGamelist called SaveLinkToFile) - so
   // saving needs one round trip before there is a file to hand to Gamelist.
   if ( Chk_ScrapeVideo->isChecked() && !FLastInfo.videoLink.isEmpty() ) {
      const QString temp =
         QDir::temp().filePath( QStringLiteral( "gamelisteditor-scrape-video-%1.mp4" )
                                   .arg( reinterpret_cast<quintptr>( this ), 0, 16 ) );

      auto onSaved = std::make_shared<QMetaObject::Connection>();
      auto onFailed = std::make_shared<QMetaObject::Connection>();
      *onSaved = connect( &FScraper, &Scraper::fileSaved, this,
                          [this, onSaved, onFailed]( const QString& aPath ) {
                             QObject::disconnect( *onSaved );
                             QObject::disconnect( *onFailed );
                             finishSave( aPath );
                          } );
      *onFailed = connect( &FScraper, &Scraper::fileSaveFailed, this,
                           [this, onSaved, onFailed]( const QString&, const QString& aMessage ) {
                              QObject::disconnect( *onSaved );
                              QObject::disconnect( *onFailed );
                              report( false, aMessage );
                           } );
      FScraper.downloadToFile( FLastInfo.videoLink, temp );
   } else {
      finishSave( QString() );
   }
}

void ScrapePanel::finishSave( const QString& aVideoPath )
{
   const GameFieldSet which = Chk_ScrapeInfos->isChecked()
                                 ? changedFields( FGamelist->at( FIndex ), fields() )
                                 : GameFieldSet();
   if ( which )
      FGamelist->setFields( { FIndex }, fields(), which );

   QString error;
   bool ok = true;
   bool savedAlready = false;

   if ( Chk_ScrapePicture->isChecked() && !FSelectedPicture.isNull() ) {
      ok = FGamelist->setImage( FIndex, FSelectedPicture, &error );
      savedAlready = true;
   }

   if ( ok && !aVideoPath.isEmpty() ) {
      ok = FGamelist->setVideo( FIndex, aVideoPath, &error );
      savedAlready = true;
      QFile::remove( aVideoPath );
   }

   // setFields() alone does not save - that is the point of the one open
   // document - so an infos-only save has to ask for it explicitly.
   if ( ok && which && !savedAlready )
      ok = FGamelist->save( &error );

   report( ok, error );
}

void ScrapePanel::report( bool aOk, const QString& aError )
{
   if ( !aOk )
      QMessageBox::warning( this, QStringLiteral( "GameList Editor" ), aError );

   emit gamesChanged();
}
