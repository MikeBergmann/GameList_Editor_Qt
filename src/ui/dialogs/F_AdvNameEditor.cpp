#include "F_AdvNameEditor.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace {

// Matches the DFM's NumbersOnly. The upper bound is arbitrary but finite; no
// game name is 1000 characters long.
constexpr int Cst_MaxCharsToRemove = 999;

QLineEdit* addCountRow( QVBoxLayout* aLayout, QWidget* aParent, const QString& aSuffix )
{
   QLineEdit* edit = new QLineEdit( aParent );
   edit->setValidator( new QIntValidator( 0, Cst_MaxCharsToRemove, edit ) );
   edit->setMaximumWidth( 48 );

   QHBoxLayout* row = new QHBoxLayout;
   row->addWidget( edit );
   row->addWidget( new QLabel( aSuffix, aParent ) );
   row->addStretch();
   aLayout->addLayout( row );

   return edit;
}

QLineEdit* addTextRow( QVBoxLayout* aLayout, QWidget* aParent, const QString& aSuffix )
{
   QLineEdit* edit = new QLineEdit( aParent );

   QHBoxLayout* row = new QHBoxLayout;
   row->addWidget( edit );
   row->addWidget( new QLabel( aSuffix, aParent ) );
   aLayout->addLayout( row );

   return edit;
}

}  // namespace

Frm_AdvNameEditor::Frm_AdvNameEditor( QWidget* aParent ) : QDialog( aParent )
{
   setWindowTitle( QStringLiteral( "Advanced Name Editor" ) );

   Grp_Delete = new QGroupBox( QStringLiteral( "Delete" ), this );
   Grp_Delete->setCheckable( true );
   Grp_Delete->setChecked( false );
   QVBoxLayout* deleteLayout = new QVBoxLayout( Grp_Delete );
   Edt_NbChars =
      addCountRow( deleteLayout, Grp_Delete, QStringLiteral( "characters at the beginning." ) );
   Edt_NbCharsEnd =
      addCountRow( deleteLayout, Grp_Delete, QStringLiteral( "characters at the end." ) );

   Grp_Add = new QGroupBox( QStringLiteral( "Add" ), this );
   Grp_Add->setCheckable( true );
   Grp_Add->setChecked( false );
   QVBoxLayout* addLayout = new QVBoxLayout( Grp_Add );
   Edt_StartString = addTextRow( addLayout, Grp_Add, QStringLiteral( "at the beginning." ) );
   Edt_EndString = addTextRow( addLayout, Grp_Add, QStringLiteral( "at the end." ) );

   Grp_Case = new QGroupBox( QStringLiteral( "Case" ), this );
   Grp_Case->setCheckable( true );
   Grp_Case->setChecked( false );
   QVBoxLayout* caseLayout = new QVBoxLayout( Grp_Case );

   // Ids are the Delphi's Rdg_Case.ItemIndex, so checkedId() returns -1 for
   // "nothing picked" exactly as ItemIndex did.
   Rdg_Case = new QButtonGroup( this );
   const QString options[] = { QStringLiteral( "Capitalize the first character" ),
                               QStringLiteral( "Convert to Uppercase" ),
                               QStringLiteral( "Convert to Lowercase" ) };
   for ( int i = 0; i < 3; ++i ) {
      QRadioButton* option = new QRadioButton( options[i], Grp_Case );
      Rdg_Case->addButton( option, i );
      caseLayout->addWidget( option );
   }

   Edt_Preview = new QLineEdit( this );
   Edt_Preview->setReadOnly( true );
   Edt_Preview->setAlignment( Qt::AlignCenter );

   QDialogButtonBox* buttons =
      new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
   buttons->button( QDialogButtonBox::Ok )->setText( QStringLiteral( "Apply" ) );
   connect( buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
   connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

   QHBoxLayout* top = new QHBoxLayout;
   top->addWidget( Grp_Delete );
   top->addWidget( Grp_Add );

   QVBoxLayout* layout = new QVBoxLayout( this );
   layout->addLayout( top );
   layout->addWidget( Grp_Case );
   layout->addWidget( new QLabel( QStringLiteral( "Preview" ), this ), 0, Qt::AlignHCenter );
   layout->addWidget( Edt_Preview );
   layout->addStretch();
   layout->addWidget( buttons );

   // Anything that can change the outcome refreshes the preview.
   for ( QGroupBox* group : { Grp_Delete, Grp_Add, Grp_Case } )
      connect( group, &QGroupBox::toggled, this, &Frm_AdvNameEditor::ProcessPreview );

   for ( QLineEdit* edit : { Edt_NbChars, Edt_NbCharsEnd, Edt_StartString, Edt_EndString } )
      connect( edit, &QLineEdit::textChanged, this, &Frm_AdvNameEditor::ProcessPreview );

   connect( Rdg_Case, &QButtonGroup::idToggled, this, &Frm_AdvNameEditor::ProcessPreview );
}

bool Frm_AdvNameEditor::Execute( const QString& aPreview, NameEdit& aEdit )
{
   FPreviewStr = aPreview;
   ProcessPreview();

   if ( exec() != QDialog::Accepted )
      return false;

   aEdit = currentEdit();
   return true;
}

NameEdit Frm_AdvNameEditor::currentEdit() const
{
   NameEdit edit;

   edit.removeChars = Grp_Delete->isChecked();
   // An empty field is 0, which is what the Delphi's explicit '' test produced.
   edit.nbStart = Edt_NbChars->text().toInt();
   edit.nbEnd = Edt_NbCharsEnd->text().toInt();

   edit.changeCase = Grp_Case->isChecked();
   edit.caseIndex = Rdg_Case->checkedId();

   edit.addChars = Grp_Add->isChecked();
   edit.startString = Edt_StartString->text();
   edit.endString = Edt_EndString->text();

   return edit;
}

void Frm_AdvNameEditor::ProcessPreview()
{
   Edt_Preview->setText( applyNameEdit( FPreviewStr, currentEdit() ) );
}
