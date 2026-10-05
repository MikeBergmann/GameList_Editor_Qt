#pragma once

#include <QDialog>

class QComboBox;
class QGroupBox;
class QLineEdit;

// ScreenScraper credentials, the language scraped metadata is requested in, and
// the HTTP proxy. Despite the unit name this was never the SSH dialog.
class Frm_Network : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_Network( QWidget* aParent = nullptr );

   // Loads the current settings, shows the dialog, and writes them back if the
   // user saves. The Delphi took all seven values as arguments and F_Main read
   // the ini straight back afterwards to learn what had changed; since both
   // ends now talk to the same QSettings, neither half of that is needed.
   void Execute();

private:
   void loadFromSettings();
   void saveToSettings();

   QLineEdit* Edt_ScreenLogin = nullptr;
   QLineEdit* Edt_ScreenPwd = nullptr;
   QComboBox* Cbx_ScrapeLanguage = nullptr;

   // Was Chk_Proxy plus an EnableControls() that greyed out six widgets.
   QGroupBox* Grp_Proxy = nullptr;
   QLineEdit* Edt_ProxyServer = nullptr;
   QLineEdit* Edt_ProxyPort = nullptr;
   QLineEdit* Edt_ProxyUser = nullptr;
   QLineEdit* Edt_ProxyPwd = nullptr;
};
