#pragma once

#include "NameEdit.h"

#include <QDialog>
#include <QString>

class QButtonGroup;
class QGroupBox;
class QLineEdit;

// Batch rename dialog: trim characters off either end, force the case, and/or
// wrap the name in fixed text, with a live preview of the first selected game.
class Frm_AdvNameEditor : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_AdvNameEditor( QWidget* aParent = nullptr );

   // False if the user cancelled, in which case aEdit is left alone.
   bool Execute( const QString& aPreview, NameEdit& aEdit );

private:
   NameEdit currentEdit() const;
   void ProcessPreview();

   QString FPreviewStr;

   // The Delphi put a bare TCheckBox on top of each group's caption to gate it,
   // then hand-toggled Enabled on every child. A checkable QGroupBox is the
   // same control, so Chk_DeleteChars/Chk_Add/Chk_Case and their three click
   // handlers are gone, as are the four Lbl_* the handlers existed to grey out.
   QGroupBox* Grp_Delete = nullptr;
   QLineEdit* Edt_NbChars = nullptr;
   QLineEdit* Edt_NbCharsEnd = nullptr;

   QGroupBox* Grp_Add = nullptr;
   QLineEdit* Edt_StartString = nullptr;
   QLineEdit* Edt_EndString = nullptr;

   QGroupBox* Grp_Case = nullptr;
   QButtonGroup* Rdg_Case = nullptr;

   QLineEdit* Edt_Preview = nullptr;
};
