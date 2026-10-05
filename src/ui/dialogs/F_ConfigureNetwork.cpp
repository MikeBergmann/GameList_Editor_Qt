#include "F_ConfigureNetwork.h"

#include "Resources.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QIntValidator>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {

// Only used to fill the combo below, so it stays here rather than in Resources.
// Order is TLangName's, which makes the combo index the stored value.
constexpr const char* Cst_LangNameFull[lnCount] = { "French", "German", "English", "Spanish",
                                                    "Portuguese (BR)" };

constexpr int Cst_MaxPort = 65535;

}  // namespace

Frm_Network::Frm_Network( QWidget* aParent ) : QDialog( aParent )
{
   setWindowTitle( QStringLiteral( "Network configuration" ) );

   QGroupBox* scraper = new QGroupBox( QStringLiteral( "ScreenScraper" ), this );
   QFormLayout* scraperLayout = new QFormLayout( scraper );

   Edt_ScreenLogin = new QLineEdit( scraper );
   Edt_ScreenPwd = new QLineEdit( scraper );

   // The DFM showed both passwords in clear. They are still stored in clear -
   // that is a separate decision - but there is no reason to put them on screen
   // for anyone standing behind you.
   Edt_ScreenPwd->setEchoMode( QLineEdit::PasswordEchoOnEdit );

   Cbx_ScrapeLanguage = new QComboBox( scraper );
   for ( int lang = 0; lang < lnCount; ++lang )
      Cbx_ScrapeLanguage->addItem( QLatin1String( Cst_LangNameFull[lang] ) );

   scraperLayout->addRow( QStringLiteral( "Login" ), Edt_ScreenLogin );
   scraperLayout->addRow( QStringLiteral( "Password" ), Edt_ScreenPwd );
   scraperLayout->addRow( QStringLiteral( "Scrape language" ), Cbx_ScrapeLanguage );

   Grp_Proxy = new QGroupBox( QStringLiteral( "Use a proxy" ), this );
   Grp_Proxy->setCheckable( true );
   QFormLayout* proxyLayout = new QFormLayout( Grp_Proxy );

   Edt_ProxyServer = new QLineEdit( Grp_Proxy );
   Edt_ProxyPort = new QLineEdit( Grp_Proxy );
   Edt_ProxyPort->setValidator( new QIntValidator( 0, Cst_MaxPort, Edt_ProxyPort ) );
   Edt_ProxyUser = new QLineEdit( Grp_Proxy );
   Edt_ProxyPwd = new QLineEdit( Grp_Proxy );
   Edt_ProxyPwd->setEchoMode( QLineEdit::PasswordEchoOnEdit );

   proxyLayout->addRow( QStringLiteral( "Server" ), Edt_ProxyServer );
   proxyLayout->addRow( QStringLiteral( "Port" ), Edt_ProxyPort );
   proxyLayout->addRow( QStringLiteral( "Username" ), Edt_ProxyUser );
   proxyLayout->addRow( QStringLiteral( "Password" ), Edt_ProxyPwd );

   QDialogButtonBox* buttons =
      new QDialogButtonBox( QDialogButtonBox::Save | QDialogButtonBox::Cancel, this );
   connect( buttons, &QDialogButtonBox::accepted, this, [this] {
      saveToSettings();
      accept();
   } );
   connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

   QVBoxLayout* layout = new QVBoxLayout( this );
   layout->addWidget( scraper );
   layout->addWidget( Grp_Proxy );
   layout->addWidget( buttons );
}

void Frm_Network::Execute()
{
   loadFromSettings();
   exec();
}

void Frm_Network::loadFromSettings()
{
   QSettings settings;
   settings.beginGroup( QLatin1String( Cst_IniOptions ) );

   Edt_ScreenLogin->setText( settings.value( QLatin1String( Cst_IniSSUser ) ).toString() );
   Edt_ScreenPwd->setText( settings.value( QLatin1String( Cst_IniSSPwd ) ).toString() );
   Cbx_ScrapeLanguage->setCurrentIndex( langFromIndex(
      settings.value( QLatin1String( Cst_IniLanguage ), Cst_IniLanguageDefault ).toInt() ) );

   Grp_Proxy->setChecked( settings.value( QLatin1String( Cst_IniProxyUse ), false ).toBool() );
   Edt_ProxyServer->setText( settings.value( QLatin1String( Cst_IniProxyServer ) ).toString() );
   Edt_ProxyPort->setText( settings.value( QLatin1String( Cst_IniProxyPort ) ).toString() );
   Edt_ProxyUser->setText( settings.value( QLatin1String( Cst_IniProxyUser ) ).toString() );
   Edt_ProxyPwd->setText( settings.value( QLatin1String( Cst_IniProxyPwd ) ).toString() );
}

void Frm_Network::saveToSettings()
{
   QSettings settings;
   settings.beginGroup( QLatin1String( Cst_IniOptions ) );

   settings.setValue( QLatin1String( Cst_IniSSUser ), Edt_ScreenLogin->text() );
   settings.setValue( QLatin1String( Cst_IniSSPwd ), Edt_ScreenPwd->text() );
   settings.setValue( QLatin1String( Cst_IniLanguage ), Cbx_ScrapeLanguage->currentIndex() );

   settings.setValue( QLatin1String( Cst_IniProxyUse ), Grp_Proxy->isChecked() );
   settings.setValue( QLatin1String( Cst_IniProxyServer ), Edt_ProxyServer->text() );
   // An empty port is stored as 0, as the Delphi did: the scraper treats it as
   // "no port given" and a missing key would read back as an empty string.
   settings.setValue( QLatin1String( Cst_IniProxyPort ), Edt_ProxyPort->text().isEmpty()
                                                            ? QStringLiteral( "0" )
                                                            : Edt_ProxyPort->text() );
   settings.setValue( QLatin1String( Cst_IniProxyUser ), Edt_ProxyUser->text() );
   settings.setValue( QLatin1String( Cst_IniProxyPwd ), Edt_ProxyPwd->text() );
}
