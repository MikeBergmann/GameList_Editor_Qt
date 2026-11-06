#include "F_Help.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

// Shown at startup: short enough to read without scrolling.
const char* const Cst_TipsHtml =
   "<h3>Welcome</h3>"
   "<ul>"
   "<li>Choose your folder with <b>File, Choose folder...</b>: the one holding your system "
   "folders, or the ES-DE folder.</li>"
   "<li><b>Stop EmulationStation first.</b> It rewrites <code>gamelist.xml</code> when it exits "
   "and would overwrite anything saved here.</li>"
   "<li>Select a game, edit it, click <b>Save Changes</b>. Select several games to edit them "
   "together: only the fields you fill in are written.</li>"
   "<li>Hover over a menu option to see what it does. <b>Help, Help</b> has the details.</li>"
   "</ul>";

const char* const Cst_HelpHtml =
   "<p><a href=\"#folder\">Choosing a folder</a> | <a href=\"#editing\">Editing games</a> | "
   "<a href=\"#sync\">Sync to other folder</a> | <a href=\"#options\">Menu options</a></p>"

   "<h3><a name=\"folder\">Choosing a folder</a></h3>"
   "<p>Select the folder where your system folders are stored, or the ES-DE folder (the one "
   "with <code>gamelists</code> and <code>settings</code>). The title bar then reads ES-DE: "
   "pictures and videos are found by the ROM's name in ES-DE's media folder, and linking or "
   "renaming them is not offered.</p>"
   "<p><b>If the folder lives on a running Recalbox, RetroPie or ES-DE, stop EmulationStation "
   "first.</b> It rewrites <code>gamelist.xml</code> when it exits and would overwrite anything "
   "saved from here.</p>"

   "<h3><a name=\"editing\">Editing games</a></h3>"
   "<ul>"
   "<li>Select a game, edit its fields, click <b>Save Changes</b>.</li>"
   "<li>Select several games to edit them together. The fields start blank, and only the ones "
   "you fill in are written to every selected game.</li>"
   "<li>The name and the pictures can only be changed one game at a time.</li>"
   "</ul>"

   "<h3><a name=\"sync\">Sync to other folder</a></h3>"
   "<p><b>Actions, Sync to other folder</b> copies the open folder (the master) into another "
   "one, ES-DE or not, in either direction. Systems pair by folder name, else by type. You "
   "review what changes first.</p>"
   "<ul>"
   "<li><b>Copied:</b> the ROMs (when the target's ROM folder is another folder), name, genre, "
   "rating, players, developer, publisher, release date, description, region, cover and "
   "video.</li>"
   "<li><b>Kept per device:</b> favorites, hidden, kid game, play count and last played. Games "
   "only on the target are listed and kept, never removed.</li>"
   "<li><b>Listed as problems:</b> absolute ROM paths (games match by ROM path), folder ROMs "
   "(ScummVM, PS3), and videos ES-DE cannot play.</li>"
   "<li><b>Not synced:</b> screenshots and other media types. ES-DE reads only jpg, png and "
   "webp covers, so others become PNG.</li>"
   "<li>Stop EmulationStation on the target first. A phone over MTP is not a folder on "
   "Windows: use a card reader or a share.</li>"
   "</ul>"

   "<h3><a name=\"options\">Menu options</a></h3>"
   "<p>Hover over an option in the Options or System menu to see what it does.</p>";

}  // namespace

Frm_Help::Frm_Help( QWidget* aParent ) : QDialog( aParent )
{
   setWindowTitle( QStringLiteral( "Help" ) );

   Red_Help = new QTextBrowser( this );

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
}

bool Frm_Help::Execute( bool aShowTips, bool aAllowOptOut )
{
   Red_Help->setHtml( QLatin1String( aAllowOptOut ? Cst_TipsHtml : Cst_HelpHtml ) );
   resize( aAllowOptOut ? QSize( 520, 300 ) : QSize( 720, 560 ) );

   Chk_ShowTips->setVisible( aAllowOptOut );
   Chk_ShowTips->setChecked( false );

   exec();

   return aAllowOptOut ? !Chk_ShowTips->isChecked() : aShowTips;
}
