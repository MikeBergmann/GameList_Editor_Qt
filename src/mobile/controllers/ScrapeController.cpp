#include "ScrapeController.h"

#include "GamesController.h"
#include "LibraryController.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkProxy>
#include <QSettings>
#include <QThread>
#include <QTimer>

namespace {

struct MediaKey
{
   const char* key;
   QLatin1StringView type;
};

const MediaKey Cst_MediaKeys[] = {
   { "box2d", Cst_MediaBox2d },       { "box3d", Cst_MediaBox3d },
   { "mix1", Cst_MediaMix1 },         { "mix2", Cst_MediaMix2 },
   { "screenshot", Cst_MediaScreenShot }, { "title", Cst_MediaSsTitle },
   { "arcadeBox", Cst_MediaArcadeBox1 },  { "wheel", Cst_MediaWheel },
};

constexpr const char* Cst_VideoKey = "video";

struct TextField
{
   const char* key;
   QString GameFields::* member;
};

const TextField Cst_TextFields[] = {
   { "name", &GameFields::name },           { "region", &GameFields::region },
   { "releaseDate", &GameFields::releaseDate }, { "genre", &GameFields::genre },
   { "developer", &GameFields::developer }, { "publisher", &GameFields::publisher },
   { "rating", &GameFields::rating },       { "players", &GameFields::players },
   { "description", &GameFields::description },
};

QString candidateRow( const ScrapeCandidate& aCandidate )
{
   return aCandidate.releaseDate.isEmpty()
             ? aCandidate.name
             : QStringLiteral( "%1 (%2)" ).arg( aCandidate.name, aCandidate.releaseDate );
}

}  // namespace

ScrapeController::ScrapeController( LibraryController* aLibrary, GamesController* aGames,
                                    QObject* aParent )
   : QObject( aParent ), FLibrary( aLibrary ), FGames( aGames )
{
   for ( const MediaKey& key : Cst_MediaKeys )
      FMedia.insert( QLatin1String( key.key ), true );
   FMedia.insert( QLatin1String( Cst_VideoKey ), true );

   connect( FGames, &GamesController::currentChanged, this, &ScrapeController::reset );
   connect( FLibrary, &LibraryController::gamelistChanged, this, &ScrapeController::reset );

   connect( &FScraper, &Scraper::gameInfoReady, this, &ScrapeController::handleGameInfo );
   connect( &FScraper, &Scraper::gameInfoFailed, this,
            [this]( const QString& aMessage, const QString& ) { handleGameInfoFailed( aMessage ); } );
   connect( &FScraper, &Scraper::searchReady, this, &ScrapeController::handleSearchReady );
   connect( &FScraper, &Scraper::searchFailed, this, [this]( const QString& aMessage, const QString& ) {
      FAwaitingSearch = false;
      if ( !FPicking ) {
         fail( aMessage );
         return;
      }
      FError = aMessage;
      emit resultChanged();
   } );
   connect( &FScraper, &Scraper::mediaReady, this,
            [this]( int, const QImage& aImage ) { handleMediaReady( aImage ); } );
   connect( &FScraper, &Scraper::mediaFinished, this, &ScrapeController::handleMediaFinished );
   connect( &FScraper, &Scraper::fileSaved, this, [this]( const QString& aPath ) {
      if ( FAwaitingVideo )
         finishSave( aPath );
   } );
   connect( &FScraper, &Scraper::fileSaveFailed, this, [this]( const QString&, const QString& aMessage ) {
      if ( FAwaitingVideo ) {
         FAwaitingVideo = false;
         fail( aMessage );
      }
   } );
}

ScrapeController::~ScrapeController()
{
   if ( FHashThread )
      FHashThread->wait();
}

const Game* ScrapeController::game() const
{
   const Gamelist* list = FLibrary->gamelist();
   const int index = FGames->current();
   return list && index >= 0 && index < list->count() ? &list->at( index ) : nullptr;
}

bool ScrapeController::canScrape() const
{
   const Game* current = game();
   return current && !current->isOrphan && !FBusy;
}

bool ScrapeController::canSave() const
{
   return FHasResult && !FBusy &&
          ( FSaveInfos || ( FSavePicture && FSelected >= 0 ) || ( FSaveVideo && hasVideo() ) );
}

QVariantMap ScrapeController::values() const
{
   QVariantMap result;
   for ( const TextField& field : Cst_TextFields )
      result.insert( QLatin1String( field.key ), FFields.*field.member );
   return result;
}

QVariantMap ScrapeController::media() const
{
   return FMedia;
}

QSet<QString> ScrapeController::mediaTypes() const
{
   QSet<QString> types;
   for ( const MediaKey& key : Cst_MediaKeys )
      if ( FMedia.value( QLatin1String( key.key ) ).toBool() )
         types.insert( key.type );
   return types;
}

QImage ScrapeController::picture( int aIndex ) const
{
   return aIndex >= 0 && aIndex < FPictures.size() ? FPictures.at( aIndex ) : QImage();
}

void ScrapeController::setManualCrc( const QString& aCrc )
{
   FManualCrc = aCrc.trimmed();
   emit mediaChanged();
}

void ScrapeController::setSavePicture( bool aOn )
{
   FSavePicture = aOn && FSelected >= 0;
   emit saveChoiceChanged();
}

void ScrapeController::setSaveVideo( bool aOn )
{
   FSaveVideo = aOn && hasVideo();
   emit saveChoiceChanged();
}

void ScrapeController::setSaveInfos( bool aOn )
{
   FSaveInfos = aOn;
   emit saveChoiceChanged();
}

void ScrapeController::setField( const QString& aKey, const QVariant& aValue )
{
   for ( const TextField& field : Cst_TextFields ) {
      if ( aKey == QLatin1String( field.key ) ) {
         FFields.*field.member = aValue.toString();
         emit resultChanged();
         return;
      }
   }
}

void ScrapeController::setMedia( const QString& aKey, bool aOn )
{
   if ( !FMedia.contains( aKey ) )
      return;
   FMedia[aKey] = aOn;
   emit mediaChanged();
}

void ScrapeController::selectPicture( int aIndex )
{
   if ( aIndex < 0 || aIndex >= FPictures.size() )
      return;
   FSelected = aIndex;
   FSavePicture = true;
   emit picturesChanged();
   emit saveChoiceChanged();
}

void ScrapeController::reset()
{
   FHasResult = false;
   FError.clear();
   FInfo = ScrapedInfo();
   FFields = GameFields();
   FPictures.clear();
   FSelected = -1;
   ++FGeneration;
   FSavePicture = false;
   FSaveVideo = false;
   FAwaitingVideo = false;
   FAwaitingSearch = false;
   FPicking = false;
   FCandidates.clear();
   FCandidateRows.clear();
   FBusy = false;

   emit resultChanged();
   emit picturesChanged();
   emit saveChoiceChanged();
   emit pickingChanged();
   emit busyChanged();
}

void ScrapeController::setBusy( bool aBusy )
{
   if ( FBusy == aBusy )
      return;
   FBusy = aBusy;
   emit busyChanged();
   emit saveChoiceChanged();
}

void ScrapeController::setPicking( bool aPicking )
{
   FPicking = aPicking;
   emit pickingChanged();
}

void ScrapeController::fail( const QString& aMessage )
{
   FError = aMessage;
   FPicking = false;
   setBusy( false );
   emit resultChanged();
   emit pickingChanged();
}

void ScrapeController::applyNetworkSettings()
{
   QSettings settings;
   settings.beginGroup( Cst_IniOptions );

   if ( settings.value( Cst_IniProxyUse, false ).toBool() ) {
      QNetworkProxy::setApplicationProxy( QNetworkProxy(
         QNetworkProxy::HttpProxy, settings.value( Cst_IniProxyServer ).toString(),
         static_cast<quint16>( settings.value( Cst_IniProxyPort, 0 ).toInt() ),
         settings.value( Cst_IniProxyUser ).toString(),
         settings.value( Cst_IniProxyPwd ).toString() ) );
   } else {
      QNetworkProxy::setApplicationProxy( QNetworkProxy::NoProxy );
   }

   FLogin = settings.value( Cst_IniSSUser ).toString();
   FPassword = settings.value( Cst_IniSSPwd ).toString();
   FLanguage = langFromIndex( settings.value( Cst_IniLanguage, Cst_IniLanguageDefault ).toInt() );
}

ScrapeRequest ScrapeController::baseRequest() const
{
   ScrapeRequest request;
   if ( const Gamelist* list = FLibrary->gamelist() )
      request.systemId = QLatin1String( systemInfo( list->system().kind ).ssId );
   request.ssLogin = FLogin;
   request.ssPassword = FPassword;
   return request;
}

// Limitation: Scraper has no cancel and its replies carry no request id, so the page keeps the
// user from leaving while busy; a reply landing after reset() would fill the wrong game.
void ScrapeController::scrape()
{
   if ( !canScrape() )
      return;

   reset();
   applyNetworkSettings();
   setBusy( true );

   if ( !FManualCrc.isEmpty() ) {
      sendRequest();
      return;
   }

   // Hashing a large disc image would run into Android's ~5 s ANR limit on the UI thread.
   Gamelist* list = FLibrary->gamelist();
   const int index = FGames->current();
   FHashThread = QThread::create( [list, index] { list->ensureHashes( index ); } );
   FHashThread->setParent( this );
   connect( FHashThread, &QThread::finished, this, [this] {
      FHashThread->deleteLater();
      FHashThread = nullptr;
      sendRequest();
   } );
   FHashThread->start();
}

void ScrapeController::sendRequest()
{
   const Game* current = game();
   if ( !current ) {
      fail( QStringLiteral( "The game is no longer available." ) );
      return;
   }

   ScrapeRequest request = baseRequest();
   request.manualCrc = !FManualCrc.isEmpty();
   request.crc32 = request.manualCrc ? FManualCrc : current->crc32;
   request.romName = current->romName();
   request.romSize = QFileInfo( current->physicalRomPath ).size();

   FScraper.requestGame( request, mediaTypes(), FMedia.value( QLatin1String( Cst_VideoKey ) ).toBool(),
                         FLanguage );
}

void ScrapeController::handleGameInfoFailed( const QString& aMessage )
{
   const Game* current = game();
   if ( aMessage != Cst_GameNotFound || !current ) {
      fail( aMessage );
      return;
   }

   const QString text = current->name.isEmpty() ? current->romNameWoExt() : current->name;
   if ( text.trimmed().isEmpty() ) {
      fail( aMessage );
      return;
   }
   FNotFoundMessage = aMessage;
   searchCandidates( text );
}

void ScrapeController::searchCandidates( const QString& aText )
{
   if ( aText.trimmed().isEmpty() )
      return;

   FSearchText = aText;
   FAwaitingSearch = true;
   FError.clear();
   FScraper.searchGames( baseRequest(), aText, FLanguage );
}

void ScrapeController::handleSearchReady( const QVector<ScrapeCandidate>& aCandidates )
{
   if ( !FAwaitingSearch )
      return;
   FAwaitingSearch = false;

   FCandidates = aCandidates;
   FCandidateRows.clear();
   for ( const ScrapeCandidate& candidate : std::as_const( FCandidates ) )
      FCandidateRows << candidateRow( candidate );

   if ( FPicking ) {
      emit pickingChanged();
      return;
   }
   if ( FCandidateRows.isEmpty() ) {
      fail( FNotFoundMessage );
      return;
   }
   setPicking( true );
}

void ScrapeController::pickCandidate( int aRow )
{
   if ( !FPicking || aRow < 0 || aRow >= FCandidates.size() )
      return;

   const Result<ScrapedInfo> info =
      parseScrapeResponse( FCandidates.at( aRow ).xml, mediaTypes(),
                           FMedia.value( QLatin1String( Cst_VideoKey ) ).toBool(), FLanguage );
   setPicking( false );
   if ( info )
      handleGameInfo( info.value() );
   else
      fail( info.error() );
}

void ScrapeController::cancelPick()
{
   FAwaitingSearch = false;
   setPicking( false );
   setBusy( false );
}

void ScrapeController::handleGameInfo( const ScrapedInfo& aInfo )
{
   FInfo = aInfo;
   FFields = GameFields();
   FFields.name = aInfo.name;
   FFields.region = aInfo.region;
   FFields.releaseDate = aInfo.releaseDate;
   FFields.genre = aInfo.genre;
   FFields.developer = aInfo.developer;
   FFields.publisher = aInfo.publisher;
   FFields.rating = aInfo.rating;
   FFields.players = aInfo.players;
   FFields.description = aInfo.description;
   FSaveVideo = !aInfo.videoLink.isEmpty();
   emit resultChanged();
   emit saveChoiceChanged();

   FScraper.requestMedia( aInfo.pictures, aInfo.maxThreads );
}

void ScrapeController::handleMediaReady( const QImage& aImage )
{
   FPictures.append( aImage );
   emit picturesChanged();
}

void ScrapeController::handleMediaFinished()
{
   FHasResult = true;
   setBusy( false );
   emit resultChanged();
   emit saveChoiceChanged();
}

void ScrapeController::save()
{
   if ( !canSave() )
      return;

   FError.clear();
   setBusy( true );
   emit resultChanged();

   // The video is only a link until now: it is fetched to a temp file, which Gamelist then copies.
   if ( FSaveVideo && hasVideo() ) {
      FAwaitingVideo = true;
      FScraper.downloadToFile( FInfo.videoLink,
                               QDir::temp().filePath( QStringLiteral( "gamelisteditor-scrape-video.mp4" ) ) );
   } else {
      QTimer::singleShot( 0, this, [this] { finishSave( QString() ); } );
   }
}

void ScrapeController::finishSave( const QString& aVideoPath )
{
   FAwaitingVideo = false;
   Gamelist* list = FLibrary->gamelist();
   const int index = FGames->current();
   if ( !list || !game() ) {
      fail( QStringLiteral( "The game is no longer available." ) );
      return;
   }

   const bool picture = FSavePicture && FSelected >= 0;
   const Result<> result = applyScrape( *list, index, FFields, FSaveInfos,
                                        picture ? FPictures.at( FSelected ) : QImage(), aVideoPath );

   // Also when an earlier step failed and the video was never used.
   if ( !aVideoPath.isEmpty() )
      QFile::remove( aVideoPath );

   setBusy( false );
   if ( !result ) {
      FError = result.error();
      emit resultChanged();
      return;
   }
   FGames->refresh();
   emit saved();
}
