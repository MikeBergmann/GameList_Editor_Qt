#include "F_MoreInfos.h"

#include "Game.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

namespace {

// The five Lbl_* members of the DFM are gone: QFormLayout owns its labels, and
// nothing outside the form ever referred to them.
QLineEdit* addField( QFormLayout* aLayout, const QString& aLabel )
{
   QLineEdit* edit = new QLineEdit( aLayout->parentWidget() );
   edit->setReadOnly( true );
   aLayout->addRow( aLabel, edit );

   return edit;
}

}  // namespace

Frm_MoreInfos::Frm_MoreInfos( QWidget* aParent ) : QDialog( aParent )
{
   setWindowTitle( QStringLiteral( "More Infos" ) );

   QFormLayout* fields = new QFormLayout;
   Edt_Playcount = addField( fields, QStringLiteral( "Playcount" ) );
   Edt_LastPlayed = addField( fields, QStringLiteral( "Last played on" ) );
   Edt_Crc32 = addField( fields, QStringLiteral( "CRC32" ) );
   Edt_Md5 = addField( fields, QStringLiteral( "MD5" ) );
   Edt_Sha1 = addField( fields, QStringLiteral( "SHA1" ) );

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

void Frm_MoreInfos::Execute( const Game& aGame )
{
   Edt_Playcount->setText( aGame.playcount );

   // An unset or unparseable <lastplayed> shows as blank rather than as an
   // epoch date - plenty of gamelists in the wild hold neither.
   const QDateTime lastPlayed = gamelistDateTime( aGame.lastplayed );
   Edt_LastPlayed->setText( lastPlayed.isValid()
                               ? lastPlayed.toString( QStringLiteral( "dd/MM/yyyy HH:mm:ss" ) )
                               : QString() );

   Edt_Crc32->setText( aGame.crc32 );
   Edt_Md5->setText( aGame.md5 );
   Edt_Sha1->setText( aGame.sha1 );

   exec();
}
