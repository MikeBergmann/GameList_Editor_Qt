#include "F_Help.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

struct HelpSection
{
   const char* title;
   const char* body;
};

// Was 150 lines of Red_Help.SelAttributes juggling over Rst_Title1..15 /
// Rst_Help1..15. The order is the Delphi's; "Disable Pi prompts" (4) and
// "SSH / Configuration" (13) are gone with the features they documented.
constexpr HelpSection Cst_HelpSections[] = {
   { "Choose your folder:", "Select the folder where your systems folders are stored.<br>"
                            "<br>"
                            "If that folder lives on a running Recalbox or RetroPie, stop "
                            "EmulationStation first: it rewrites <code>gamelist.xml</code> when it "
                            "exits, and would overwrite anything saved from here.<br>"
                            "<br>"
                            "Select a game to edit its fields, then click Save Changes. Select "
                            "several games to edit them together: the fields start blank, and only "
                            "the ones you fill in are written to every selected game. The name and "
                            "the pictures can only be changed one game at a time." },

   { "Enable God Mode:", "Enabling this option will let you delete games directly from the "
                         "application. A delete button will be added to the GUI, and you will be "
                         "prompted to confirm when you click it.<br>"
                         "Deleting a game removes its entry from the gamelist, deletes the "
                         "matching picture and video, and deletes the file from your folder." },

   { "Delete without prompt:",
     "Enabling this option will disable the prompt to confirm when you delete "
     "a game. Use with caution." },

   { "Auto Hash:", "Enabling this option will hash the files automatically when you click on "
                   "More Infos.<br>"
                   "Do this if you have a powerful computer, or if your systems only contain "
                   "small roms. Hashing files can be very slow, so use it with caution.<br>"
                   "If you do not enable this option, you will be asked whether to hash the "
                   "file each time you click on More Infos." },

   { "Show tips at start:", "Enabling this option will show this help window when the application "
                            "starts.<br>"
                            "You can disable it again either here, or by checking "
                            "\"Don't show again\" below before closing this window." },

   { "Use Genesis logo:", "This will let you use the Genesis logo and name instead of MegaDrive." },

   { "System - Convert to lowercase:",
     "Will convert all the text to lowercase for the whole selected system "
     "(every game will be converted)." },

   { "System - Convert to uppercase:",
     "Will convert all the text to uppercase for the whole selected system "
     "(every game will be converted)." },

   { "System - Remove region from games names:",
     "Will remove the region tag in the name (i.e. [xxxx]) for every game of "
     "the selected system." },

   { "System - Delete orphans from gamelist:",
     "Will remove from <code>gamelist.xml</code> all the games that are not "
     "physically present on your drive.<br>"
     "Orphan means the game is listed in the gamelist, but the associated rom "
     "no longer exists." },

   { "System - Delete duplicates from gamelist:",
     "Will remove all duplicates - the same game listed two or more times - "
     "from <code>gamelist.xml</code>." },

   { "Game - Convert to lowercase:",
     "Will convert all the text to lowercase for the selected game." },

   { "Game - Convert to uppercase:",
     "Will convert all the text to uppercase for the selected game." },
};

QString buildHelpHtml()
{
   QString html;
   for ( const HelpSection& section : Cst_HelpSections ) {
      html += QStringLiteral( "<p><b><u>%1</u></b><br>%2</p>" )
                 .arg( QLatin1String( section.title ), QLatin1String( section.body ) );
   }

   return html;
}

}  // namespace

Frm_Help::Frm_Help( QWidget* aParent ) : QDialog( aParent )
{
   setWindowTitle( QStringLiteral( "Help" ) );

   Red_Help = new QTextBrowser( this );
   Red_Help->setOpenExternalLinks( true );
   Red_Help->setHtml( buildHelpHtml() );

   Chk_ShowTips = new QCheckBox( QStringLiteral( "Don't show again" ), this );

   QDialogButtonBox* buttons = new QDialogButtonBox( QDialogButtonBox::Close, this );
   connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

   QHBoxLayout* bottom = new QHBoxLayout;
   bottom->addWidget( Chk_ShowTips );
   bottom->addStretch();
   bottom->addWidget( buttons );

   QVBoxLayout* layout = new QVBoxLayout( this );
   layout->addWidget( Red_Help );
   layout->addLayout( bottom );

   resize( 720, 560 );
}

bool Frm_Help::Execute( bool aShowTips, bool aAllowOptOut )
{
   Chk_ShowTips->setVisible( aAllowOptOut );
   Chk_ShowTips->setChecked( false );

   exec();

   return aAllowOptOut ? !Chk_ShowTips->isChecked() : aShowTips;
}
