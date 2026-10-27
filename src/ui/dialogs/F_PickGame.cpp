#include "F_PickGame.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

Frm_PickGame::Frm_PickGame( const LaunchBoxDb& aDb, QWidget* aParent )
   : QDialog( aParent ), FDb( aDb )
{
   setWindowTitle( QStringLiteral( "Pick the LaunchBox game" ) );
   resize( 520, 400 );

   Lbl_Hint = new QLabel( this );
   Lbl_Hint->setWordWrap( true );

   Edt_Search = new QLineEdit( this );
   Edt_Search->setObjectName( QStringLiteral( "Edt_Search" ) );
   Edt_Search->setClearButtonEnabled( true );

   Lst_Candidates = new QListWidget( this );
   Lst_Candidates->setObjectName( QStringLiteral( "Lst_Candidates" ) );

   Box_Buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );

   QVBoxLayout* layout = new QVBoxLayout( this );
   layout->addWidget( Lbl_Hint );
   layout->addWidget( Edt_Search );
   layout->addWidget( Lst_Candidates, 1 );
   layout->addWidget( Box_Buttons );

   connect( Edt_Search, &QLineEdit::textChanged, this, &Frm_PickGame::search );
   connect( Lst_Candidates, &QListWidget::currentRowChanged, this, [this]( int aRow ) {
      Box_Buttons->button( QDialogButtonBox::Ok )->setEnabled( aRow >= 0 );
   } );
   connect( Lst_Candidates, &QListWidget::itemDoubleClicked, this, &QDialog::accept );
   connect( Box_Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
   connect( Box_Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
}

void Frm_PickGame::search()
{
   Lst_Candidates->clear();
   for ( const LaunchBoxCandidate& candidate : FDb.similar( Edt_Search->text() ) ) {
      QString text = candidate.name;
      if ( !candidate.year.isEmpty() )
         text += QStringLiteral( " (%1)" ).arg( candidate.year );
      if ( !candidate.altName.isEmpty() )
         text += QStringLiteral( " \u2013 also known as %1" ).arg( candidate.altName );
      QListWidgetItem* item = new QListWidgetItem( text, Lst_Candidates );
      item->setData( Qt::UserRole, candidate.index );
   }
   if ( Lst_Candidates->count() > 0 )
      Lst_Candidates->setCurrentRow( 0 );
   else
      Box_Buttons->button( QDialogButtonBox::Ok )->setEnabled( false );
}

int Frm_PickGame::Execute( const QString& aSearch )
{
   Lbl_Hint->setText( QStringLiteral( "No LaunchBox game has exactly this name. Pick the closest "
                                      "one, or change the search." ) );
   Edt_Search->setText( aSearch );
   search();
   Edt_Search->selectAll();
   Edt_Search->setFocus();

   if ( exec() != QDialog::Accepted || !Lst_Candidates->currentItem() )
      return -1;
   return Lst_Candidates->currentItem()->data( Qt::UserRole ).toInt();
}
