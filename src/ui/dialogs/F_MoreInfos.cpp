#include "F_MoreInfos.h"

#include "Game.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

namespace {

QLineEdit* addField( QFormLayout* aLayout, const QString& aLabel, const char* aObjectName )
{
   QLineEdit* edit = new QLineEdit( aLayout->parentWidget() );
   edit->setObjectName( QLatin1String( aObjectName ) );
   edit->setReadOnly( true );
   aLayout->addRow( aLabel, edit );

   return edit;
}

// A path may not exist: the field then says where the editor looked. The tooltip holds
// the path alone, for when the field is too narrow.
void showFile( QLineEdit* aEdit, const QString& aPath, bool aMissing )
{
   aEdit->setText( aMissing && !aPath.isEmpty() ? aPath + QStringLiteral( " (missing)" ) : aPath );
   aEdit->setToolTip( aPath );
   aEdit->setCursorPosition( 0 );
}

}  // namespace

Frm_MoreInfos::Frm_MoreInfos( QWidget* aParent ) : QDialog( aParent )
{
   setWindowTitle( QStringLiteral( "More Infos" ) );
   setMinimumWidth( 560 );

   QFormLayout* fields = new QFormLayout;
   Edt_Gamelist = addField( fields, QStringLiteral( "Gamelist" ), "Edt_Gamelist" );
   Edt_Rom = addField( fields, QStringLiteral( "ROM" ), "Edt_Rom" );
   Edt_Image = addField( fields, QStringLiteral( "Image" ), "Edt_Image" );
   Edt_Video = addField( fields, QStringLiteral( "Video" ), "Edt_Video" );
   Edt_Playcount = addField( fields, QStringLiteral( "Playcount" ), "Edt_Playcount" );
   Edt_LastPlayed = addField( fields, QStringLiteral( "Last played on" ), "Edt_LastPlayed" );
   Edt_Crc32 = addField( fields, QStringLiteral( "CRC32" ), "Edt_Crc32" );
   Edt_Md5 = addField( fields, QStringLiteral( "MD5" ), "Edt_Md5" );
   Edt_Sha1 = addField( fields, QStringLiteral( "SHA1" ), "Edt_Sha1" );

   // Hashes are fixed-width hex; a monospaced font makes them comparable by eye.
   const QFont mono = QFontDatabase::systemFont( QFontDatabase::FixedFont );
   for ( QLineEdit* hash : { Edt_Crc32, Edt_Md5, Edt_Sha1 } )
      hash->setFont( mono );

   QDialogButtonBox* buttons = new QDialogButtonBox( QDialogButtonBox::Close, this );
   connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

   QVBoxLayout* layout = new QVBoxLayout( this );
   layout->addLayout( fields );
   layout->addWidget( buttons );
}

void Frm_MoreInfos::Execute( const Game& aGame, const QString& aGamelistPath )
{
   showFile( Edt_Gamelist, aGamelistPath, false );
   showFile( Edt_Rom, aGame.physicalRomPath, aGame.isOrphan );
   showFile( Edt_Image, aGame.physicalImagePath, aGame.missingImage );
   showFile( Edt_Video, aGame.physicalVideoPath, aGame.missingVideo );

   Edt_Playcount->setText( aGame.playcount );

   // An unset or unparseable <lastplayed> shows as blank rather than as an
   // epoch date - plenty of gamelists in the wild hold neither.
   Edt_LastPlayed->setText( gamelistDateTimeToDisplay( aGame.lastplayed ) );

   Edt_Crc32->setText( aGame.crc32 );
   Edt_Md5->setText( aGame.md5 );
   Edt_Sha1->setText( aGame.sha1 );

   exec();
}
