#include "F_About.h"

#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

// Was Rst_Text. Markup rather than plain text because the Delphi got its links
// from EM_AUTOURLDETECT, which has no Qt equivalent - and does not need one,
// since the URLs are fixed and known here.
constexpr const char* Cst_AboutHtml = R"html(
<p>GameList Editor manages the <code>gamelist.xml</code> of a
<a href="https://www.recalbox.com/">Recalbox</a> or
<a href="https://retropie.org.uk/">RetroPie</a> installation.</p>

<p>Written by NeeeeB and andresdelcampo, originally in Delphi.<br>
Original source code:
<a href="https://github.com/andresdelcampo/GameList_Editor">github.com/andresdelcampo/GameList_Editor</a></p>

<p>This is an AI assisted port to C++/Qt6, sourcecode can be found at <a href="https://github.com/MikeBergmann/GameList_Editor_Qt">github.com/MikeBergmann/GameList_Editor_Qt</a></p>

)html";

}  // namespace

Frm_About::Frm_About( QWidget* aParent ) : QDialog( aParent )
{
   setWindowTitle( QStringLiteral( "About..." ) );

   Red_About = new QTextBrowser( this );
   Red_About->setOpenExternalLinks( true );
   Red_About->setHtml( QLatin1String( Cst_AboutHtml ) );

   QDialogButtonBox* buttons = new QDialogButtonBox( QDialogButtonBox::Close, this );
   connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

   QVBoxLayout* layout = new QVBoxLayout( this );
   layout->addWidget( Red_About );
   layout->addWidget( buttons );

   resize( 420, 340 );
}
