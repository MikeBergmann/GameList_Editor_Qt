#include "F_AddRoms.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

Frm_AddRoms::Frm_AddRoms( QWidget* aParent ) : QDialog( aParent )
{
   setWindowTitle( QStringLiteral( "Add missing ROMs" ) );
   resize( 480, 420 );

   Lst_Roms = new QListWidget( this );
   Lst_Roms->setObjectName( QStringLiteral( "Lst_Roms" ) );

   QDialogButtonBox* buttons =
      new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
   QPushButton* browse = buttons->addButton( QStringLiteral( "Browse..." ), QDialogButtonBox::ActionRole );
   connect( browse, &QPushButton::clicked, this, &Frm_AddRoms::browse );
   connect( buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
   connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

   QVBoxLayout* layout = new QVBoxLayout( this );
   layout->addWidget( new QLabel( QStringLiteral( "Files in the system folder that are not in "
                                                  "the gamelist. Untick what you do not want." ),
                                  this ) );
   layout->addWidget( Lst_Roms, 1 );
   layout->addWidget( buttons );
}

QStringList Frm_AddRoms::Execute( const QString& aSystemDir, const QStringList& aCandidates )
{
   FSystemDir = aSystemDir;
   Lst_Roms->clear();
   for ( const QString& path : aCandidates )
      addRom( path );

   if ( exec() != QDialog::Accepted )
      return {};

   QStringList chosen;
   for ( int row = 0; row < Lst_Roms->count(); ++row ) {
      const QListWidgetItem* item = Lst_Roms->item( row );
      if ( item->checkState() == Qt::Checked )
         chosen << item->data( Qt::UserRole ).toString();
   }
   return chosen;
}

void Frm_AddRoms::addRom( const QString& aPath )
{
   const QString relative = QDir( FSystemDir ).relativeFilePath( aPath );
   if ( !Lst_Roms->findItems( relative, Qt::MatchExactly ).isEmpty() )
      return;

   QListWidgetItem* item = new QListWidgetItem( relative, Lst_Roms );
   item->setData( Qt::UserRole, aPath );
   item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
   item->setCheckState( Qt::Checked );
}

void Frm_AddRoms::browse()
{
   const QStringList files =
      QFileDialog::getOpenFileNames( this, QStringLiteral( "Choose ROMs" ), FSystemDir );

   for ( const QString& file : files ) {
      if ( QDir( FSystemDir ).relativeFilePath( file ).startsWith( QLatin1String( ".." ) ) ) {
         QMessageBox::warning( this, QStringLiteral( "GameList Editor" ),
                               QStringLiteral( "%1 is not inside the system folder." ).arg( file ) );
         continue;
      }
      addRom( file );
   }
}
