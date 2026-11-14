#pragma once

#include "Game.h"
#include "LaunchBoxDb.h"
#include "LaunchBoxDump.h"
#include "Scraper.h"

#include <QImage>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include <memory>

class GamesController;
class LibraryController;
class QThread;

// Scrapes the game selected in GamesController from screenscraper.fr or from the local LaunchBox
// dump: the nine text fields, the pictures, the video (ScreenScraper only), and the save of
// whatever the user ticked. Owns its Scraper and its LaunchBoxDump.
class ScrapeController : public QObject
{
   Q_OBJECT
   Q_PROPERTY( bool busy READ isBusy NOTIFY busyChanged )
   Q_PROPERTY( bool canScrape READ canScrape NOTIFY busyChanged )
   Q_PROPERTY( QString error READ error NOTIFY resultChanged )
   Q_PROPERTY( QVariantMap values READ values NOTIFY resultChanged )
   Q_PROPERTY( bool hasResult READ hasResult NOTIFY resultChanged )
   Q_PROPERTY( QVariantMap media READ media NOTIFY mediaChanged )
   Q_PROPERTY( QString manualCrc READ manualCrc WRITE setManualCrc NOTIFY mediaChanged )
   Q_PROPERTY( int generation READ generation NOTIFY picturesChanged )
   Q_PROPERTY( int pictureCount READ pictureCount NOTIFY picturesChanged )
   Q_PROPERTY( int selectedPicture READ selectedPicture NOTIFY picturesChanged )
   Q_PROPERTY( bool hasVideo READ hasVideo NOTIFY resultChanged )
   Q_PROPERTY( bool savePicture READ savePicture WRITE setSavePicture NOTIFY saveChoiceChanged )
   Q_PROPERTY( bool saveVideo READ saveVideo WRITE setSaveVideo NOTIFY saveChoiceChanged )
   Q_PROPERTY( bool saveInfos READ saveInfos WRITE setSaveInfos NOTIFY saveChoiceChanged )
   Q_PROPERTY( bool canSave READ canSave NOTIFY saveChoiceChanged )
   Q_PROPERTY( bool picking READ isPicking NOTIFY pickingChanged )
   Q_PROPERTY( QStringList candidates READ candidates NOTIFY pickingChanged )
   Q_PROPERTY( QString searchText READ searchText NOTIFY pickingChanged )
   Q_PROPERTY( int source READ source WRITE setSource NOTIFY sourceChanged )
   Q_PROPERTY( bool launchBoxReady READ launchBoxReady NOTIFY dumpChanged )
   Q_PROPERTY( int dumpProgress READ dumpProgress NOTIFY dumpChanged )
   Q_PROPERTY( QString note READ note NOTIFY resultChanged )

public:
   enum Source { ScreenScraper = 0, LaunchBox = 1 };
   Q_ENUM( Source )

   ScrapeController( LibraryController* aLibrary, GamesController* aGames,
                     QObject* aParent = nullptr,
                     const QString& aLaunchBoxDir = LaunchBoxDump::defaultCacheDir() );
   ~ScrapeController() override;

   bool isBusy() const { return FBusy; }
   // A game whose ROM is gone has nothing to hash.
   bool canScrape() const;
   const QString& error() const { return FError; }
   // The nine scraped text fields by key (name, region, releaseDate, genre, developer,
   // publisher, rating, players, description), as the user left them.
   QVariantMap values() const;
   bool hasResult() const { return FHasResult; }
   // What to fetch, by key: box2d, box3d, mix1, mix2, screenshot, title, arcadeBox, wheel, video.
   QVariantMap media() const;
   // Non-empty: send only this CRC32, no ROM name or size.
   const QString& manualCrc() const { return FManualCrc; }
   // Changes with every new scrape, so an image:// URL built from it is never served stale.
   int generation() const { return FGeneration; }
   int pictureCount() const { return static_cast<int>( FPictures.size() ); }
   // -1 for none.
   int selectedPicture() const { return FSelected; }
   bool hasVideo() const { return !FInfo.videoLink.isEmpty(); }
   bool savePicture() const { return FSavePicture; }
   bool saveVideo() const { return FSaveVideo; }
   bool saveInfos() const { return FSaveInfos; }
   bool canSave() const;
   // ScreenScraper did not know the ROM and lists its closest guesses; busy stays on.
   bool isPicking() const { return FPicking; }
   const QStringList& candidates() const { return FCandidateRows; }
   const QString& searchText() const { return FSearchText; }
   // Where results come from; kept in QSettings. Anything but LaunchBox reads as ScreenScraper.
   int source() const { return FSource; }
   // The dump is unpacked and a scrape can read it.
   bool launchBoxReady() const { return FDump.isCached(); }
   // Percent of the dump download, -1 while the size is unknown or the dump is unpacking.
   int dumpProgress() const { return FDumpProgress; }
   // A remark about the last result, e.g. an ambiguous LaunchBox name.
   const QString& note() const { return FNote; }

   // Null for an index out of range. Read by the image provider on the GUI thread.
   QImage picture( int aIndex ) const;

   void setManualCrc( const QString& aCrc );
   void setSavePicture( bool aOn );
   void setSaveVideo( bool aOn );
   void setSaveInfos( bool aOn );
   void setSource( int aSource );

   // Downloads the LaunchBox dump, or checks that it is current. Busy until it is done.
   Q_INVOKABLE void refreshLaunchBox();

   // Reads the login, language and proxy from QSettings, hashes the ROM, then asks.
   Q_INVOKABLE void scrape();
   // Unknown keys are ignored.
   Q_INVOKABLE void setField( const QString& aKey, const QVariant& aValue );
   Q_INVOKABLE void setMedia( const QString& aKey, bool aOn );
   Q_INVOKABLE void selectPicture( int aIndex );
   // Writes what is ticked; answers with saved() or error().
   Q_INVOKABLE void save();
   // Drops the result and every picture.
   Q_INVOKABLE void reset();

   // The name search of the picker. A reply replaces candidates (at once for LaunchBox).
   Q_INVOKABLE void searchCandidates( const QString& aText );
   Q_INVOKABLE void pickCandidate( int aRow );
   Q_INVOKABLE void cancelPick();

   // Exposed so tests can emit its signals without a network round trip.
   Scraper& scraper() { return FScraper; }

signals:
   void busyChanged();
   void resultChanged();
   void mediaChanged();
   void picturesChanged();
   void saveChoiceChanged();
   void pickingChanged();
   void sourceChanged();
   void dumpChanged();
   // The Gamelist changed under the edit page.
   void saved();

private:
   void sendRequest();
   void requestLaunchBox();
   void buildLaunchBoxIndex( const QString& aPlatform );
   void lookupLaunchBox();
   void showLaunchBoxCandidates( const QString& aText );
   void fail( const QString& aMessage );
   void setBusy( bool aBusy );
   void setPicking( bool aPicking );
   void handleGameInfo( const ScrapedInfo& aInfo );
   void handleGameInfoFailed( const QString& aMessage );
   void handleSearchReady( const QVector<ScrapeCandidate>& aCandidates );
   void handleMediaReady( const QImage& aImage );
   void handleMediaFinished();
   void finishSave( const QString& aVideoPath );
   void applyNetworkSettings();
   const Game* game() const;
   QSet<QString> mediaTypes() const;
   ScrapeRequest baseRequest() const;

   LibraryController* FLibrary;
   GamesController* FGames;
   Scraper FScraper;

   QThread* FHashThread = nullptr;
   QThread* FIndexThread = nullptr;
   bool FBusy = false;
   bool FHasResult = false;
   QString FError;

   ScrapedInfo FInfo;
   GameFields FFields;
   QVector<QImage> FPictures;
   int FSelected = -1;
   int FGeneration = 0;

   QVariantMap FMedia;
   QString FManualCrc;
   bool FSavePicture = false;
   bool FSaveVideo = false;
   bool FSaveInfos = true;

   QString FLogin;
   QString FPassword;
   LangName FLanguage = lnEnglish;

   bool FPicking = false;
   bool FAwaitingSearch = false;
   QString FSearchText;
   QString FNotFoundMessage;
   QVector<ScrapeCandidate> FCandidates;
   QStringList FCandidateRows;

   bool FAwaitingVideo = false;

   int FSource = ScreenScraper;
   QString FNote;
   LaunchBoxDump FDump;
   int FDumpProgress = -1;
   std::shared_ptr<LaunchBoxDb> FLbIndex;
   QString FLbPlatform;
   LaunchBoxQuery FLbQuery;
   QVector<LaunchBoxCandidate> FLbCandidates;
   bool FLbPicking = false;
};
