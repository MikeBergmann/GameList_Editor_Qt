#include "F_LinkMedia.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

namespace {

QString kindName( bool aIsImage )
{
   return aIsImage ? QStringLiteral( "image" ) : QStringLiteral( "video" );
}

}  // namespace

Frm_LinkMedia::Frm_LinkMedia( QWidget* aParent ) : QDialog( aParent )
{
   resize( 560, 420 );

   Lbl_Intro = new QLabel( this );
   Lbl_Intro->setWordWrap( true );

   Lst_Media = new QListWidget( this );
   Lst_Media->setObjectName( QStringLiteral( "Lst_Media" ) );

   QDialogButtonBox* buttons =
      new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
   connect( buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
   connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

   QVBoxLayout* layout = new QVBoxLayout( this );
   layout->addWidget( Lbl_Intro );
   layout->addWidget( Lst_Media, 1 );
   layout->addWidget( buttons );
}

QVector<int> Frm_LinkMedia::review( const QString& aTitle, const QString& aIntro,
                                    const QVector<Row>& aRows )
{
   setWindowTitle( aTitle );
   Lbl_Intro->setText( aIntro );

   Lst_Media->clear();
   for ( int row = 0; row < aRows.size(); ++row ) {
      QListWidgetItem* item = new QListWidgetItem( aRows.at( row ).text, Lst_Media );
      item->setData( Qt::UserRole, row );
      if ( aRows.at( row ).enabled ) {
         item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
         item->setCheckState( Qt::Checked );
      } else {
         item->setFlags( item->flags() & ~( Qt::ItemIsEnabled | Qt::ItemIsSelectable ) );
         item->setCheckState( Qt::Unchecked );
      }
   }

   if ( exec() != QDialog::Accepted )
      return {};

   QVector<int> chosen;
   for ( int row = 0; row < Lst_Media->count(); ++row ) {
      const QListWidgetItem* item = Lst_Media->item( row );
      if ( item->checkState() == Qt::Checked )
         chosen.append( item->data( Qt::UserRole ).toInt() );
   }
   return chosen;
}

QVector<MediaLink> Frm_LinkMedia::Execute( const Gamelist& aGamelist,
                                           const QVector<MediaLink>& aCandidates )
{
   const QDir systemDir( aGamelist.systemDir() );

   QVector<Row> rows;
   for ( const MediaLink& link : aCandidates ) {
      rows.append( { QStringLiteral( "%1: %2 %3 %4" )
                        .arg( aGamelist.at( link.index ).romName(), kindName( link.isImage ),
                              QString( QChar( 0x2192 ) ), systemDir.relativeFilePath( link.path ) ) } );
   }

   QVector<MediaLink> chosen;
   for ( const int row : review( QStringLiteral( "Link existing media" ),
                                 QStringLiteral( "Pictures and videos found for games that do not "
                                                 "link them. Untick what you do not want." ),
                                 rows ) )
      chosen.append( aCandidates.at( row ) );
   return chosen;
}

QVector<MediaRename> Frm_LinkMedia::ExecuteRename( const Gamelist& aGamelist,
                                                   const QVector<MediaRename>& aCandidates )
{
   const QDir systemDir( aGamelist.systemDir() );

   QVector<Row> rows;
   for ( const MediaRename& rename : aCandidates ) {
      QString text = QStringLiteral( "%1: %2 %3 %4 %5" )
                        .arg( aGamelist.at( rename.index ).romName(), kindName( rename.isImage ),
                              systemDir.relativeFilePath( rename.path ), QString( QChar( 0x2192 ) ),
                              QFileInfo( rename.newPath ).fileName() );
      if ( !rename.problem.isEmpty() )
         text += QStringLiteral( " (not renamed: %1)" ).arg( rename.problem );
      rows.append( { text, rename.problem.isEmpty() } );
   }

   QVector<MediaRename> chosen;
   for ( const int row : review( QStringLiteral( "Rename media after their ROM" ),
                                 QStringLiteral( "Pictures and videos whose file name is not their "
                                                 "ROM's. Untick what you do not want renamed." ),
                                 rows ) )
      chosen.append( aCandidates.at( row ) );
   return chosen;
}
