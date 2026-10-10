#include "F_LinkMedia.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

Frm_LinkMedia::Frm_LinkMedia( QWidget* aParent ) : QDialog( aParent )
{
   setWindowTitle( QStringLiteral( "Link existing media" ) );
   resize( 560, 420 );

   Lst_Media = new QListWidget( this );
   Lst_Media->setObjectName( QStringLiteral( "Lst_Media" ) );

   QDialogButtonBox* buttons =
      new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
   connect( buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
   connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

   QVBoxLayout* layout = new QVBoxLayout( this );
   layout->addWidget( new QLabel( QStringLiteral( "Pictures and videos found for games that do not "
                                                  "link them. Untick what you do not want." ),
                                  this ) );
   layout->addWidget( Lst_Media, 1 );
   layout->addWidget( buttons );
}

QVector<MediaLink> Frm_LinkMedia::Execute( const Gamelist& aGamelist,
                                           const QVector<MediaLink>& aCandidates )
{
   const QDir systemDir( aGamelist.systemDir() );

   Lst_Media->clear();
   for ( int row = 0; row < aCandidates.size(); ++row ) {
      const MediaLink& link = aCandidates.at( row );
      const QString text = QStringLiteral( "%1: %2 %3 %4" )
                              .arg( aGamelist.at( link.index ).romName(),
                                    link.isImage ? QStringLiteral( "image" ) : QStringLiteral( "video" ),
                                    QString( QChar( 0x2192 ) ),
                                    systemDir.relativeFilePath( link.path ) );
      QListWidgetItem* item = new QListWidgetItem( text, Lst_Media );
      item->setData( Qt::UserRole, row );
      item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
      item->setCheckState( Qt::Checked );
   }

   if ( exec() != QDialog::Accepted )
      return {};

   QVector<MediaLink> chosen;
   for ( int row = 0; row < Lst_Media->count(); ++row ) {
      const QListWidgetItem* item = Lst_Media->item( row );
      if ( item->checkState() == Qt::Checked )
         chosen.append( aCandidates.at( item->data( Qt::UserRole ).toInt() ) );
   }
   return chosen;
}
