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
   "<a href=\"#media\">Pictures and videos</a> | <a href=\"#sync\">Sync to other folder</a> | "
   "<a href=\"#options\">Menu options</a></p>"

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

   "<h3><a name=\"media\">Pictures and videos</a></h3>"
   "<p>How media is found depends on the layout. The title bar says ES-DE for the ES-DE one.</p>"
   "<p><b>Legacy layout</b> (ROM folders with a <code>gamelist.xml</code> each): a game's "
   "<code>&lt;image&gt;</code> and <code>&lt;video&gt;</code> hold a link to a file, usually in "
   "<code>./media/images</code> and <code>./media/videos</code>, or <code>./images</code> and "
   "<code>./videos</code>.</p>"
   "<ul>"
   "<li>A picture you set is saved as PNG; a video is copied as it is. Both are named after the "
   "ROM (<code>Sonic.png</code>) and saved in the folder of the first game that has media, else "
   "in <code>./media/images</code> or <code>./media/videos</code>.</li>"
   "<li><b>Link existing media</b> finds files for games without a link, by the ROM's name.</li>"
   "<li><b>Rename media after ROMs</b> renames linked files that do not carry their ROM's name.</li>"
   "<li>A file that another game also uses is never deleted or renamed.</li>"
   "</ul>"
   "<p><b>Dash names.</b> Some scrapers name files after the ROM plus <code>-image</code> or "
   "<code>-video</code>: <code>Sonic-image.png</code>, <code>Sonic-video.mp4</code>.</p>"
   "<ul>"
   "<li>Link existing media accepts both <code>Sonic.png</code> and <code>Sonic-image.png</code>, "
   "and prefers the plain name when both exist.</li>"
   "<li>Rename media leaves dash names alone. Tick <i>Also rename files ending in -image or "
   "-video</i> to turn them into plain names.</li>"
   "<li>New media follows the style of the first game that has media: if its file is "
   "<code>Sonic-image.png</code>, new files get <code>-image</code> and <code>-video</code> too, "
   "and a re-scrape replaces a dash file in place. Games that mix both styles follow that first "
   "one.</li>"
   "<li>A ROM that really is called <code>Foo-image.zip</code> is not mistaken for a dash "
   "name.</li>"
   "</ul>"
   "<p><b>ES-DE layout:</b> <code>&lt;image&gt;</code> and <code>&lt;video&gt;</code> are not "
   "used. Media is found by the ROM's name (without extension, with its subfolder) at "
   "<code>&lt;media folder&gt;/&lt;type&gt;/&lt;ROM path&gt;.&lt;ext&gt;</code>. This editor "
   "writes <code>covers</code> and <code>videos</code>.</p>"
   "<ul>"
   "<li>Dash names are not used there: <code>Sonic-image.png</code> counts as missing.</li>"
   "<li>ES-DE reads only <code>jpg</code>, <code>png</code> and <code>webp</code> pictures, "
   "in lowercase, and <code>mp4 mkv avi wmv mov webm m4v</code> videos.</li>"
   "<li>Linking and renaming are not offered, and changing media does not save the "
   "gamelist.</li>"
   "<li>Deleting a game also removes its media of every type, unless another game shares "
   "it.</li>"
   "</ul>"
   "<p>If saving fails, media files and gamelist go back as they were. Deleted media goes to "
   "the trash where there is one.</p>"

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
