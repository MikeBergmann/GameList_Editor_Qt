#include "F_PickGame.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

Frm_PickGame::Frm_PickGame( QWidget* aParent ) : QDialog( aParent )
{
   setWindowTitle( QStringLiteral( "Pick the game" ) );
   resize( 520, 400 );

   Lbl_Hint = new QLabel( this );
   Lbl_Hint->setWordWrap( true );

   Edt_Search = new QLineEdit( this );
   Edt_Search->setObjectName( QStringLiteral( "Edt_Search" ) );
   Edt_Search->setClearButtonEnabled( true );

   Btn_Search = new QPushButton( QStringLiteral( "Search" ), this );
   Btn_Search->setAutoDefault( false );
   Btn_Search->hide();

   Lst_Candidates = new QListWidget( this );
   Lst_Candidates->setObjectName( QStringLiteral( "Lst_Candidates" ) );

   Box_Buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );

   QHBoxLayout* searchRow = new QHBoxLayout;
   searchRow->addWidget( Edt_Search, 1 );
   searchRow->addWidget( Btn_Search );

   QVBoxLayout* layout = new QVBoxLayout( this );
   layout->addWidget( Lbl_Hint );
   layout->addLayout( searchRow );
   layout->addWidget( Lst_Candidates, 1 );
   layout->addWidget( Box_Buttons );

   connect( Edt_Search, &QLineEdit::textChanged, this, [this]( const QString& aText ) {
      if ( FLive )
         emit searchRequested( aText );
   } );
   connect( Btn_Search, &QPushButton::clicked, this,
            [this] { emit searchRequested( Edt_Search->text() ); } );
   connect( Lst_Candidates, &QListWidget::currentRowChanged, this, [this]( int aRow ) {
      Box_Buttons->button( QDialogButtonBox::Ok )->setEnabled( aRow >= 0 );
   } );
   connect( Lst_Candidates, &QListWidget::itemDoubleClicked, this, &QDialog::accept );
   connect( Box_Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
   connect( Box_Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
}

void Frm_PickGame::setHint( const QString& aText )
{
   Lbl_Hint->setText( aText );
}

void Frm_PickGame::setLiveSearch( bool aLive )
{
   FLive = aLive;
   Btn_Search->setVisible( !aLive );
}

void Frm_PickGame::setRows( const QStringList& aRows )
{
   Lst_Candidates->clear();
   Lst_Candidates->addItems( aRows );
   if ( Lst_Candidates->count() > 0 )
      Lst_Candidates->setCurrentRow( 0 );
   else
      Box_Buttons->button( QDialogButtonBox::Ok )->setEnabled( false );
}

void Frm_PickGame::keyPressEvent( QKeyEvent* aEvent )
{
   // Enter would otherwise hit OK, picking a stale list before the new search ran.
   if ( !FLive && Edt_Search->hasFocus() &&
        ( aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter ) ) {
      emit searchRequested( Edt_Search->text() );
      return;
   }
   QDialog::keyPressEvent( aEvent );
}

int Frm_PickGame::Execute( const QString& aSearch )
{
   {
      const QSignalBlocker blocker( Edt_Search );
      Edt_Search->setText( aSearch );
   }
   Edt_Search->selectAll();
   Edt_Search->setFocus();

   return exec() == QDialog::Accepted ? Lst_Candidates->currentRow() : -1;
}
