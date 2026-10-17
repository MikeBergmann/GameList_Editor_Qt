#pragma once

#include "Game.h"
#include "Scraper.h"

#include <QImage>
#include <QWidget>

#include <functional>

class Gamelist;
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QScrollArea;

// The scrape tab. Applies to the one selected game, and owns its Scraper.
class ScrapePanel : public QWidget
{
   Q_OBJECT

public:
   explicit ScrapePanel( QWidget* aParent = nullptr );

   // Borrowed, not owned. Null means no system is loaded.
   void setGamelist( Gamelist* aGamelist );

   // -1 for none, e.g. a batch selection.
   void setSelection( int aIndex );

   // Re-reads the QSettings keys and clears the Save checkboxes.
   void activate();

   // Asked before a save, which reloads the edit tab; false cancels. Unset: go ahead.
   void setLeaveGuard( std::function<bool()> aGuard ) { FLeaveGuard = std::move( aGuard ); }

   // Exposed so tests can emit its signals without a network round trip.
   Scraper& scraper() { return FScraper; }

signals:
   void gamesChanged();

   // The selection and the Gamelist must stay put until the reply lands.
   void busyChanged( bool aBusy );

private:
   void buildLayout();
   void setBusy( bool aBusy );
   void requestScrape();
   void save();
   void finishSave( const QString& aVideoPath );
   void selectPicture( const QImage& aImage );
   void convertCase( bool aUpper );
   void emptyScrapeFields();
   void clearScrapeMedia();
   void enableScrapeComponents( bool aEnabled );
   void updateSaveEnabled();
   void showSelectedPicture();
   void report( const Result<>& aResult );

   void handleGameInfo( const ScrapedInfo& aInfo );
   void handleGameInfoFailed( const QString& aMessage, const QString& aDetails );
   void handleMediaReady( int aIndex, const QImage& aImage );
   void handleMediaFinished();

   GameFields fields() const;
   void setFields( const GameFields& aFields );
   QSet<QString> mediaTypes() const;  // The 8 picture checkboxes, as Cst_Media* values

   std::function<bool()> FLeaveGuard;
   Gamelist* FGamelist = nullptr;
   int FIndex = -1;

   Scraper FScraper;
   ScrapedInfo FLastInfo;
   bool FHasScrapedData = false;
   QImage FSelectedPicture;

   QString FSSLogin;
   QString FSSPassword;

   LangName FLanguage = lnEnglish;

   QLineEdit* Edt_ScrapeName = nullptr;
   QLineEdit* Edt_ScrapeRegion = nullptr;
   QLineEdit* Edt_ScrapeDate = nullptr;
   QLineEdit* Edt_ScrapeGenre = nullptr;
   QLineEdit* Edt_ScrapeDeveloper = nullptr;
   QLineEdit* Edt_ScrapePublisher = nullptr;
   QLineEdit* Edt_ScrapeRating = nullptr;
   QLineEdit* Edt_ScrapePlayers = nullptr;
   QPlainTextEdit* Mmo_ScrapeDescription = nullptr;
   QLineEdit* Edt_ScrapeRomPath = nullptr;

   QCheckBox* Chk_Box2D = nullptr;
   QCheckBox* Chk_Box3D = nullptr;
   QCheckBox* Chk_Mix1 = nullptr;
   QCheckBox* Chk_Mix2 = nullptr;
   QCheckBox* Chk_Screenshot = nullptr;
   QCheckBox* Chk_Title = nullptr;
   QCheckBox* Chk_ArcadeBox = nullptr;
   QCheckBox* Chk_Wheel = nullptr;
   QCheckBox* Chk_Video = nullptr;

   QCheckBox* Chk_ManualCRC = nullptr;
   QLineEdit* Edt_ManualCRC = nullptr;

   QCheckBox* Chk_ScrapePicture = nullptr;
   QCheckBox* Chk_ScrapeVideo = nullptr;
   QCheckBox* Chk_ScrapeInfos = nullptr;

   QLabel* Img_Scrape = nullptr;
   QProgressBar* Img_Loading = nullptr;  // busy indicator
   QScrollArea* Scl_Pictures = nullptr;
   QHBoxLayout* FPicturesLayout = nullptr;

   QPushButton* Btn_Scrape = nullptr;
   QPushButton* Btn_ScrapeSave = nullptr;
   QPushButton* Btn_ScrapeUpper = nullptr;
   QPushButton* Btn_ScrapeLower = nullptr;
};
