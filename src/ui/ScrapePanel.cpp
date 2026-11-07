#include "ScrapePanel.h"

#include "Gamelist.h"
#include "F_PickGame.h"
#include "Resources.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <memory>

namespace {

constexpr int Cst_ThumbnailSize = 120;

constexpr int Cst_SourceScreenScraper = 0;
constexpr int Cst_SourceLaunchBox = 1;

QString pickerRow( const QString& aName, const QString& aYear, const QString& aAltName = {} )
{
   QString text = aName;
   if ( !aYear.isEmpty() )
      text += QStringLiteral( " (%1)" ).arg( aYear );
   if ( !aAltName.isEmpty() )
      text += QStringLiteral( " \u2013 also known as %1" ).arg( aAltName );
   return text;
}

}  // namespace

ScrapePanel::ScrapePanel( QWidget* aParent, const QString& aLaunchBoxDir )
   : QWidget( aParent ), FDump( aLaunchBoxDir )
{
   buildLayout();

   connect( &FScraper, &Scraper::gameInfoReady, this, &ScrapePanel::handleGameInfo );
   connect( &FScraper, &Scraper::gameInfoFailed, this, &ScrapePanel::handleScrapeFailed );
   connect( &FScraper, &Scraper::searchReady, this, &ScrapePanel::handleSearchReady );
   connect( &FScraper, &Scraper::searchFailed, this, &ScrapePanel::handleSearchFailed );
   connect( &FScraper, &Scraper::mediaReady, this, &ScrapePanel::handleMediaReady );
   connect( &FScraper, &Scraper::mediaFinished, this, &ScrapePanel::handleMediaFinished );

   connect( &FDump, &LaunchBoxDump::downloadProgress, this,
            [this]( qint64 aReceived, qint64 aTotal ) {
               Img_Loading->setRange( 0, aTotal > 0 ? static_cast<int>( aTotal / 1024 ) : 0 );
               Img_Loading->setValue( static_cast<int>( aReceived / 1024 ) );
            } );
   connect( &FDump, &LaunchBoxDump::unpacking, this, [this] { Img_Loading->setRange( 0, 0 ); } );
   connect( &FDump, &LaunchBoxDump::finished, this, [this]( bool aUpdated ) {
      if ( aUpdated ) {
         FLbIndex.reset();
         FLbPlatform.clear();
      }
      Img_Loading->setRange( 0, 0 );
      setBusy( false );
      Lbl_Note->setText( aUpdated ? QStringLiteral( "LaunchBox DB updated." )
                                  : QStringLiteral( "LaunchBox DB is already up to date." ) );
   } );
   connect( &FDump, &LaunchBoxDump::failed, this, [this]( const QString& aMessage ) {
      Img_Loading->setRange( 0, 0 );
      setBusy( false );
      QMessageBox::warning( this, QStringLiteral( "GameList Editor Qt" ), aMessage );
   } );

   setSelection( -1 );
}

ScrapePanel::~ScrapePanel()
{
   if ( FIndexThread )
      FIndexThread->wait();
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

   // 3x3 instead of one row: nine checkboxes in a row were wider than the
   // panel could give them at 1024px.
   QGridLayout* mediaRow = new QGridLayout;
   int cell = 0;
   for ( QCheckBox* check : { Chk_Box2D, Chk_Box3D, Chk_Mix1, Chk_Mix2, Chk_Screenshot, Chk_Title,
                              Chk_ArcadeBox, Chk_Wheel, Chk_Video } ) {
      mediaRow->addWidget( check, cell / 3, cell % 3 );
      ++cell;
   }
   mediaRow->setColumnStretch( 3, 1 );

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

   Cbo_Source = new QComboBox( this );
   Cbo_Source->setObjectName( QStringLiteral( "Cbo_Source" ) );
   Cbo_Source->addItems( { QStringLiteral( "ScreenScraper" ), QStringLiteral( "LaunchBox" ) } );
   connect( Cbo_Source, &QComboBox::currentIndexChanged, this, [this]( int aIndex ) {
      QSettings settings;
      settings.beginGroup( Cst_IniOptions );
      settings.setValue( Cst_IniScrapeSource, aIndex );
      applySource();
   } );

   Btn_RefreshLaunchBox =
      new QPushButton( QStringLiteral( "Refresh LaunchBox DB" ), this );
   Btn_RefreshLaunchBox->setObjectName( QStringLiteral( "Btn_RefreshLaunchBox" ) );
   Btn_RefreshLaunchBox->setVisible( false );
   connect( Btn_RefreshLaunchBox, &QPushButton::clicked, this, &ScrapePanel::refreshLaunchBox );

   Lbl_Note = new QLabel( this );
   Lbl_Note->setObjectName( QStringLiteral( "Lbl_Note" ) );
   Lbl_Note->setWordWrap( true );

   QHBoxLayout* sourceRow = new QHBoxLayout;
   sourceRow->addWidget( new QLabel( QStringLiteral( "Source" ), this ) );
   sourceRow->addWidget( Cbo_Source );
   sourceRow->addWidget( Btn_RefreshLaunchBox );
   sourceRow->addStretch( 1 );

   QVBoxLayout* left = new QVBoxLayout;
   left->addLayout( sourceRow );
   left->addLayout( mediaRow );
   left->addWidget( Btn_Scrape );
   left->addWidget( Lbl_Note );
   left->addLayout( fields );
   left->addWidget( new QLabel( QStringLiteral( "Description" ), this ) );
   left->addWidget( Mmo_ScrapeDescription, 1 );

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
   Scl_Pictures->setMinimumHeight( Cst_ThumbnailSize + 20 +
                                   Scl_Pictures->horizontalScrollBar()->sizeHint().height() );

   Chk_ManualCRC = new QCheckBox( QStringLiteral( "Enter CRC32 manually" ), this );
   Chk_ManualCRC->setObjectName( QStringLiteral( "Chk_ManualCRC" ) );
   Edt_ManualCRC = new QLineEdit( this );
   Edt_ManualCRC->setObjectName( QStringLiteral( "Edt_ManualCRC" ) );
   Edt_ManualCRC->setEnabled( false );
   connect( Chk_ManualCRC, &QCheckBox::toggled, Edt_ManualCRC, &QLineEdit::setEnabled );

   Btn_ScrapeUpper = new QPushButton( QStringLiteral( "To uppercase" ), this );
   Btn_ScrapeUpper->setToolTip( QStringLiteral( "Convert all text to uppercase" ) );
   Btn_ScrapeUpper->setObjectName( QStringLiteral( "Btn_ScrapeUpper" ) );
   connect( Btn_ScrapeUpper, &QPushButton::clicked, this, [this] { convertCase( true ); } );

   Btn_ScrapeLower = new QPushButton( QStringLiteral( "To lowercase" ), this );
   Btn_ScrapeLower->setToolTip( QStringLiteral( "Convert all text to lowercase" ) );
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

   QHBoxLayout* caseRow = new QHBoxLayout;
   caseRow->addWidget( Btn_ScrapeUpper );
   caseRow->addWidget( Btn_ScrapeLower );
   right->addLayout( caseRow );

   QHBoxLayout* saveRow = new QHBoxLayout;
   saveRow->addWidget( Chk_ScrapePicture );
   saveRow->addWidget( Chk_ScrapeVideo );
   saveRow->addWidget( Chk_ScrapeInfos );
   right->addLayout( saveRow );
   right->addWidget( Btn_ScrapeSave );

   QHBoxLayout* columns = new QHBoxLayout;
   columns->addLayout( left, 1 );
   columns->addLayout( right );

   // The thumbnail strip spans the full panel width above both columns.
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

   // A game whose ROM has vanished has nothing to hash.
   const bool canScrape = valid && !FGamelist->at( aIndex ).isOrphan;
   Btn_Scrape->setEnabled( canScrape );
   const bool launchBox = launchBoxSelected();
   Chk_ManualCRC->setEnabled( canScrape && !launchBox );
   Edt_ManualCRC->setEnabled( canScrape && !launchBox && Chk_ManualCRC->isChecked() );
}

bool ScrapePanel::launchBoxSelected() const
{
   return Cbo_Source->currentIndex() == Cst_SourceLaunchBox;
}

// LaunchBox has no video, CRC lookup or Mix/Arcade box art. The old results are from the other source.
void ScrapePanel::applySource()
{
   const bool launchBox = launchBoxSelected();
   Btn_RefreshLaunchBox->setVisible( launchBox );
   const QList<QWidget*> sourceOnly = { Chk_Mix1, Chk_Mix2, Chk_ArcadeBox, Chk_Video };
   for ( QWidget* widget : sourceOnly )
      widget->setEnabled( !launchBox );
   setSelection( FIndex );
}

void ScrapePanel::setBusy( bool aBusy )
{
   Img_Loading->setVisible( aBusy );
   setEnabled( !aBusy );
   emit busyChanged( aBusy );
}

void ScrapePanel::activate()
{
   QSettings settings;
   settings.beginGroup( Cst_IniOptions );

   if ( settings.value( Cst_IniProxyUse, false ).toBool() ) {
      const QString server = settings.value( Cst_IniProxyServer ).toString();
      const int port = settings.value( Cst_IniProxyPort, 0 ).toInt();
      const QString user = settings.value( Cst_IniProxyUser ).toString();
      const QString password = settings.value( Cst_IniProxyPwd ).toString();
      QNetworkProxy::setApplicationProxy( QNetworkProxy(
         QNetworkProxy::HttpProxy, server, static_cast<quint16>( port ), user, password ) );
   } else {
      QNetworkProxy::setApplicationProxy( QNetworkProxy::NoProxy );
   }

   FSSLogin = settings.value( Cst_IniSSUser ).toString();
   FSSPassword = settings.value( Cst_IniSSPwd ).toString();

   FLanguage = langFromIndex(
      settings.value( Cst_IniLanguage, Cst_IniLanguageDefault ).toInt() );

   Cbo_Source->setCurrentIndex(
      settings.value( Cst_IniScrapeSource, Cst_SourceScreenScraper ).toInt() ==
            Cst_SourceLaunchBox
         ? Cst_SourceLaunchBox
         : Cst_SourceScreenScraper );

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
   const QVector<QPair<QCheckBox*, QLatin1StringView>> map = {
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
         types.insert( pair.second );
   return types;
}

void ScrapePanel::emptyScrapeFields()
{
   setFields( {} );
   Lbl_Note->clear();
   FSelectedPicture = QImage();
   showSelectedPicture();
   FLastInfo = ScrapedInfo();
   FHasScrapedData = false;
   enableScrapeComponents( false );
   updateSaveEnabled();

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

// The button needs scraped data to save and something ticked to save it with.
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

   setBusy( true );

   if ( launchBoxSelected() ) {
      requestLaunchBox();
      return;
   }

   // The hash is cached on the Game once computed, so a rescrape does not
   // rehash a ROM that has not changed.
   FGamelist->ensureHashes( FIndex );
   const Game& game = FGamelist->at( FIndex );

   ScrapeRequest request;
   request.systemId = QLatin1String( systemInfo( FGamelist->system().kind ).ssId );
   request.manualCrc = Chk_ManualCRC->isChecked();
   request.crc32 = request.manualCrc ? Edt_ManualCRC->text() : game.crc32;
   request.romName = game.romName();
   request.romSize = QFileInfo( game.physicalRomPath ).size();
   request.ssLogin = FSSLogin;
   request.ssPassword = FSSPassword;

   FScraper.requestGame( request, mediaTypes(), Chk_Video->isChecked(), FLanguage );
}

void ScrapePanel::refreshLaunchBox()
{
   Lbl_Note->clear();
   setBusy( true );
   FDump.refresh();
}

void ScrapePanel::requestLaunchBox()
{
   const QString platform =
      QLatin1String( systemInfo( FGamelist->system().kind ).lbName );
   if ( platform.isEmpty() ) {
      handleGameInfoFailed( QStringLiteral( "LaunchBox has no platform for this system." ), {} );
      return;
   }
   if ( !FDump.isCached() ) {
      handleGameInfoFailed( QStringLiteral( "The LaunchBox DB is not downloaded yet. Use "
                                            "\"Refresh LaunchBox DB\" first." ),
                            {} );
      return;
   }

   if ( FLbIndex && FLbPlatform == platform )
      lookupLaunchBox();
   else
      buildLaunchBoxIndex( platform );
}

// Reading Metadata.xml takes tens of seconds, so it runs on a worker; the panel stays busy.
void ScrapePanel::buildLaunchBoxIndex( const QString& aPlatform )
{
   const QString metadata = FDump.filePath( QStringLiteral( "Metadata.xml" ) );
   const QString files = FDump.filePath( QStringLiteral( "Files.xml" ) );
   const QString mame = FDump.filePath( QStringLiteral( "Mame.xml" ) );
   const auto db = std::make_shared<LaunchBoxDb>();
   const auto result = std::make_shared<Result<>>();

   FIndexThread = QThread::create(
      [=] { *result = db->loadDump( metadata, files, mame, aPlatform ); } );
   FIndexThread->setParent( this );
   connect( FIndexThread, &QThread::finished, this, [this, db, result, aPlatform] {
      FIndexThread->deleteLater();
      FIndexThread = nullptr;
      if ( !*result ) {
         handleGameInfoFailed( result->error(), {} );
         return;
      }
      FLbIndex = db;
      FLbPlatform = aPlatform;
      lookupLaunchBox();
   } );
   FIndexThread->start();
}

void ScrapePanel::lookupLaunchBox()
{
   const Game& game = FGamelist->at( FIndex );
   LaunchBoxQuery query;
   query.romName = game.romNameWoExt();
   query.gameName = game.name;
   query.region = game.region;
   query.mediaTypes = mediaTypes();

   const Result<LaunchBoxMatch> match = FLbIndex->find( query );
   if ( !match ) {
      pickLaunchBoxGame( query );
      return;
   }
   if ( match.value().ambiguous )
      Lbl_Note->setText(
         QStringLiteral( "Several LaunchBox games share this name; the first was taken." ) );
   handleGameInfo( match.value().info );
}

void ScrapePanel::pickLaunchBoxGame( const LaunchBoxQuery& aQuery )
{
   if ( FLbIndex->gameCount() == 0 ) {
      handleGameInfoFailed( Cst_GameNotFound, {} );
      return;
   }

   Frm_PickGame dialog( this );
   dialog.setHint( QStringLiteral( "No LaunchBox game has exactly this name. Pick the closest "
                                   "one, or change the search." ) );
   QVector<LaunchBoxCandidate> shown;
   const auto search = [&]( const QString& aText ) {
      shown = FLbIndex->similar( aText );
      QStringList rows;
      for ( const LaunchBoxCandidate& candidate : std::as_const( shown ) )
         rows << pickerRow( candidate.name, candidate.year, candidate.altName );
      dialog.setRows( rows );
   };
   connect( &dialog, &Frm_PickGame::searchRequested, &dialog, search );

   const QString initial = aQuery.gameName.isEmpty() ? aQuery.romName : aQuery.gameName;
   search( initial );
   const int row = dialog.Execute( initial );
   if ( row < 0 ) {
      setBusy( false );
      return;
   }
   handleGameInfo( FLbIndex->infoAt( shown.at( row ).index, aQuery ) );
}

void ScrapePanel::handleScrapeFailed( const QString& aMessage, const QString& aDetails )
{
   if ( aMessage != Cst_GameNotFound || !FGamelist || FIndex < 0 ) {
      handleGameInfoFailed( aMessage, aDetails );
      return;
   }
   FNotFoundDetails = aDetails;
   const Game& game = FGamelist->at( FIndex );
   searchScreenScraper( game.name.isEmpty() ? game.romNameWoExt() : game.name );
}

void ScrapePanel::searchScreenScraper( const QString& aText )
{
   if ( aText.trimmed().isEmpty() )
      return;

   FSearchText = aText;
   FAwaitingSearch = true;

   ScrapeRequest request;
   request.systemId = QLatin1String( systemInfo( FGamelist->system().kind ).ssId );
   request.ssLogin = FSSLogin;
   request.ssPassword = FSSPassword;
   FScraper.searchGames( request, aText, FLanguage );
}

void ScrapePanel::handleSearchReady( const QVector<ScrapeCandidate>& aCandidates )
{
   if ( !FAwaitingSearch )
      return;
   FAwaitingSearch = false;

   FCandidates = aCandidates;
   QStringList rows;
   for ( const ScrapeCandidate& candidate : std::as_const( FCandidates ) )
      rows << pickerRow( candidate.name, candidate.releaseDate );

   if ( FPicker ) {
      FPicker->setRows( rows );
      return;
   }
   if ( rows.isEmpty() ) {
      handleGameInfoFailed( Cst_GameNotFound, FNotFoundDetails );
      return;
   }

   Frm_PickGame dialog( this );
   dialog.setHint( QStringLiteral( "ScreenScraper has no game for this ROM. Pick the closest "
                                   "one, or change the name and search again (each search "
                                   "counts against your ScreenScraper quota)." ) );
   dialog.setLiveSearch( false );
   dialog.setRows( rows );
   connect( &dialog, &Frm_PickGame::searchRequested, this, &ScrapePanel::searchScreenScraper );

   FPicker = &dialog;
   const int row = dialog.Execute( FSearchText );
   FPicker = nullptr;
   FAwaitingSearch = false;

   if ( row < 0 ) {
      setBusy( false );
      return;
   }
   const Result<ScrapedInfo> info = parseScrapeResponse(
      FCandidates.at( row ).xml, mediaTypes(), Chk_Video->isChecked(), FLanguage );
   if ( info )
      handleGameInfo( info.value() );
   else
      handleGameInfoFailed( info.error(), {} );
}

void ScrapePanel::handleSearchFailed( const QString& aMessage, const QString& aDetails )
{
   if ( !FPicker ) {
      FAwaitingSearch = false;
      handleGameInfoFailed( aMessage, aDetails );
      return;
   }
   QMessageBox::warning( FPicker, QStringLiteral( "GameList Editor Qt" ), aMessage );
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

void ScrapePanel::handleGameInfoFailed( const QString& aMessage, const QString& aDetails )
{
   setBusy( false );

   QMessageBox box( QMessageBox::Warning, QStringLiteral( "GameList Editor Qt" ), aMessage, QMessageBox::Ok,
                    this );
   if ( aDetails.isEmpty() ) {
      box.exec();
      return;
   }

   box.setDetailedText( aDetails );
   QPushButton* exportButton =
      box.addButton( QStringLiteral( "Export..." ), QMessageBox::ActionRole );

   // Any button closes a QMessageBox, so Export reopens it afterwards.
   for ( ;; ) {
      box.exec();
      if ( box.clickedButton() != exportButton )
         break;

      const QString path = QFileDialog::getSaveFileName(
         this, QStringLiteral( "Export server reply" ),
         QDir::home().filePath( QStringLiteral( "scrape-reply.txt" ) ),
         QStringLiteral( "Text files (*.txt);;All files (*)" ) );
      if ( path.isEmpty() )
         continue;

      QFile file( path );
      if ( !file.open( QIODevice::WriteOnly | QIODevice::Truncate ) ||
           file.write( aDetails.toUtf8() ) < 0 )
         QMessageBox::warning( this, QStringLiteral( "GameList Editor Qt" ),
                               QStringLiteral( "Could not write %1:\n%2" )
                                  .arg( path, file.errorString() ) );
   }
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
   setBusy( false );
   FHasScrapedData = true;
   enableScrapeComponents( true );
   updateSaveEnabled();
}

void ScrapePanel::save()
{
   if ( !FGamelist || FIndex < 0 )
      return;

   // The guard can save, which may move the selection and clear the scraped data.
   if ( FLeaveGuard && !FLeaveGuard() )
      return;
   if ( !FGamelist || FIndex < 0 || !FHasScrapedData )
      return;

   // Same busy look as a scrape. report() undoes it on every way out.
   setBusy( true );
   QGuiApplication::setOverrideCursor( Qt::WaitCursor );

   // The video is only a link until now, so it is downloaded here, which needs one
   // round trip before there is a file to hand to Gamelist.
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
                              report( Error{ aMessage } );
                           } );
      FScraper.downloadToFile( FLastInfo.videoLink, temp );
   } else {
      // Deferred one turn of the event loop so the busy state paints before
      // the (blocking) image and XML writes start.
      QTimer::singleShot( 0, this, [this] { finishSave( QString() ); } );
   }
}

void ScrapePanel::finishSave( const QString& aVideoPath )
{
   const int index = FIndex;
   const bool picture = Chk_ScrapePicture->isChecked() && !FSelectedPicture.isNull();
   const Result<> result =
      applyScrape( *FGamelist, index, fields(), Chk_ScrapeInfos->isChecked(),
                   picture ? FSelectedPicture : QImage(), aVideoPath );

   // Also when an earlier step failed and the video was never used.
   if ( !aVideoPath.isEmpty() )
      QFile::remove( aVideoPath );

   report( result );
}

void ScrapePanel::report( const Result<>& aResult )
{
   QGuiApplication::restoreOverrideCursor();
   setBusy( false );

   if ( !aResult )
      QMessageBox::warning( this, QStringLiteral( "GameList Editor Qt" ), aResult.error() );

   emit gamesChanged();
}
