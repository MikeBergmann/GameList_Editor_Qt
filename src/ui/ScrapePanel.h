#pragma once

#include "Game.h"
#include "Scraper.h"

#include <QImage>
#include <QWidget>

class Gamelist;
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QScrollArea;

// Tbs_Scrape: the ~30 widgets of the scrape tab, and the picture the user
// chose out of the results. Scrapes only ever apply to the one game currently
// selected in the main tab (FScrapedGame), so this addresses that one index
// into the Gamelist rather than taking a selection like GameEditPanel does.
//
// It owns its own Scraper - the network request, unlike the gamelist edits,
// has no other consumer - and produces the same GameFields shape
// GameEditPanel does, so Gamelist::setFields needs no aScrape flag.
class ScrapePanel : public QWidget
{
   Q_OBJECT

public:
   explicit ScrapePanel( QWidget* aParent = nullptr );

   // Borrowed, not owned. Null means no system is loaded.
   void setGamelist( Gamelist* aGamelist );

   // Index into the Gamelist of the game to scrape, or -1 for none - which is
   // what Tbs_Scrape.TabVisible used to hide for a batch selection.
   void setSelection( int aIndex );

   // Tbs_ScrapeShow (3460): reads the same QSettings Frm_Network writes -
   // proxy, ScreenScraper credentials, scrape language - and clears the Save
   // Picture/Save Video checkboxes left over from whatever was scraped last.
   void activate();

   // Exposed so a test can emit these directly - the way tst_scraper.cpp
   // drives parseScrapeResponse with no QNetworkAccessManager at all - rather
   // than needing a real network round trip to exercise this panel.
   Scraper& scraper() { return FScraper; }

signals:
   // The gamelist changed under the list view. Same signal shape as
   // GameEditPanel's, for the same reason: the form re-runs the filter.
   void gamesChanged();

private:
   void buildLayout();
   void requestScrape();  // Btn_ScrapeClick (4013)
   void save();           // Btn_ScrapeSaveClick (4036)
   void finishSave( const QString& aVideoPath );
   void selectPicture( const QImage& aImage );  // ImgScrapedClick (4075)
   void convertCase( bool aUpper );             // ConvertScrapeToUpOrLow (3927)
   void emptyScrapeFields();                    // EmptyScrapeFields (1972)
   void clearScrapeMedia();                     // ClearScrapeMedia (3498)
   void enableScrapeComponents( bool aEnabled ); // EnableScrapeComponents (3948)
   void updateSaveEnabled();  // Chk_ScrapeClick (4042) folded with the above
   void showSelectedPicture();
   void report( bool aOk, const QString& aError );

   void handleGameInfo( const ScrapedInfo& aInfo );
   void handleGameInfoFailed( const QString& aMessage );
   void handleMediaReady( int aIndex, const QImage& aImage );
   void handleMediaFinished();

   GameFields fields() const;
   void setFields( const GameFields& aFields );
   QSet<QString> mediaTypes() const;  // The 8 picture checkboxes, as Cst_Media* values

   Gamelist* FGamelist = nullptr;
   int FIndex = -1;

   Scraper FScraper;
   ScrapedInfo FLastInfo;
   bool FHasScrapedData = false;
   QImage FSelectedPicture;

   // Read from QSettings by activate(), the same keys Frm_Network writes.
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

   // Img_ScrapeBackground folds into this label's placeholder text, the way
   // GameEditPanel's Img_Game already absorbed Img_BackGround. Img_ScreenScraper
   // was decorative chrome and is dropped outright, per the Assets decision.
   QLabel* Img_Scrape = nullptr;
   QProgressBar* Img_Loading = nullptr;  // busy indicator; Delphi's was a spinning icon
   QScrollArea* Scl_Pictures = nullptr;
   QHBoxLayout* FPicturesLayout = nullptr;

   QPushButton* Btn_Scrape = nullptr;
   QPushButton* Btn_ScrapeSave = nullptr;
   QPushButton* Btn_ScrapeUpper = nullptr;
   QPushButton* Btn_ScrapeLower = nullptr;
};
