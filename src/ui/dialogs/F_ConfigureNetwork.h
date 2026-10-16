#pragma once

#include <QDialog>

class QComboBox;
class QGroupBox;
class QLineEdit;

// ScreenScraper credentials, the language scraped metadata is requested in, and
// the HTTP proxy.
class Frm_Network : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_Network( QWidget* aParent = nullptr );

   // Loads the current settings, shows the dialog, and writes them back if the user saves.
   // Callers re-read QSettings afterwards.
   void Execute();

private:
   void loadFromSettings();
   void saveToSettings();

   QLineEdit* Edt_ScreenLogin = nullptr;
   QLineEdit* Edt_ScreenPwd = nullptr;
   QComboBox* Cbx_ScrapeLanguage = nullptr;

   // Checking the group box enables the proxy fields.
   QGroupBox* Grp_Proxy = nullptr;
   QLineEdit* Edt_ProxyServer = nullptr;
   QLineEdit* Edt_ProxyPort = nullptr;
   QLineEdit* Edt_ProxyUser = nullptr;
   QLineEdit* Edt_ProxyPwd = nullptr;
};
